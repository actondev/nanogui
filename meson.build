project('nanogui', 'cpp')

glfw_dep = dependency('glfw')
nanovg_dep = dependency('nanovg')

inc =[include_directories('include')]

subdir('src')

glad = false

defines = [
  '-DNANOGUI_USE_OPENGL',
]

if glad
  defines += '-DNANOGUI_GLAD'
  src += files('ext/glad/src/glad.c')
  inc += include_directories
endif

('ext/glad/include')

nanogui = library('nanogui',
                  src, # from src subdir
                  include_directories: inc,
                  cpp_args: defines,
                  dependencies: [
                    glfw_dep,
                    nanovg_dep
                  ])

nanogui_dep = declare_dependency(
  include_directories : inc,
  link_with : nanogui)
