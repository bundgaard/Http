#include <Http/Http.h>

// clang-format off
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <http.h>
// clang-format on

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <stdexcept>
#include <string>

#define CHECK_WIN32_RC(fn_to_rc)                                                                  \
    if (fn_to_rc != NO_ERROR)                                                                     \
    {                                                                                             \
        throw std::runtime_error(std::format(#fn_to_rc ", failed with code: {:#08x}", fn_to_rc)); \
    }

namespace http_server::detail
{
    std::wstring Widen(std::string_view str)
    {
        auto len = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
        std::wstring wstr(len, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), wstr.data(), len);
        return wstr;
    }

    static std::string Narrow(std::wstring_view str)
    {
        auto len = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0, nullptr, nullptr);
        std::string nstr(len, 0);
        WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nstr.data(), len, nullptr, nullptr);
        // Truncate
        if (auto found_pos = nstr.find('\0'); found_pos != std::string::npos)
        {
            nstr.resize(std::distance(nstr.begin(), nstr.begin() + found_pos));
        }
        return nstr;
    }
}

namespace http_server
{
    struct URL
    {
        std::string Host;
        std::string Path;
        std::string Query;
    };
    static std::string Basename(std::string_view path)
    {
        auto pos = path.find_last_of('/');
        if (pos == std::string::npos)
        {
            return std::string(path);
        }
        return std::string(path.substr(pos + 1));
    }

    static std::string Dirname(std::string_view path)
    {
        auto pos = path.find_last_of('/');
        if (pos == std::string::npos)
        {
            return std::string(path);
        }

        return std::string(path.substr(0, pos));
    }

    static URL Parse(HTTP_REQUEST *request)
    {
        std::wstring_view full_url{
            request->CookedUrl.pFullUrl,
            request->CookedUrl.FullUrlLength};
        std::wstring_view query_string{
            request->CookedUrl.pQueryString,
            request->CookedUrl.QueryStringLength};
        std::wstring_view abs_path{request->CookedUrl.pAbsPath, request->CookedUrl.AbsPathLength};
        std::wstring_view host{request->CookedUrl.pHost, request->CookedUrl.HostLength};
        URL url{};
        url.Host = detail::Narrow(host);
        url.Path = detail::Narrow(abs_path);
        url.Query = detail::Narrow(query_string);
        return url;
    }

    struct InternalRequest
    {
        std::vector<uint8_t> RequestData;
    };

    class SimpleResponseWriter : public ResponseWriter
    {
    public:
        SimpleResponseWriter() = default;
        ~SimpleResponseWriter() override = default;

        void WriteHeader(StatusCode code) override
        {
            m_status_code = code;
        };

        void Write(std::string_view data) override
        {
            // This can be turned into calling a callback in the server, so we actually can stream data whenever we write to this.
            m_body += data;
        };

        Header &GetHeader() override
        {
            return m_header;
        };

        StatusCode GetStatusCode() const
        {
            return m_status_code;
        }

        const Header &GetHeader() const
        {
            return m_header;
        }

        const std::string &GetBody() const
        {
            return m_body;
        }

    private:
        StatusCode m_status_code{};
        mutable Header m_header{};
        std::string m_body;
    };

    struct Response
    {
        HTTP_REQUEST_ID Id;
        std::unique_ptr<SimpleResponseWriter> writer;
    };

    enum class HttpState
    {
        WaitingForRequest,
        ProcessRequest,
        SendResponse,
        Shutdown,
        UnknownEndpoint,
    };

    class ConcreteRequest : public Request
    {
    public:
        ConcreteRequest(const std::string &method, const std::string &url, const Header &header)
            : m_method(method), m_url(url), m_header(header)
        {
        }
        ~ConcreteRequest() = default;

        const std::string &GetMethod() const override
        {
            return m_method;
        }
        const std::string &GetURL() const override
        {
            return m_url;
        }
        const Header &GetHeader() const override
        {
            return m_header;
        }
        const std::vector<uint8_t> &GetBody() const override
        {

            return m_body;
        }

        void SetBody(const std::vector<uint8_t> &body)
        {
            m_body = body;
        }

    private:
        std::string m_method{};
        std::string m_url{};
        Header m_header{};
        std::vector<uint8_t> m_body{};
    };

