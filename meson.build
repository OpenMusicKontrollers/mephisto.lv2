project('mephisto.lv2', 'c', default_options : [
	'buildtype=release',
	'warning_level=3',
	'werror=false',
	'b_lto=false',
	'c_std=c11'])

add_project_arguments('-D_GNU_SOURCE', language : 'c')

conf_data = configuration_data()
cc = meson.get_compiler('c')

lv2_validate = find_program('lv2_validate', native : true, required : false)
sord_validate = find_program('sord_validate', native : true, required : false)
lv2lint = find_program('lv2lint', required : false)
faust = find_program('faust', required : true)

# get FAUST version
r = run_command(faust, '--version')
faust_ver = r.stdout().split('Version')[1].split()[0].strip()
faust_arr = faust_ver.split('.')
add_project_arguments('-DFAUST_MAJOR_VERSION=' + faust_arr[0], language : 'c')
add_project_arguments('-DFAUST_MINOR_VERSION=' + faust_arr[1], language : 'c')
add_project_arguments('-DFAUST_MICRO_VERSION=' + faust_arr[2], language : 'c')

if faust_ver.version_compare('>=2.14.4')
	message('supported FAUST version found ' + faust_ver)
else
	error('unsupported FAUST version found ' + faust_ver)
endif

m_dep = cc.find_library('m')
lv2_dep = dependency('lv2', version : '>=1.14.0')
faust_dep = cc.find_library('faust')
dsp_deps = [m_dep, lv2_dep, faust_dep]

props_inc = include_directories('props.lv2')
timely_inc = include_directories('timely.lv2')
ser_inc = include_directories('ser_atom.lv2')
varchunk_inc = include_directories('varchunk')
inc_dir = [props_inc, timely_inc, ser_inc, varchunk_inc]

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

if lv2_validate.found() and sord_validate.found()
	test('LV2 validate', lv2_validate,
		args : [manifest_ttl, dsp_ttl, ui_ttl, pset_ttl])
endif

if lv2lint.found()
	test('LV2 lint', lv2lint,
		args : ['-Swarn',
			'http://open-music-kontrollers.ch/lv2/mephisto#audio_1x1',
			'http://open-music-kontrollers.ch/lv2/mephisto#audio_2x2',
			'http://open-music-kontrollers.ch/lv2/mephisto#audio_4x4',
			'http://open-music-kontrollers.ch/lv2/mephisto#audio_8x8',
			'http://open-music-kontrollers.ch/lv2/mephisto#cv_1x1',
			'http://open-music-kontrollers.ch/lv2/mephisto#cv_2x2',
			'http://open-music-kontrollers.ch/lv2/mephisto#cv_4x4',
			'http://open-music-kontrollers.ch/lv2/mephisto#cv_8x8'])
endif

if faust.found()
	test('FAUST bank-filter_through', faust,
		args : ['../bank-filter_through.dsp']) #FIXME path
	test('FAUST bank-filter_gain', faust,
		args : ['../bank-filter_gain.dsp']) #FIXME path
	test('FAUST bank-time_lfo', faust,
		args : ['../bank-time_lfo.dsp']) #FIXME path
	test('FAUST bank-instrument_osc', faust,
		args : ['../bank-instrument_osc.dsp']) #FIXME path
endif
