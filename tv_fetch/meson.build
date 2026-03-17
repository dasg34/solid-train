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
json_dep = dependency('json', required: false)
if not json_dep.found()
  json_dep = dependency('json-c', required: true)
endif
fixture_root = get_option('fixture_root')

tv_fetch_cpp_args = ['-DTV_FETCH_PROJECT_ROOT="@0@"'.format(
  meson.project_source_root(),
)]

if fixture_root != ''
  tv_fetch_cpp_args += [
    '-DTV_FETCH_DEFAULT_FIXTURE_ROOT="@0@"'.format(fixture_root),
  ]
endif

tv_fetch_includes = include_directories('include')

tv_fetch_lib = static_library(
  'tv_fetch_lib',
  [
    'src/cli.cpp',
    'src/http_client.cpp',
    'src/json.cpp',
    'src/support.cpp',
    'src/news/news_fetcher.cpp',
    'src/finance/finance_fetcher.cpp',
    'src/commute/commute_fetcher.cpp',
    'src/sports/sports_fetcher.cpp',
    'src/weather/weather_fetcher.cpp',
  ],
  include_directories: tv_fetch_includes,
  dependencies: [curl_dep, json_dep],
  cpp_args: tv_fetch_cpp_args,
)

executable(
  'tv_fetch',
  'src/main.cpp',
  include_directories: tv_fetch_includes,
  link_with: tv_fetch_lib,
  install: true,
)

tv_fetch_tests = executable(
  'tv_fetch_tests',
  'tests/weather_fetcher_test.cpp',
  include_directories: tv_fetch_includes,
  link_with: tv_fetch_lib,
)

test('tv_fetch_tests', tv_fetch_tests)

install_data(
  [
    'fixtures/mock_weather_seoul.json',
    'fixtures/mock_news.json',
    'fixtures/mock_finance.json',
    'fixtures/mock_commute.json',
    'fixtures/mock_sports.json',
  ],
  install_dir: join_paths(get_option('datadir'), 'tv_fetch', 'fixtures'),
)
