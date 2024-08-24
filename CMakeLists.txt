cmake_minimum_required(VERSION 3.22)

include(GNUInstallDirs)

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

install(TARGETS ${PROJECT_NAME} 
    EXPORT HttpTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT HttpTargets
    FILE HttpTargets.cmake
    NAMESPACE Http::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Http
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/HttpConfigVersion.cmake
    VERSION 1.0.0
    COMPATIBILITY AnyNewerVersion
)