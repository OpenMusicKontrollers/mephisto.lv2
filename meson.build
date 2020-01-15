project('d2tk', 'c', default_options : [
	'buildtype=release',
	'warning_level=3',
	'werror=false',
	'b_lto=false',
	'c_std=gnu11'])

static_link = false #meson.is_cross_build()
build_debug = get_option('build-debug')

prefix = get_option('prefix')
datadir = get_option('datadir')
bindir = get_option('bindir')

pdatadir = join_paths(prefix, datadir, 'd2tk', '')

add_project_arguments('-DD2TK_DATA_DIR="'+pdatadir+'"', language : 'c')

cc = meson.get_compiler('c')

m_dep = cc.find_library('m')
util_dep = cc.find_library('util', required : false)
freetype_dep = dependency('freetype2', version : '>=18.0.0',
	static : static_link, required : false)
pixman_dep = dependency('pixman-1', version : '>=0.34.0',
	static : static_link, required : false)
cairo_dep = dependency('cairo', version : '>=1.14.0',
	static : static_link, required : false)
cairo_xlib_dep = dependency('cairo-xlib', version : '>=1.14.0',
	static : static_link, required : false)
evdev_dep = dependency('libevdev', version : '>=1.5.0',
	static : static_link, required : false)
input_dep = dependency('libinput', version : '>=1.6.0',
	static : static_link, required : false)
udev_dep = dependency('libudev', version : '>=220',
	static : static_link, required : false)
glew_dep = dependency('glew', version : '>=2.1.0',
	static : static_link, required : false)
vterm_dep = dependency('vterm', version : '>=0.1',
	static : static_link, required : false)

if not glew_dep.found()
	# use embedded glew
	glew_dep = declare_dependency(
		include_directories : include_directories('glew-2.1.0'),
		sources : join_paths('glew-2.1.0', 'glew.c'))
endif

deps = [m_dep, evdev_dep, vterm_dep]
links = []

pugl_inc = include_directories('pugl')
nanovg_inc = include_directories('nanovg/src')
inc_dir = [pugl_inc, nanovg_inc]

rawvers = run_command('cat', 'VERSION').stdout().strip()
version = rawvers.split('.')

conf_data = configuration_data()
conf_data.set('MAJOR_VERSION', version[0])
conf_data.set('MINOR_VERSION', version[1])
conf_data.set('MICRO_VERSION', version[2])

add_project_arguments('-D_GNU_SOURCE', language : 'c')

if build_debug
	add_project_arguments('-DD2TK_DEBUG', language : 'c')
endif

lib_srcs = [
	join_paths('src', 'hash.c'),
	join_paths('src', 'core.c'),
	join_paths('src', 'base.c'),
	join_paths('src', 'base_table.c'),
	join_paths('src', 'base_frame.c'),
	join_paths('src', 'base_layout.c'),
	join_paths('src', 'base_scrollbar.c'),
	join_paths('src', 'base_pane.c'),
	join_paths('src', 'base_cursor.c'),
	join_paths('src', 'base_button.c'),
	join_paths('src', 'base_image.c'),
	join_paths('src', 'base_bitmap.c'),
	join_paths('src', 'base_custom.c'),
	join_paths('src', 'base_meter.c'),
	join_paths('src', 'base_combo.c'),
	join_paths('src', 'base_textfield.c'),
	join_paths('src', 'base_label.c'),
	join_paths('src', 'base_link.c'),
	join_paths('src', 'base_dial.c'),
	join_paths('src', 'base_flowmatrix.c')
]

if vterm_dep.found()
	conf_data.set('D2TK_PTY', 1)
	lib_srcs += join_paths('src', 'base_pty.c')
	lib_srcs += join_paths('linenoise', 'linenoise.c')
	deps += util_dep
else
	conf_data.set('D2TK_PTY', 0)
endif

if evdev_dep.found()
	conf_data.set('D2TK_EVDEV', 1)
else
	conf_data.set('D2TK_EVDEV', 0)
endif

if input_dep.found() and input_dep.version().version_compare('>=1.15.0')
	conf_data.set('D2TK_INPUT_1_15', 1)
else
	conf_data.set('D2TK_INPUT_1_15', 0)
endif

bin_srcs = [
	join_paths('example', 'example.c')
]

pugl_srcs = [
	join_paths('src', 'frontend_pugl.c'),
	join_paths('pugl', 'pugl', 'detail', 'implementation.c')
]

pugl_gl_srcs = []

pugl_cairo_srcs = []

pugl_bin_srcs = [
	join_paths('example', 'd2tk_pugl.c')
]

nanovg_srcs = [
	join_paths('nanovg', 'src', 'nanovg.c'),
	join_paths('src', 'backend_nanovg.c')
]

cairo_srcs = [
	join_paths('src', 'backend_cairo.c')
]

fbdev_srcs = [
	join_paths('src', 'frontend_fbdev.c')
]

fbdev_bin_srcs = [
	join_paths('example', 'd2tk_fbdev.c')
]

test_core_srcs = [
	join_paths('test', 'core.c'),
	join_paths('test', 'mock.c')
]

test_base_srcs = [
	join_paths('test', 'base.c'),
	join_paths('test', 'mock.c')
]

