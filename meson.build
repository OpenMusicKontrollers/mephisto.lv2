project('mephisto.lv2', 'c', default_options : [
	'buildtype=release',
	'warning_level=3',
	'werror=false',
	'b_lto=false',
	'c_std=gnu11'])

d2tk = subproject('d2tk')

use_backend = get_option('use-backend')
if use_backend == 'nanovg'
	d2tk_dep = d2tk.get_variable('d2tk_nanovg')
elif use_backend == 'cairo'
	d2tk_dep = d2tk.get_variable('d2tk_cairo')
else
	error('no valid UI backend given')
endif

source_root = meson.source_root()
build_root = meson.build_root()

add_project_arguments('-D_GNU_SOURCE', language : 'c')

conf_data = configuration_data()
cc = meson.get_compiler('c')

lv2_validate = find_program('lv2_validate', native : true, required : false)
sord_validate = find_program('sord_validate', native : true, required : false)
lv2lint = find_program('lv2lint', required : false)
faust = find_program('faust', required : false)

if cc.has_member('UIGlue', 'addSoundFile',
		prefix : '#include <faust/gui/CInterface.h>')
	add_project_arguments('-DFAUST_ADDSOUNDFILE=addSoundFile', language : 'c')
else
	add_project_arguments('-DFAUST_ADDSOUNDFILE=addSoundfile', language : 'c')
endif

m_dep = cc.find_library('m')
lv2_dep = dependency('lv2', version : '>=1.14.0')
faust_dep = cc.find_library('faust')
util_dep = cc.find_library('util')
rt_dep = cc.find_library('rt')
thread_dep = dependency('threads')
vterm_dep = dependency('vterm', version : '>=0.1')

dsp_deps = [m_dep, lv2_dep, faust_dep]
ui_deps = [lv2_dep, d2tk_dep, rt_dep, vterm_dep, thread_dep]

props_inc = include_directories('props.lv2')
timely_inc = include_directories('timely.lv2')
ser_inc = include_directories('ser_atom.lv2')
varchunk_inc = include_directories('varchunk')
d2tk_inc = include_directories(join_paths('subprojects', 'd2tk'))
inc_dir = [props_inc, timely_inc, ser_inc, varchunk_inc, d2tk_inc]

inst_dir = join_paths(get_option('libdir'), 'lv2', meson.project_name())

dsp_srcs = ['mephisto.c']

ui_srcs = ['mephisto_ui.c']

c_args = ['-fvisibility=hidden']

version = run_command('cat', 'VERSION').stdout().strip().split('.')
add_project_arguments('-DMEPHISTO_VERSION="'
	+ version[0] + '.' + version[1] + '.' + version[2] + '"', language : 'c')
conf_data.set('MAJOR_VERSION', version[0])
conf_data.set('MINOR_VERSION', version[1])
conf_data.set('MICRO_VERSION', version[2])

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
	dependencies : ui_deps,
	install : true,
	install_dir : inst_dir)

bank_filter_through = run_command('cat', 'bank-filter_through.dsp').stdout()
conf_data.set('BANK-FILTER_THROUGH', bank_filter_through)

bank_filter_gain = run_command('cat', 'bank-filter_gain.dsp').stdout()
conf_data.set('BANK-FILTER_GAIN', bank_filter_gain)

bank_time_lfo = run_command('cat', 'bank-time_lfo.dsp').stdout()
conf_data.set('BANK-TIME_LFO', bank_time_lfo)

bank_instrument_osc = run_command('cat', 'bank-instrument_osc.dsp').stdout()
conf_data.set('BANK-INSTRUMENT_OSC', bank_instrument_osc)

suffix = mod.full_path().strip().split('.')[-1]
conf_data.set('MODULE_SUFFIX', '.' + suffix)

manifest_ttl = configure_file(
	input : 'manifest.ttl.in',
	output : 'manifest.ttl',
	configuration : conf_data,
	install : true,
	install_dir : inst_dir)

dsp_ttl = configure_file(
	input : 'mephisto.ttl.in',
	output : 'mephisto.ttl',
	configuration : conf_data,
	install : true,
	install_dir : inst_dir)

ui_ttl = configure_file(
	input : 'mephisto_ui.ttl',
	output : 'mephisto_ui.ttl',
	copy: true,
	install : true,
	install_dir : inst_dir)

pset_ttl = configure_file(
	input : 'presets.ttl.in',
	output : 'presets.ttl',
	configuration : conf_data,
	install : true,
	install_dir : inst_dir)

configure_file(
	input : 'FiraCode-Bold.ttf',
	output : 'FiraCode-Bold.ttf',
	copy : true,
	install : true,
	install_dir : inst_dir)
configure_file(
	input : 'FiraCode-Medium.ttf',
	output : 'FiraCode-Medium.ttf',
	copy : true,
	install : true,
	install_dir : inst_dir)
configure_file(
	input : 'FiraCode-Regular.ttf',
	output : 'FiraCode-Regular.ttf',
	copy : true,
	install : true,
	install_dir : inst_dir)
configure_file(
	input : 'FiraCode-Light.ttf',
	output : 'FiraCode-Light.ttf',
	copy : true,
	install : true,
	install_dir : inst_dir)

if lv2_validate.found() and sord_validate.found()
	test('LV2 validate', lv2_validate,
		args : [manifest_ttl, dsp_ttl, ui_ttl, pset_ttl])
endif

if lv2lint.found()
	test('LV2 lint', lv2lint,
		args : ['-M', 'pack',
			'-E', 'warn',
			'-I', join_paths(build_root, ''),
			'http://open-music-kontrollers.ch/lv2/mephisto#audio_1x1',
			'http://open-music-kontrollers.ch/lv2/mephisto#audio_2x2',
			'http://open-music-kontrollers.ch/lv2/mephisto#audio_4x4',
			'http://open-music-kontrollers.ch/lv2/mephisto#audio_8x8',
			'http://open-music-kontrollers.ch/lv2/mephisto#cv_1x1',
			'http://open-music-kontrollers.ch/lv2/mephisto#cv_2x2',
			'http://open-music-kontrollers.ch/lv2/mephisto#cv_4x4',
			'http://open-music-kontrollers.ch/lv2/mephisto#cv_8x8'
		])
endif

if faust.found()
	test('FAUST bank-filter_through', faust, args : [
		join_paths(source_root, 'bank-filter_through.dsp')
	])
	test('FAUST bank-filter_gain', faust, args : [
		join_paths(source_root, 'bank-filter_gain.dsp')
	])
	test('FAUST bank-time_lfo', faust, args : [
		join_paths(source_root, 'bank-time_lfo.dsp')
	])
	test('FAUST bank-instrument_osc', faust, args : [
		join_paths(source_root, 'bank-instrument_osc.dsp')
	])
endif