    class CompletionPort
    {
    public:
        CompletionPort(HANDLE handle)
            : m_iocp(CreateIoCompletionPort(handle, nullptr, 0, 1))
        {
            if (m_iocp == nullptr)
            {
                throw std::runtime_error("Failed to create IOCP");
            }

            if (!SetFileCompletionNotificationModes(handle, FILE_SKIP_COMPLETION_PORT_ON_SUCCESS | FILE_SKIP_SET_EVENT_ON_HANDLE))
            {
                throw std::runtime_error("Failed to set file completion notification modes");
            }
        }

        [[nodiscard]] bool Wait() const
        {
            DWORD bytes_transferred{};
            ULONG_PTR completion_value{};
            OVERLAPPED *overlapped = nullptr;
            return GetQueuedCompletionStatus(m_iocp, &bytes_transferred, &completion_value, &overlapped, 500);
        }

        ~CompletionPort()
        {
            if (m_iocp != nullptr)
            {
                CloseHandle(m_iocp);
            }
        }

    private:
        HANDLE m_iocp = nullptr;
    };
    class Http::Pimpl
    {
    public:
        Pimpl(std::string_view host) : m_host(host)
        {
            CHECK_WIN32_RC(HttpInitialize(HTTPAPI_VERSION_2, HTTP_INITIALIZE_SERVER, nullptr));
            CHECK_WIN32_RC(HttpCreateRequestQueue(HTTPAPI_VERSION_2, L"http::queue", nullptr, 0, &request_queue_handle));
            CHECK_WIN32_RC(HttpCreateServerSession(HTTPAPI_VERSION_2, &session_id, 0));
            CHECK_WIN32_RC(HttpCreateUrlGroup(session_id, &url_group_id, 0));
            Configure();
            m_iocp = std::make_unique<CompletionPort>(request_queue_handle);
        }

        void Configure() const
        {
            HTTP_BINDING_INFO binding{};
            binding.Flags.Present = 1;
            binding.RequestQueueHandle = request_queue_handle;
            CHECK_WIN32_RC(HttpSetUrlGroupProperty(url_group_id, HttpServerBindingProperty, &binding, sizeof(binding)));
        }

        ~Pimpl()
        {
            HttpCloseUrlGroup(url_group_id);
            HttpCloseServerSession(session_id);
            HttpCloseRequestQueue(request_queue_handle);
            HttpTerminate(HTTP_INITIALIZE_SERVER, nullptr);
        }

        void AddHandler(std::string_view pattern, HandlerFunc func)
        {

            CHECK_WIN32_RC(HttpAddUrlToUrlGroup(
                url_group_id,
                detail::Widen(std::string(m_host) + std::string(pattern)).c_str(),
                0,
                0));

            std::cerr << "Added handler for " << pattern << std::endl;
            m_handlers[std::string(pattern)] = func;
        }

        void UnknownEndpoint()
        {
            auto &request = m_requests.back();
            HTTP_REQUEST *http_request = reinterpret_cast<HTTP_REQUEST *>(request.RequestData.data());

            HTTP_RESPONSE response{};
            response.StatusCode = 404;
            response.pReason = "Not Found";
            response.ReasonLength = sizeof("Not Found") - 1;
            OVERLAPPED overlapped{};
            ULONG bytes_sent{};
            uint32_t rc = HttpSendHttpResponse(
                request_queue_handle,
                http_request->RequestId,
                0,
                &response,
                nullptr,
                nullptr,
                nullptr,
                0,
                &overlapped,
                nullptr);
            switch (rc)
            {
            case NOERROR:
            {
                std::cerr << "Unknown endpoint" << std::endl;

                m_requests.pop_back(); // Remove the request from the queue, since we are not going to process it.
                m_state = HttpState::WaitingForRequest;
            }
                return;
            case ERROR_IO_PENDING:
                if (m_iocp->Wait())
                {
                    m_requests.pop_back(); // Remove the request from the queue, since we are not going to process it.
                    m_state = HttpState::WaitingForRequest;
                }
                break;
            default:
                throw std::runtime_error(std::format("Failed to send response {:#08x}", rc));
            }
        }

        void Run()
        {
            while (m_state != HttpState::Shutdown)
            {
                switch (m_state)
                {
                case HttpState::WaitingForRequest:
                    WaitingForRequest();
                    break;
                case HttpState::ProcessRequest:
                    ProcessRequest();
                    break;
                case HttpState::SendResponse:
                    SendResponse();
                    break;
                case HttpState::UnknownEndpoint:
                    UnknownEndpoint();
                    break;
                default:
                    break;
                }
            };
        }

        void Stop()
        {
            m_state = HttpState::Shutdown;
        }

