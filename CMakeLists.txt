cmake_minimum_required(VERSION 3.22)

set(CMAKE_CXX_STANDARD 23)

project(Http LANGUAGES CXX)


add_library(Http STATIC
    src/Http.cpp
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        include
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE 
    httpapi.lib
)