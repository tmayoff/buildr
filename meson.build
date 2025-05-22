project('buildr', 'cpp')

cpp = meson.get_compiler('cpp')

boost = dependency('boost')
boost_deps = cpp.find_library('boost_program_options')

executable(
    'buildr',
    'src/main.cpp',
    dependencies: [boost, boost_deps],
)
