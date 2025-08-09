project('dgate', 'cpp', default_options : ['cpp_std=c++17', 'warning_level=3'])

cxx = meson.get_compiler('cpp')
ev = cxx.find_library('ev', has_headers : 'ev++.h' )

executable('dgate', 'src/dgate/main.cxx', dependencies : ev)
