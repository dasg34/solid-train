project(
  'tizen-tool-presentation-validate',
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

tizen_tool_presentation_validate_includes = include_directories('include')

tizen_tool_presentation_validate_lib = static_library(
  'tizen_tool_presentation_validate_lib',
  [
    'src/cli.cpp',
    'src/json.cpp',
    'src/validator.cpp',
  ],
  include_directories: tizen_tool_presentation_validate_includes,
  dependencies: [json_dep],
)

executable(
  'tizen-tool-presentation-validate',
  'src/main.cpp',
  include_directories: tizen_tool_presentation_validate_includes,
  link_with: tizen_tool_presentation_validate_lib,
  install: true,
)

tizen_tool_presentation_validate_tests = executable(
  'tizen_tool_presentation_validate_tests',
  'tests/validator_test.cpp',
  include_directories: tizen_tool_presentation_validate_includes,
  link_with: tizen_tool_presentation_validate_lib,
)

test(
  'tizen_tool_presentation_validate_tests',
  tizen_tool_presentation_validate_tests,
)
