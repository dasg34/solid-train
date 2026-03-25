project(
  'tizen-tool-domain-fetch',
  'cpp',
  version: '0.1.0',
  default_options: [
    'cpp_std=c++17',
    'warning_level=3',
    'werror=true',
  ],
)

curl_dep = dependency('libcurl', required: true)
json_dep = dependency('json', required: false)
if not json_dep.found()
  json_dep = dependency('json-c', required: true)
endif
cpp_compiler = meson.get_compiler('cpp')
iconv_dep = cpp_compiler.find_library('iconv', required: false)
fixture_root = get_option('fixture_root')

tizen_tool_domain_fetch_cpp_args = ['-DTIZEN_TOOL_DOMAIN_FETCH_PROJECT_ROOT="@0@"'.format(
  meson.project_source_root(),
)]

if fixture_root != ''
  tizen_tool_domain_fetch_cpp_args += [
    '-DTIZEN_TOOL_DOMAIN_FETCH_DEFAULT_FIXTURE_ROOT="@0@"'.format(fixture_root),
  ]
endif

tizen_tool_domain_fetch_includes = include_directories('include')

tizen_tool_domain_fetch_lib = static_library(
  'tizen_tool_domain_fetch_lib',
  [
    'src/cli.cpp',
    'src/http_client.cpp',
    'src/json.cpp',
    'src/support.cpp',
    'src/news/news_fetcher.cpp',
    'src/youtube/youtube_fetcher.cpp',
    'src/finance/finance_fetcher.cpp',
    'src/commute/commute_fetcher.cpp',
    'src/daily/daily_fetcher.cpp',
    'src/emergency/emergency_fetcher.cpp',
    'src/scenario/scenario_fetcher.cpp',
    'src/schedule/schedule_fetcher.cpp',
    'src/sports/sports_fetcher.cpp',
    'src/travel/travel_fetcher.cpp',
    'src/weather/weather_fetcher.cpp',
  ],
  include_directories: tizen_tool_domain_fetch_includes,
  dependencies: [curl_dep, json_dep, iconv_dep],
  cpp_args: tizen_tool_domain_fetch_cpp_args,
)

executable(
  'tizen-tool-domain-fetch',
  'src/main.cpp',
  include_directories: tizen_tool_domain_fetch_includes,
  link_with: tizen_tool_domain_fetch_lib,
  install: true,
)

tizen_tool_domain_fetch_tests = executable(
  'tizen-tool-domain-fetch-tests',
  'tests/weather_fetcher_test.cpp',
  include_directories: tizen_tool_domain_fetch_includes,
  link_with: tizen_tool_domain_fetch_lib,
)

test('tizen-tool-domain-fetch-tests', tizen_tool_domain_fetch_tests)

install_data(
  [
    'fixtures/mock_weather_seoul.json',
    'fixtures/mock_news.json',
    'fixtures/mock_finance.json',
    'fixtures/mock_commute.json',
    'fixtures/mock_sports.json',
    'fixtures/mock_daily.json',
    'fixtures/mock_emergency.json',
    'fixtures/mock_family.json',
    'fixtures/mock_meal_delivery.json',
    'fixtures/mock_media.json',
    'fixtures/mock_schedule.json',
    'fixtures/mock_shopping.json',
    'fixtures/mock_smart_home.json',
    'fixtures/mock_travel.json',
    'fixtures/mock_wellness.json',
  ],
  install_dir: join_paths(get_option('datadir'), 'tizen-tool-domain-fetch', 'fixtures'),
)
