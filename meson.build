project('lispy', 'c')

args = ['-g', '-O3', '-Wall', '-Wextra', '-Wshadow']
files = ['main.c', 'mpc.c']

cc = meson.get_compiler('c')
deps = [cc.find_library('m'), cc.find_library('readline')]

executable('lispy', files, c_args : args, dependencies : deps)
