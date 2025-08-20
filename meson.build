project('dgate', 'cpp', default_options : ['cpp_std=c++20', 'warning_level=3', 'buildtype=debugoptimized'])

cxx = meson.get_compiler('cpp')
ev = cxx.find_library('ev', has_headers : 'ev++.h' )
lmdb = dependency('lmdb')

deps = [ ev, lmdb ]

incdir = include_directories('src')

dgate_src = [
  'src/dgate/main.cxx',
  'src/dgate/app.cxx',
  'src/dgate/packet.cxx',

  'src/ircddb/app.cxx',
  'src/ircddb/client.cxx',
  'src/ircddb/irc_msg.cxx',

  'src/dv/frame.cxx',
  'src/dv/header.cxx',
  'src/dv/crc.cxx',
]

executable('dgate', dgate_src, dependencies : deps, include_directories : incdir)

ditap_src = [
  'src/itap/main.cxx',
  'src/itap/app.cxx',
  'src/dgate/client.cxx',
  'src/dgate/packet.cxx',
  'src/dv/header.cxx',
  'src/dv/crc.cxx',
]

executable('ditap', ditap_src, dependencies : ev, include_directories : incdir)

dlink_src = [
  'src/dlink/main.cxx',
  'src/dlink/app.cxx',
  'src/dlink/app_xrf.cxx',
  'src/dgate/client.cxx',
  'src/dgate/packet.cxx',
  'src/dv/crc.cxx',
]

executable('dlink', dlink_src, dependencies : ev, include_directories : incdir)
