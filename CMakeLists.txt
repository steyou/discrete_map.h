cmake_minimum_required(VERSION 3.20)
project(discrete_map)

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_BUILD_TYPE Debug)

include_directories(include)
set(
    SRC_FILES
    main.cpp
)
add_executable(${PROJECT_NAME} ${SRC_FILES})

if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_link_libraries(${PROJECT_NAME})
