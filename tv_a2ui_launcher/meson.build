project(
  'tv_a2ui_launcher',
  'cpp',
  version: '0.1.0',
  default_options: [
    'cpp_std=c++20',
    'warning_level=3',
    'werror=true',
  ],
)

cpp_compiler = meson.get_compiler('cpp')
app_control_dep = dependency('capi-appfw-app-control', required: false)
if not app_control_dep.found()
  app_control_dep = cpp_compiler.find_library('app-control', required: false)
endif

launcher_includes = include_directories('include')

launcher_lib = static_library(
  'tv_a2ui_launcher_lib',
  [
    'src/cli.cpp',
    'src/launcher.cpp',
  ],
  include_directories: launcher_includes,
  dependencies: [app_control_dep],
)

executable(
  'tv_a2ui_launcher',
  'src/main.cpp',
  include_directories: launcher_includes,
  link_with: launcher_lib,
  install: true,
)

launcher_tests = executable(
  'tv_a2ui_launcher_tests',
  'tests/launcher_test.cpp',
  include_directories: launcher_includes,
  link_with: launcher_lib,
)

test('tv_a2ui_launcher_tests', launcher_tests)
