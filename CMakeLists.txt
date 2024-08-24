cmake_minimum_required(VERSION 3.22)



set(CMAKE_CXX_STANDARD 23)

project(Http LANGUAGES CXX)


add_library(Http STATIC)

target_sources(${PROJECT_NAME}
    PRIVATE
    src/Http.cpp
    PUBLIC FILE_SET HEADERS
    BASE_DIRS include
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
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    INTERFACE
    "$<INSTALL_INTERFACE:include>"
)

include(GNUInstallDirs)

install(
    TARGETS ${PROJECT_NAME} 
    EXPORT "${PROJECT_NAME}Export" 
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILE_SET HEADERS
)

install(EXPORT "${PROJECT_NAME}Export"
    FILE "${PROJECT_NAME}Config.cmake"
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)
# install(DIRECTORY include DESTINATION ${CMAKE_INSTALL_PREFIX})