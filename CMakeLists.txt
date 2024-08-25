cmake_minimum_required(VERSION 3.22)



project(Http LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(Http STATIC 
    src/Http.cpp 
)
target_link_libraries(${PROJECT_NAME}
    PUBLIC 
    httpapi.lib
)

target_include_directories(${PROJECT_NAME} 
    PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

include(GNUInstallDirs)

message(STATUS "🌟 ${CMAKE_INSTALL_INCLUDEDIR}")
message(STATUS "🌟 ${CMAKE_INSTALL_LIBDIR}")
message(STATUS "🌟 ${CMAKE_CURRENT_SOURCE_DIR}")

# install(
#     TARGETS ${PROJECT_NAME} 
#     EXPORT "${PROJECT_NAME}Export" 
#     ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
#     INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
#     FILE_SET HEADERS
# )
# install(
#     DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME}
#     DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
#     FILES_MATCHING PATTERN "*.h")
# 
# install(EXPORT "${PROJECT_NAME}Export"
#     FILE "${PROJECT_NAME}Config.cmake"
#     NAMESPACE ${PROJECT_NAME}::
#     DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
# )