project(
  'tv_fetch',
  'cpp',
  version: '0.1.0',
  default_options: [
    'cpp_std=c++20',
    'warning_level=3',
    'werror=true',
  ],
)

curl_dep = dependency('libcurl', required: true)

project_root_define = '-DTV_FETCH_PROJECT_ROOT="@0@"'.format(
  meson.project_source_root(),
)

tv_fetch_includes = include_directories('include', 'third_party')

tv_fetch_lib = static_library(
  'tv_fetch_lib',
  [
    'src/cli.cpp',
    'src/http_client.cpp',
    'src/weather/weather_fetcher.cpp',
  ],
  include_directories: tv_fetch_includes,
  dependencies: curl_dep,
  cpp_args: [project_root_define],
)

executable(
  'tv_fetch',
  'src/main.cpp',
  include_directories: tv_fetch_includes,
  link_with: tv_fetch_lib,
)

tv_fetch_tests = executable(
  'tv_fetch_tests',
  'tests/weather_fetcher_test.cpp',
  include_directories: tv_fetch_includes,
  link_with: tv_fetch_lib,
)

test('tv_fetch_tests', tv_fetch_tests)
