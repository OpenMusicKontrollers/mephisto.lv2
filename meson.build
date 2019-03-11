project('mephisto.lv2', 'c', default_options : [
	'buildtype=release',
	'warning_level=3',
	'werror=false',
	'b_lto=true',
	'c_std=c11'])

add_project_arguments('-D_GNU_SOURCE', language : 'c')

conf_data = configuration_data()
cc = meson.get_compiler('c')

cp = find_program('cp')
lv2_validate = find_program('lv2_validate', native : true, required : false)
lv2lint = find_program('lv2lint', required : false)
clone = [cp, '@INPUT@', '@OUTPUT@']

m_dep = cc.find_library('m')
lv2_dep = dependency('lv2', version : '>=1.14.0')
faust_dep = cc.find_library('libfaust')
dsp_deps = [m_dep, lv2_dep, faust_dep]

props_inc = include_directories('props.lv2')
ser_inc = include_directories('ser_atom.lv2')
varchunk_inc = include_directories('varchunk')
inc_dir = [props_inc, ser_inc, varchunk_inc]

inst_dir = join_paths(get_option('libdir'), 'lv2', meson.project_name())

dsp_srcs = ['mephisto.c']

ui_srcs = ['mephisto_ui.c']

c_args = ['-fvisibility=hidden',
	'-ffast-math']

if host_machine.system() == 'windows'
	# nothing
elif host_machine.system() == 'darwin'
	# nothing
else
	# nothing
endif

mod = shared_module('mephisto', dsp_srcs,
	c_args : c_args,
	include_directories : inc_dir,
	name_prefix : '',
	dependencies : dsp_deps,
	install : true,
	install_dir : inst_dir)

ui = shared_module('mephisto_ui', ui_srcs,
	c_args : c_args,
	include_directories : inc_dir,
	name_prefix : '',
	dependencies : dsp_deps,
	install : true,
	install_dir : inst_dir)

version = run_command('cat', 'VERSION').stdout().strip().split('.')
conf_data.set('MAJOR_VERSION', version[0])
conf_data.set('MINOR_VERSION', version[1])
conf_data.set('MICRO_VERSION', version[2])

suffix = mod.full_path().strip().split('.')[-1]
conf_data.set('MODULE_SUFFIX', '.' + suffix)

manifest_ttl = configure_file(input : 'manifest.ttl.in', output : 'manifest.ttl',
	configuration : conf_data,
	install : true,
	install_dir : inst_dir)
dsp_ttl = custom_target('mephisto_ttl',
	input : 'mephisto.ttl',
	output : 'mephisto.ttl',
	command : clone,
	install : true,
	install_dir : inst_dir)
ui_ttl = custom_target('mephisto_ui_ttl',
	input : 'mephisto_ui.ttl',
	output : 'mephisto_ui.ttl',
	command : clone,
	install : true,
	install_dir : inst_dir)
pset_ttl = custom_target('presets.ttl',
	input : 'presets.ttl',
	output : 'presets.ttl',
	command : clone,
	install : true,
	install_dir : inst_dir)

if lv2_validate.found()
	test('LV2 validate', lv2_validate,
		args : [manifest_ttl, dsp_ttl, ui_ttl, pset_ttl])
endif

if lv2lint.found()
	test('LV2 lint', lv2lint,
		args : ['-Swarn',
			'http://open-music-kontrollers.ch/lv2/mephisto#mono',
			'http://open-music-kontrollers.ch/lv2/mephisto#stereo'])
endif
