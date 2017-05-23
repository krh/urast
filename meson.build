project('urast', 'c')

c = meson.get_compiler('c')

libpng16 = dependency('libpng16')

urast_files = files('urast.c', 'urast.h', 'demo.c')

executable('urast',
	urast_files,
	c_args : [ '-march=core-avx2', '-D_GNU_SOURCE' ],
	dependencies : [ libpng16 ],
	install : true)
