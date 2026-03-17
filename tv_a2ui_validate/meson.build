project(
  'tv_a2ui_validate',
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

tv_a2ui_validate_cpp_args = ['-DTV_VALIDATE_PROJECT_ROOT="@0@"'.format(
  meson.project_source_root(),
)]

tv_a2ui_validate_includes = include_directories('include')

tv_a2ui_validate_lib = static_library(
  'tv_a2ui_validate_lib',
  [
    'src/cli.cpp',
    'src/json.cpp',
    'src/validator.cpp',
  ],
  include_directories: tv_a2ui_validate_includes,
  dependencies: [json_dep],
  cpp_args: tv_a2ui_validate_cpp_args,
)

executable(
  'tv_a2ui_validate',
  'src/main.cpp',
  include_directories: tv_a2ui_validate_includes,
  link_with: tv_a2ui_validate_lib,
  install: true,
)

tv_a2ui_validate_tests = executable(
  'tv_a2ui_validate_tests',
  'tests/validator_test.cpp',
  include_directories: tv_a2ui_validate_includes,
  link_with: tv_a2ui_validate_lib,
  cpp_args: tv_a2ui_validate_cpp_args,
)

test('tv_a2ui_validate_tests', tv_a2ui_validate_tests)
