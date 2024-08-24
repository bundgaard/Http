#pragma once
#include <string_view>
#include <memory>
#include <functional>
#include <vector>
#include <string>
#include <map>

namespace http_server
{
    enum class StatusCode
    {
        Ok = 200,
        BadRequest = 400,
        NotFound = 404,
        InternalServerError = 500
    };

    using Header = std::map<std::string, std::vector<std::string>>;

    struct ResponseWriter
    {
        virtual void WriteHeader(StatusCode code) = 0;
        virtual void Write(std::string_view data) = 0;
        virtual Header &GetHeader() = 0;
        virtual ~ResponseWriter() = default;
    };

    struct Request
    {
        virtual ~Request() = default;
        virtual const std::string &GetMethod() const = 0;
        virtual const std::string &GetURL() const = 0;
        virtual const Header &GetHeader() const = 0;
        virtual const std::vector<uint8_t> &GetBody() const = 0;
    };

    using HandlerFunc = std::function<void(ResponseWriter &, const Request &)>;

    class Http
    {
    public:
        explicit Http(std::string_view host);
        ~Http();

        void HandleFunc(std::string_view pattern, HandlerFunc handler);
        void Run();
        void Stop();

    private:
        class Pimpl;
        std::unique_ptr<Pimpl> m_pimpl;
    };
} // namespace http_server