cmake_minimum_required(VERSION 3.20)

project(tv_fetch VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CURL REQUIRED)

add_library(
  tv_fetch_lib
  src/cli.cpp
  src/http_client.cpp
  src/weather/weather_fetcher.cpp
)

target_include_directories(
  tv_fetch_lib
  PUBLIC
    include
    third_party
)

target_link_libraries(tv_fetch_lib PRIVATE CURL::libcurl)

target_compile_definitions(
  tv_fetch_lib
  PRIVATE TV_FETCH_PROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_options(
  tv_fetch_lib
  PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_executable(tv_fetch src/main.cpp)
target_link_libraries(tv_fetch PRIVATE tv_fetch_lib)

enable_testing()

add_executable(tv_fetch_tests tests/weather_fetcher_test.cpp)
target_link_libraries(tv_fetch_tests PRIVATE tv_fetch_lib)

add_test(NAME tv_fetch_tests COMMAND tv_fetch_tests)
