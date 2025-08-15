project('dgate', 'cpp', default_options : ['cpp_std=c++20', 'warning_level=3', 'buildtype=debugoptimized'])

cxx = meson.get_compiler('cpp')
ev = cxx.find_library('ev', has_headers : 'ev++.h' )
lmdb = dependency('lmdb')

deps = [ ev, lmdb ]

incdir = include_directories('src')

dgate_src = [
  'src/dgate/main.cxx',
  'src/ircddb/app.cxx',
  'src/ircddb/client.cxx',
  'src/ircddb/irc_msg.cxx',
  'src/dv/frame.cxx',
  'src/dv/header.cxx',
]

executable('dgate', dgate_src, dependencies : deps, include_directories : incdir)
