cmake_minimum_required(VERSION 3.20)
project(geoviz)

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_BUILD_TYPE Debug)

include_directories(include)
set(
    SRC_FILES
    main.cpp
)
add_executable(${PROJECT_NAME} ${SRC_FILES})

target_link_libraries(${PROJECT_NAME})
