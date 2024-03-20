project('simple-raytracer', 'cpp', default_options : ['cpp_std=gnu++20'])

boost = dependency('boost')

imgui = dependency('imgui', default_options : ['sdl2=enabled'])
sdl2 = dependency('sdl2')
opencl = dependency('OpenCL')

includes = include_directories('lib', 'include')

files = [
  'lib/tiny-gizmo.cpp',
  'src/interface.cpp',
  'src/shape.cpp',
  'src/parser.cpp',
  'src/tracer.cpp',
  'src/main.cpp'
]

executable('tracer',
  files,
  include_directories : includes,
  dependencies : [boost, imgui, sdl2, opencl]
)
