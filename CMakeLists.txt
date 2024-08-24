cmake_minimum_required(VERSION 3.22)

set(CMAKE_CXX_STANDARD 23)

project(Http LANGUAGES CXX)


add_library(Http STATIC
    src/Http.cpp
)

target_sources(${PROJECT_NAME}
    PRIVATE
    src/Http.cpp
    PUBLIC
    include/Http/Http.h
)


target_compile_definitions(${PROJECT_NAME}
    PUBLIC
    -DUNICODE
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE 
    httpapi.lib
)

target_include_directories(${PROJECT_NAME} 
    PUBLIC
    include
)
message(STATUS "CMAKE_INSTALL_INCLUDEDIR: ${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})