c_args = ['-fvisibility=hidden',
	'-ffast-math']

if host_machine.system() == 'windows'
	add_project_arguments('-DGLEW_STATIC', language : 'c')
	deps += cc.find_library('opengl32')
	deps += cc.find_library('gdi32')
	deps += cc.find_library('ws2_32')
	pugl_srcs += 'pugl/pugl/detail/win.c'
	pugl_gl_srcs += 'pugl/pugl/detail/win_gl.c'
	pugl_cairo_srcs += 'pugl/pugl/detail/win_cairo.c'
elif host_machine.system() == 'darwin'
	add_languages('objc')
	links += ['-framework', 'OpenGL']
	links += ['-framework', 'Cocoa']
	pugl_srcs += 'pugl/pugl/detail/mac.m'
	pugl_gl_srcs += 'pugl/pugl/detail/mac_gl.m'
	pugl_cairo_srcs += 'pugl/pugl/detail/mac_cairo.m'
else
	deps += dependency('gl')
	deps += dependency('x11', version : '>=1.6.0')
	deps += dependency('xext', version : '>=1.3.0')
	pugl_srcs += 'pugl/pugl/detail/x11.c'
	pugl_gl_srcs += 'pugl/pugl/detail/x11_gl.c'
	pugl_cairo_srcs += 'pugl/pugl/detail/x11_cairo.c'
endif

if freetype_dep.found() and pixman_dep.found() and cairo_dep.found() and cairo_xlib_dep.found() and (host_machine.system() == 'linux')
	d2tk_cairo = declare_dependency(
		compile_args : '-DPUGL_HAVE_CAIRO',
		include_directories : inc_dir,
		dependencies : [deps, freetype_dep, pixman_dep, cairo_dep, cairo_xlib_dep],
		link_args : links,
		sources : [lib_srcs, cairo_srcs, pugl_srcs, pugl_cairo_srcs])

	executable('d2tk.cairo', [bin_srcs, pugl_bin_srcs],
		c_args : c_args,
		include_directories : inc_dir,
		dependencies: d2tk_cairo,
		install : false)

	if input_dep.found() and udev_dep.found() and evdev_dep.found()
		d2tk_fbdev = declare_dependency(
			include_directories : inc_dir,
			dependencies : [deps, freetype_dep, pixman_dep, cairo_dep, input_dep, udev_dep],
			link_args : links,
			sources : [lib_srcs, cairo_srcs, fbdev_srcs])

		executable('d2tk.fbdev', [bin_srcs, fbdev_bin_srcs],
			c_args : c_args,
			include_directories : inc_dir,
			dependencies: d2tk_fbdev,
			install : false)
	endif
endif

d2tk_nanovg = declare_dependency(
	include_directories : inc_dir,
	dependencies : [deps, glew_dep],
	link_args : links,
	sources : [lib_srcs, nanovg_srcs, pugl_srcs, pugl_gl_srcs])

executable('d2tk.nanovg', [bin_srcs, pugl_bin_srcs],
	c_args : c_args,
	include_directories : inc_dir,
	dependencies: d2tk_nanovg,
	install : false)

configure_file(
	input : join_paths('d2tk', 'config.h.in'),
	output : 'config.h',
	configuration : conf_data,
	install : false)

configure_file(
	input : join_paths('ttf', 'FiraSans-Bold.ttf'),
	output : 'FiraSans-Bold.ttf',
	copy : true,
	install : false)

configure_file(
	input : join_paths('ttf', 'FiraMono-Bold.ttf'),
	output : 'FiraMono-Bold.ttf',
	copy : true,
	install : false)

configure_file(
	input : join_paths('ttf', 'FiraCode-Bold.ttf'),
	output : 'FiraCode-Bold.ttf',
	copy : true,
	install : false)

configure_file(
	input : join_paths('ttf', 'FiraCode-Light.ttf'),
	output : 'FiraCode-Light.ttf',
	copy : true,
	install : false)

configure_file(
	input : join_paths('ttf', 'FiraCode-Medium.ttf'),
	output : 'FiraCode-Medium.ttf',
	copy : true,
	install : false)

configure_file(
	input : join_paths('ttf', 'FiraCode-Regular.ttf'),
	output : 'FiraCode-Regular.ttf',
	copy : true,
	install : false)

configure_file(
	input : join_paths('example', 'libre-arrow-circle-right.png'),
	output : 'libre-arrow-circle-right.png',
	copy : true,
	install : false)

configure_file(
	input : join_paths('example', 'libre-gui-folder.png'),
	output : 'libre-gui-folder.png',
	copy : true,
	install : false)

configure_file(
	input : join_paths('example', 'libre-gui-file.png'),
	output : 'libre-gui-file.png',
	copy : true,
	install : false)

test_core = executable('test.core', [test_core_srcs, lib_srcs],
	c_args : c_args,
	dependencies : deps,
	include_directories : inc_dir,
	install : false)

test_base = executable('test.base', [test_base_srcs, lib_srcs],
	c_args : c_args,
	dependencies : deps,
	include_directories : inc_dir,
	install : false)

test('Test core', test_core)
test('Test base', test_base)
