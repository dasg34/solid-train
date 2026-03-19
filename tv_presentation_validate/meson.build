project(
  'tv_presentation_validate',
  'cpp',
  version: '0.1.0',
  default_options: [
    'cpp_std=c++20',
    'warning_level=3',
    'werror=true',
  ],
)

json_dep = dependency('json', required: false)
if not json_dep.found()
  json_dep = dependency('json-c', required: true)
endif

tv_presentation_validate_includes = include_directories('include')

tv_presentation_validate_lib = static_library(
  'tv_presentation_validate_lib',
  [
    'src/cli.cpp',
    'src/json.cpp',
    'src/validator.cpp',
  ],
  include_directories: tv_presentation_validate_includes,
  dependencies: [json_dep],
)

executable(
  'tv_presentation_validate',
  'src/main.cpp',
  include_directories: tv_presentation_validate_includes,
  link_with: tv_presentation_validate_lib,
  install: true,
)

tv_presentation_validate_tests = executable(
  'tv_presentation_validate_tests',
  'tests/validator_test.cpp',
  include_directories: tv_presentation_validate_includes,
  link_with: tv_presentation_validate_lib,
)

test('tv_presentation_validate_tests', tv_presentation_validate_tests)