    private:
        void WaitingForRequest()
        {
            HTTP_REQUEST_ID request_id = HTTP_NULL_ID;
            std::vector<uint8_t> http_request(sizeof(HTTP_REQUEST) + (1 << 14));
            OVERLAPPED overlapped{};
            ULONG bytes_received{};
            uint32_t rc = HttpReceiveHttpRequest(
                request_queue_handle,
                request_id,
                0, // Only fetch the headers, the body (if post) will be fetched later, using the same request ID.
                reinterpret_cast<HTTP_REQUEST *>(http_request.data()), http_request.size(),
                &bytes_received,
                &overlapped);
            switch (rc)
            {
            case NO_ERROR:
                m_requests.emplace_back(InternalRequest{std::move(http_request)});
                m_state = HttpState::ProcessRequest;
                break;
            case ERROR_MORE_DATA:
                // Allocate a bigger buffer and retry again.
                break;
            case ERROR_INVALID_PARAMETER:
                break;
            case ERROR_NOACCESS:
                break;
            case ERROR_HANDLE_EOF:
                break;
            case ERROR_IO_PENDING:
                // The request is not ready yet, we will get it later.
                if (m_iocp->Wait())
                {
                    m_requests.emplace_back(InternalRequest{std::move(http_request)});
                    m_state = HttpState::ProcessRequest;
                }
                break;
            default:
                throw std::runtime_error(std::format("HttpReceiveHttpRequest failed {:#08x}", rc));
            }
            /**
             * ADDING THE REMARK AND RETURN VALUE TO FIX THE DOCUMENTATION LATER.
             *
             * If the function succeeds, the return value is NO_ERROR.
             * If the function is being used asynchronously, a return value of ERROR_IO_PENDING indicates that the next request is not yet ready and will be retrieved later through normal overlapped I/O completion mechanisms.
             * If the function fails, the return value is one of the following error codes.
             *
             * Value	Meaning
             * ERROR_INVALID_PARAMETER
             * One or more of the supplied parameters is in an unusable form.
             * ERROR_NOACCESS
             * One or more of the supplied parameters points to an invalid or unaligned memory buffer. The pRequestBuffer parameter must point to a valid memory buffer with a memory alignment equal or greater to the memory alignment requirement for an HTTP_REQUEST structure.
             * ERROR_MORE_DATA
             * The value of RequestBufferLength is greater than or equal to the size of the request header received, but is not as large as the combined size of the request structure and entity body. The buffer size required to read the remaining part of the entity body is returned in the pBytesReceived parameter if this is non-NULL and if the call is synchronous. Call the function again with a large enough buffer to retrieve all data.
             * ERROR_HANDLE_EOF
             * The specified request has already been completely retrieved; in this case, the value pointed to by pBytesReceived is not meaningful, and pRequestBuffer should not be examined.
             * Other
             * A system error code defined in WinError.h.
             */
        } // WaitingForRequest
        std::string GetMethod(HTTP_REQUEST *request)
        {
            switch (request->Verb)
            {
            case HttpVerbGET:
                return "GET";
            case HttpVerbPOST:
                return "POST";
            case HttpVerbPUT:
                return "PUT";
            case HttpVerbDELETE:
                return "DELETE";
            case HttpVerbHEAD:
                return "HEAD";
            case HttpVerbOPTIONS:
                return "OPTIONS";
            case HttpVerbTRACE:
                return "TRACE";
            case HttpVerbCONNECT:
                return "CONNECT";
            case HttpVerbMOVE:
                return "MOVE";
            case HttpVerbCOPY:
                return "COPY";
            default:
                return "UNKNOWN";
            }
        }

        std::vector<uint8_t> ProcessRequestBody(HTTP_REQUEST &request)
        {
            std::vector<uint8_t> buffer(0x800, 0);
            ULONG bytes_received{};
            OVERLAPPED overlapped{};
            ULONG rc = HttpReceiveRequestEntityBody(request_queue_handle, request.RequestId, 0, buffer.data(), buffer.size(), &bytes_received, &overlapped);
            switch (rc)
            {
            case NOERROR:
                buffer.resize(bytes_received);
                return buffer;
            case ERROR_IO_PENDING:
                if (m_iocp->Wait())
                {
                    buffer.resize(bytes_received);
                    return buffer;
                }
                break;
            case ERROR_INVALID_PARAMETER:
                std::cerr << "Invalid parameter" << std::endl;
                break;
            case ERROR_HANDLE_EOF:
                std::cerr << "Data already processed, ignore result" << std::endl;
                break;
            case ERROR_DLL_INIT_FAILED:
                std::cerr << "DLL init failed" << std::endl;
                break;
            default:
                std::cerr << "Failed to handle request body " << std::to_string(rc) << std::endl;
                break;
            }

            return {};
        }

        void ProcessRequest()
        {
            auto &request_bytes = m_requests.back();

            HTTP_REQUEST *request = reinterpret_cast<HTTP_REQUEST *>(request_bytes.RequestData.data());
            std::cout << "Request ID: " << request->RequestId << std::endl;
            std::cout << "Method: " << GetMethod(request) << std::endl;

            auto cooked_url = detail::Narrow(
                std::wstring_view{
                    request->CookedUrl.pFullUrl,
                    request->CookedUrl.FullUrlLength});
            URL parsed_url = Parse(request);
            
            auto path = Dirname(parsed_url.Path);
            
            auto handler = m_handlers.find(path + "/");
            while (handler == m_handlers.end() && !path.empty())
            {
                path = Dirname(path);
                handler = m_handlers.find(path + "/");
            }

            if (handler != m_handlers.end())
            {
                std::unique_ptr<SimpleResponseWriter> response_writer = std::make_unique<SimpleResponseWriter>();
                std::unique_ptr<ConcreteRequest> current_request = std::make_unique<ConcreteRequest>(GetMethod(request), parsed_url.Path, Header{});

                auto body = ProcessRequestBody(*request);
                current_request->SetBody(body);
                handler->second(*response_writer.get(), *current_request.get());

                m_responses.emplace_back(Response{
                    request->RequestId,
                    std::move(response_writer)});

                m_requests.pop_back();
                m_state = HttpState::SendResponse;
            }
            else
            {
                m_state = HttpState::UnknownEndpoint;
            }
        } // ProcessRequest

        void SendResponse()
        {
            auto &response = m_responses.back();

            constexpr std::string_view STATUS_OK = "OK";

            HTTP_RESPONSE current_response{};
            auto writer = response.writer.get();
            current_response.StatusCode = static_cast<USHORT>(writer->GetStatusCode());
            current_response.pReason = STATUS_OK.data();
            current_response.ReasonLength = STATUS_OK.length();

            current_response.Headers.KnownHeaders[HttpHeaderContentType].pRawValue = "text/plain";
            current_response.Headers.KnownHeaders[HttpHeaderContentType].RawValueLength = sizeof("text/plain") - 1;

            auto &body = writer->GetBody();
            HTTP_DATA_CHUNK chunk{};
            chunk.DataChunkType = HttpDataChunkFromMemory;
            chunk.FromMemory.pBuffer = const_cast<void *>(reinterpret_cast<const void *>(body.data()));
            chunk.FromMemory.BufferLength = body.size();

            current_response.EntityChunkCount = 1;
            current_response.pEntityChunks = &chunk;

            ULONG bytes_sent{};
            uint32_t rc = HttpSendHttpResponse(
                request_queue_handle,
                response.Id,
                0,
                &current_response,
                nullptr,
                nullptr,
                nullptr,
                0,
                nullptr,
                nullptr);
            switch (rc)
            {
            case NO_ERROR:

                m_state = HttpState::WaitingForRequest;
                break;
            default:
                throw std::runtime_error(std::format("Failed to send response {:#08x}", rc));
            }
        } // SendResponse

    private:
        std::string_view m_host;
        HANDLE request_queue_handle;
        HTTP_SERVER_SESSION_ID session_id;
        HTTP_URL_GROUP_ID url_group_id;

        std::map<std::string, HandlerFunc> m_handlers;
        HttpState m_state{HttpState::WaitingForRequest};
        std::vector<InternalRequest> m_requests;
        std::vector<Response> m_responses;
        std::unique_ptr<CompletionPort> m_iocp;
    };

    Http::Http(std::string_view host)
        : m_pimpl(std::make_unique<Pimpl>(host))
    {
    }

    Http::~Http()
    {
#ifdef DB_DEBUG
        std::cerr << "Http::~Http()" << std::endl;
#endif // DB_DEBUG
    }

    void Http::HandleFunc(std::string_view pattern, HandlerFunc func)
    {
        m_pimpl->AddHandler(pattern, func);
    }

    void Http::Run()
    {
        m_pimpl->Run();
    }

    void Http::Stop()
    {
        m_pimpl->Stop();
    }
} // namespace http_server