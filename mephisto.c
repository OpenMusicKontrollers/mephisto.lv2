/*
 * Copyright (c) 2019 Hanspeter Portner (dev@open-music-kontrollers.ch)
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the Artistic License 2.0 as published by
 * The Perl Foundation.
 *
 * This source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * Artistic License 2.0 for more details.
 *
 * You should have received a copy of the Artistic License 2.0
 * along the source as a COPYING file. If not, obtain it from
 * http://www.perlfoundation.org/artistic_license_2_0.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <inttypes.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <pthread.h>
#include <inttypes.h>

#include <mephisto.h>
#include <props.h>
#include <varchunk.h>

#include <faust/dsp/llvm-c-dsp.h>

#define MAX_CHANNEL 2
#define MAX_LABEL 32
#define MAX_VOICES 48

typedef struct _dsp_t dsp_t;
typedef struct _job_t job_t;
typedef struct _plughandle_t plughandle_t;

typedef struct _cntrl_button_t cntrl_button_t;
typedef struct _cntrl_check_button_t cntrl_check_button_t;
typedef struct _cntrl_vertical_slider_t cntrl_vertical_slider_t;
typedef struct _cntrl_horizontal_slider_t cntrl_horizontal_slider_t;
typedef struct _cntrl_num_entry_t cntrl_num_entry_t;
typedef struct _cntrl_horizontal_bargraph_t cntrl_horizontal_bargraph_t;
typedef struct _cntrl_vertical_bargraph_t cntrl_vertical_bargraph_t;
typedef struct _cntrl_sound_file_t cntrl_sound_file_t;

typedef struct _cntrl_t cntrl_t;

typedef enum _cntrl_type_t {
	CNTRL_BUTTON,
	CNTRL_CHECK_BUTTON,
	CNTRL_VERTICAL_SLIDER,
	CNTRL_HORIZONTAL_SLIDER,
	CNTRL_NUM_ENTRY,
	CNTRL_HORIZONTAL_BARGRAPH,
	CNTRL_VERTICAL_BARGRAPH,
	CNTRL_SOUND_FILE
} cntrl_type_t;

struct _cntrl_button_t {
	float *zone;
};

struct _cntrl_check_button_t {
	float *zone;
};

struct _cntrl_vertical_slider_t {
	float init;
	float min;
	float max;
	float ran;
	float step;
	float *zone;
};

struct _cntrl_horizontal_slider_t {
	float init;
	float min;
	float max;
	float ran;
	float step;
	float *zone;
};

struct _cntrl_num_entry_t {
	float init;
	float min;
	float max;
	float ran;
	float step;
	float *zone;
};

struct _cntrl_horizontal_bargraph_t {
	float min;
	float max;
	float *zone;
};

struct _cntrl_vertical_bargraph_t {
	float min;
	float max;
	float *zone;
};

struct _cntrl_sound_file_t {
	uint32_t dummy; //FIXME
};

struct _cntrl_t {
	char label [MAX_LABEL];
	cntrl_type_t type;
	union {
		cntrl_button_t button;
		cntrl_check_button_t check_button;
		cntrl_vertical_slider_t vertical_slider;
		cntrl_horizontal_slider_t horizontal_slider;
		cntrl_num_entry_t num_entry;
		cntrl_horizontal_bargraph_t horizontal_bargraph;
		cntrl_vertical_bargraph_t vertical_bargraph;
		cntrl_sound_file_t sound_file;
	};
};

struct _dsp_t {
	plughandle_t *handle;
	llvm_dsp_factory *factory;
	llvm_dsp *instance;
	UIGlue ui_glue;
	MetaGlue meta_glue;
	uint32_t nins;
	uint32_t nouts;
	uint32_t ncntrls;
	cntrl_t cntrls [NCONTROLS];
	bool has_freq;
	bool has_gain;
	bool has_gate;
	uint32_t nvoices;
};

typedef enum _job_type_t {
	JOB_TYPE_INIT,
	JOB_TYPE_DEINIT,
} job_type_t;

struct _job_t {
	job_type_t type;
	dsp_t *dsp;
};

struct _plughandle_t {
	LV2_URID_Map *map;
	LV2_Worker_Schedule *sched;
	LV2_Atom_Forge forge;
	LV2_Atom_Forge_Ref ref;

	LV2_Log_Log *log;
	LV2_Log_Logger logger;

	plugstate_t state;
	plugstate_t stash;

	const LV2_Atom_Sequence *control;
	LV2_Atom_Sequence *notify;
	const float *audio_in [MAX_CHANNEL];
	float *audio_out [MAX_CHANNEL];
	unsigned nchannel;

	PROPS_T(props, MAX_NPROPS);

	varchunk_t *to_worker;

	uint32_t xfade_max;
	uint32_t xfade_cur;
	uint32_t xfade_dst;

	uint32_t srate;
	char bundle_path [PATH_MAX];

	bool play;
	dsp_t *dsp [2];
};

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static void
_intercept_code(void *data, int64_t frames __attribute__((unused)),
	props_impl_t *impl)
{
	plughandle_t *handle = data;

	char *code;
	if( (code = varchunk_write_request(handle->to_worker, impl->value.size)) )
	{
		memcpy(code, handle->state.code, impl->value.size);

		varchunk_write_advance(handle->to_worker, impl->value.size);

		const job_t job = {
			.type = JOB_TYPE_INIT
		};
		handle->sched->schedule_work(handle->sched->handle, sizeof(job), &job);
	}
	else if(handle->log)
	{
		lv2_log_trace(&handle->logger, "[%s] ringbuffer overflow\n", __func__);
	}
}

static void
_refresh_value(plughandle_t *handle, uint32_t idx)
{
	dsp_t *dsp = handle->dsp[handle->play];

	if(!dsp)
	{
		return;
	}

	cntrl_t *cntrl = idx < dsp->ncntrls
		? &dsp->cntrls[idx]
		: NULL;

	if(!cntrl)
	{
		return;
	}

	switch(cntrl->type)
	{
		case CNTRL_BUTTON:
		{
			const float val = handle->state.control[idx] > 0.5f
				? 1.f
				: 0.0;

			if(cntrl->button.zone)
			{
				*cntrl->button.zone = val;
			}
		} break;
		case CNTRL_CHECK_BUTTON:
		{
			const float val = handle->state.control[idx] > 0.5f
				? 1.f
				: 0.0;

			if(cntrl->check_button.zone)
			{
				*cntrl->check_button.zone = val;
			}
		} break;
		case CNTRL_VERTICAL_SLIDER:
		{
			const float val = handle->state.control[idx] * cntrl->vertical_slider.ran
				+ cntrl->vertical_slider.min;

			if(cntrl->vertical_slider.zone)
			{
				*cntrl->vertical_slider.zone = val;
			}
		} break;
		case CNTRL_HORIZONTAL_SLIDER:
		{
			const float val = handle->state.control[idx] * cntrl->horizontal_slider.ran
				+ cntrl->horizontal_slider.min;

			if(cntrl->horizontal_slider.zone)
			{
				*cntrl->horizontal_slider.zone = val;
			}
		} break;
		case CNTRL_NUM_ENTRY:
		{
			const float val = handle->state.control[idx] * cntrl->num_entry.ran
				+ cntrl->num_entry.min;

			if(cntrl->num_entry.zone)
			{
				*cntrl->num_entry.zone = val;
			}
		} break;
		case CNTRL_HORIZONTAL_BARGRAPH:
		{
			//FIXME
		} break;
		case CNTRL_VERTICAL_BARGRAPH:
		{
			//FIXME
		} break;
		case CNTRL_SOUND_FILE:
		{
			//FIXME
		} break;
	}
}

static void
_intercept_control(void *data, int64_t frames __attribute__((unused)),
	props_impl_t *impl)
{
	plughandle_t *handle = data;
	const uint32_t idx = (float *)impl->value.body - handle->state.control;

	_refresh_value(handle, idx);
}

static const props_def_t defs [MAX_NPROPS] = {
	{
		.property = MEPHISTO__code,
		.offset = offsetof(plugstate_t, code),
		.type = LV2_ATOM__String,
		.event_cb = _intercept_code,
		.max_size = CODE_SIZE
	},
	CONTROL(1),
	CONTROL(2),
	CONTROL(3),
	CONTROL(4),
	CONTROL(5),
	CONTROL(6),
	CONTROL(7),
	CONTROL(1),
	CONTROL(9),
	CONTROL(10),
	CONTROL(11),
	CONTROL(12),
	CONTROL(13),
	CONTROL(14),
	CONTROL(15),
	CONTROL(16)
};

static inline void
_play(plughandle_t *handle, int64_t from, int64_t to)
{
	const uint32_t nsamples = to - from;
	const size_t buflen = nsamples * sizeof(float);

	float *audio_in [3] = {
		alloca(buflen), //FIXME check
		handle->nchannel > 1
			? alloca(buflen)
			: NULL, //FIXME check
		NULL
	};
	float *audio_out [3] = {
		&handle->audio_out[0][from],
		handle->nchannel > 1
			? &handle->audio_out[1][from]
			: NULL,
		NULL
	};

	for(uint32_t n = 0; n < handle->nchannel; n++)
	{
		for(uint32_t i = 0; i < nsamples; i++)
		{
			// clone audio in for in-place compatibility
			audio_in[n][i] = handle->audio_in[n][from + i];

			// generate silence
			audio_out[n][i] = 0.f;
		}
	}

	{
		dsp_t *dsp = handle->dsp[handle->play];
		if(dsp && dsp->instance)
		{
			computeCDSPInstance(dsp->instance, nsamples, (FAUSTFLOAT **)audio_in,
				(FAUSTFLOAT **)audio_out);
		}
	}

	if(handle->xfade_cur > 0)
	{
		for(uint32_t i = 0;
			(handle->xfade_cur > 0) && (i < nsamples);
			i++, handle->xfade_cur--)
		{
			const float gain = (float)handle->xfade_cur / handle->xfade_max;
			const float mul = handle->xfade_dst ? (1.f - gain) : gain;

			for(uint32_t n = 0; n < handle->nchannel; n++)
			{
				audio_out[n][i] *= mul;
			}
		}

		if(handle->xfade_cur == 0)
		{
			if(handle->xfade_dst == 0)
			{
				handle->play = !handle->play;
				handle->xfade_cur = handle->xfade_max;
				handle->xfade_dst = 1;

				dsp_t *dsp = handle->dsp[handle->play];
				for(uint32_t i = 0; i < dsp->ncntrls; i++)
				{
					_refresh_value(handle, i);
				}
			}
		}
	}
}

static LV2_Handle
instantiate(const LV2_Descriptor* descriptor, double rate,
	const char *bundle_path, const LV2_Feature *const *features)
{
	plughandle_t *handle = calloc(1, sizeof(plughandle_t));
	if(!handle)
	{
		return NULL;
	}
	mlock(handle, sizeof(plughandle_t));

	handle->nchannel = 1;
	if(!strcmp(descriptor->URI, MEPHISTO__stereo))
	{
		handle->nchannel = 2;
	}

	strncpy(handle->bundle_path, bundle_path, sizeof(handle->bundle_path));

	for(unsigned i=0; features[i]; i++)
	{
		if(!strcmp(features[i]->URI, LV2_URID__map))
		{
			handle->map = features[i]->data;
		}
		else if(!strcmp(features[i]->URI, LV2_WORKER__schedule))
		{
			handle->sched= features[i]->data;
		}
		else if(!strcmp(features[i]->URI, LV2_LOG__log))
		{
			handle->log = features[i]->data;
		}
	}

	if(!handle->map)
	{
		fprintf(stderr,
			"%s: Host does not support urid:map\n", descriptor->URI);
		free(handle);
		return NULL;
	}

	if(!handle->sched)
	{
		fprintf(stderr,
			"%s: Host does not support work:sched\n", descriptor->URI);
		free(handle);
		return NULL;
	}

	if(handle->log)
	{
		lv2_log_logger_init(&handle->logger, handle->map, handle->log);
	}

	lv2_atom_forge_init(&handle->forge, handle->map);

	if(!props_init(&handle->props, descriptor->URI,
		defs, MAX_NPROPS, &handle->state, &handle->stash,
		handle->map, handle))
	{
		fprintf(stderr, "failed to initialize property structure\n");
		free(handle);
		return NULL;
	}

	handle->to_worker = varchunk_new(BUF_SIZE, true);
	handle->xfade_max = 100e-3 * rate;
	handle->srate = rate;

	return handle;
}

static void
connect_port(LV2_Handle instance, uint32_t port, void *data)
{
	plughandle_t *handle = (plughandle_t *)instance;

	switch(port)
	{
		case 0:
			handle->control = (const LV2_Atom_Sequence *)data;
			break;
		case 1:
			handle->notify = (LV2_Atom_Sequence *)data;
			break;
		case 2:
			handle->audio_in[0] = (const float *)data;
			break;
		case 3:
			handle->audio_out[0] = (float *)data;
			break;
		case 4:
			handle->audio_in[1] = (const float *)data;
			break;
		case 5:
			handle->audio_out[1] = (float *)data;
			break;
		default:
			break;
	}
}

static void
run(LV2_Handle instance, uint32_t nsamples)
{
	plughandle_t *handle = instance;

	const uint32_t capacity = handle->notify->atom.size;
	LV2_Atom_Forge_Frame frame;
	lv2_atom_forge_set_buffer(&handle->forge, (uint8_t *)handle->notify, capacity);
	handle->ref = lv2_atom_forge_sequence_head(&handle->forge, &frame, 0);

	props_idle(&handle->props, &handle->forge, 0, &handle->ref);

	int64_t last_t = 0;
	LV2_ATOM_SEQUENCE_FOREACH(handle->control, ev)
	{
		const LV2_Atom_Object *obj = (const LV2_Atom_Object *)&ev->body;

		props_advance(&handle->props, &handle->forge, ev->time.frames, obj, &handle->ref);
		_play(handle, last_t, ev->time.frames);

		last_t = ev->time.frames;
	}

	_play(handle, last_t, nsamples);

	if(handle->ref)
	{
		lv2_atom_forge_pop(&handle->forge, &frame);
	}
	else
	{
		lv2_atom_sequence_clear(handle->notify);

		if(handle->log)
		{
			lv2_log_trace(&handle->logger, "forge buffer overflow\n");
		}
	}
}

static int
_meta_get_fmt_v(const char *val, const char *fmt, va_list arg)
{
	return vsscanf(val, fmt, arg);
}

static int
_meta_get_fmt(const char *val, const char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	const int res = _meta_get_fmt_v(val, fmt, arg);
	va_end(arg);

	return res;
}

static void
_meta_declare(void *iface, const char *key, const char *val)
{
	dsp_t *dsp = iface;
	plughandle_t *handle = dsp->handle;

	if(handle->log)
	{
		lv2_log_note(&handle->logger, "[%s] %s %s", __func__,
			key, val);
	}

	if(!strcmp(key, "options"))
	{
		if(_meta_get_fmt(val, "[nvoices=%"SCNu32"]", &dsp->nvoices) == 1)
		{
			if(dsp->nvoices == 0)
			{
				dsp->nvoices = MAX_VOICES;
			}
		}
	}
}

static cntrl_t *
_ui_next_cntrl(dsp_t *dsp)
{
	if(dsp->ncntrls < (NCONTROLS - 1))
	{
		return &dsp->cntrls[dsp->ncntrls++];
	}

	return NULL;
}

static void
_ui_open_tab_box(void* iface, const char* label)
{
	dsp_t *dsp = iface;
	plughandle_t *handle = dsp->handle;

	if(handle->log)
	{
		lv2_log_note(&handle->logger, "[%s] %s", __func__,
			label);
	}
}

static void
_ui_open_horizontal_box(void* iface, const char* label)
{
	dsp_t *dsp = iface;
	plughandle_t *handle = dsp->handle;

	if(handle->log)
	{
		lv2_log_note(&handle->logger, "[%s] %s", __func__,
			label);
	}
}

static void
_ui_open_vertical_box(void* iface, const char* label)
{
	dsp_t *dsp = iface;
	plughandle_t *handle = dsp->handle;

	if(handle->log)
	{
		lv2_log_note(&handle->logger, "[%s] %s", __func__,
			label);
	}
}

static void
_ui_close_box(void* iface)
{
	dsp_t *dsp = iface;
	plughandle_t *handle = dsp->handle;

	if(handle->log)
	{
		lv2_log_note(&handle->logger, "[%s]", __func__);
	}
}

static void
_ui_add_common(dsp_t *dsp, cntrl_t *cntrl, cntrl_type_t type, const char *label)
{
	cntrl->type = type;
	strncpy(cntrl->label, label, sizeof(cntrl->label));

	if(strstr(label, "freq"))
	{
		dsp->has_freq = true;
	}
	else if(strstr(label, "gain"))
	{
		dsp->has_gain = true;
	}
	else if(strstr(label, "gate"))
	{
		dsp->has_gate= true;
	}
}

static void
_ui_add_button(void* iface, const char* label, FAUSTFLOAT* zone)
{
	dsp_t *dsp = iface;
	plughandle_t *handle = dsp->handle;

	if(handle->log)
	{
		lv2_log_note(&handle->logger, "[%s] %s %f", __func__,
			label, *zone);
	}

	cntrl_t *cntrl = _ui_next_cntrl(dsp);
	if(!cntrl)
	{
		return;
	}

	_ui_add_common(dsp, cntrl, CNTRL_BUTTON, label);
	cntrl->button.zone = zone;
}

static void
_ui_add_check_button(void* iface, const char* label, FAUSTFLOAT* zone)
{
	dsp_t *dsp = iface;
	plughandle_t *handle = dsp->handle;

	if(handle->log)
	{
		lv2_log_note(&handle->logger, "[%s] %s %f", __func__,
			label, *zone);
	}

	cntrl_t *cntrl = _ui_next_cntrl(dsp);
	if(!cntrl)
	{
		return;
	}

	_ui_add_common(dsp, cntrl, CNTRL_CHECK_BUTTON, label);
	cntrl->check_button.zone = zone;
}

static void
_ui_add_vertical_slider(void* iface, const char* label, FAUSTFLOAT* zone,
	FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step)
{
	dsp_t *dsp = iface;
	plughandle_t *handle = dsp->handle;

	if(handle->log)
	{
		lv2_log_note(&handle->logger, "[%s] %s %f %f %f %f %f", __func__,
			label, *zone, init, min, max, step);
	}

	cntrl_t *cntrl = _ui_next_cntrl(dsp);
	if(!cntrl)
	{
		return;
	}

	_ui_add_common(dsp, cntrl, CNTRL_VERTICAL_SLIDER, label);
	cntrl->vertical_slider.zone = zone;
	cntrl->vertical_slider.init = init;
	cntrl->vertical_slider.min = min;
	cntrl->vertical_slider.max = max;
	cntrl->vertical_slider.ran = max - min;
	cntrl->vertical_slider.step = step;
}

static void
_ui_add_horizontal_slider(void* iface, const char* label, FAUSTFLOAT* zone,
	FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step)
{
	dsp_t *dsp = iface;
	plughandle_t *handle = dsp->handle;

	if(handle->log)
	{
		lv2_log_note(&handle->logger, "[%s] %s %f %f %f %f %f", __func__,
			label, *zone, init, min, max, step);
	}

	cntrl_t *cntrl = _ui_next_cntrl(dsp);
	if(!cntrl)
	{
		return;
	}

	_ui_add_common(dsp, cntrl, CNTRL_HORIZONTAL_SLIDER, label);
	cntrl->horizontal_slider.zone = zone;
	cntrl->horizontal_slider.init = init;
	cntrl->horizontal_slider.min = min;
	cntrl->horizontal_slider.max = max;
	cntrl->horizontal_slider.ran = max - min;
	cntrl->horizontal_slider.step = step;
}

static void
_ui_add_num_entry(void* iface, const char* label, FAUSTFLOAT* zone,
	FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step)
{
	dsp_t *dsp = iface;
	plughandle_t *handle = dsp->handle;

	if(handle->log)
	{
		lv2_log_note(&handle->logger, "[%s] %s %f %f %f %f %f", __func__,
			label, *zone, init, min, max, step);
	}

	cntrl_t *cntrl = _ui_next_cntrl(dsp);
	if(!cntrl)
	{
		return;
	}

	_ui_add_common(dsp, cntrl, CNTRL_NUM_ENTRY, label);
	cntrl->num_entry.zone = zone;
	cntrl->num_entry.init = init;
	cntrl->num_entry.min = min;
	cntrl->num_entry.max = max;
	cntrl->num_entry.ran = max - min;
	cntrl->num_entry.step = step;
}

static void
_ui_add_horizontal_bargraph(void* iface, const char* label, FAUSTFLOAT* zone,
	FAUSTFLOAT min, FAUSTFLOAT max)
{
	dsp_t *dsp = iface;
	plughandle_t *handle = dsp->handle;

	if(handle->log)
	{
		lv2_log_note(&handle->logger, "[%s] %s %f %f %f", __func__,
			label, *zone, min, max);
	}

	cntrl_t *cntrl = _ui_next_cntrl(dsp);
	if(!cntrl)
	{
		return;
	}

	_ui_add_common(dsp, cntrl, CNTRL_HORIZONTAL_BARGRAPH, label);
	cntrl->horizontal_bargraph.zone = zone;
	cntrl->horizontal_bargraph.min = min;
	cntrl->horizontal_bargraph.max = max;
}

static void
_ui_add_vertical_bargraph(void* iface, const char* label, FAUSTFLOAT* zone,
	FAUSTFLOAT min, FAUSTFLOAT max)
{
	dsp_t *dsp = iface;
	plughandle_t *handle = dsp->handle;

	if(handle->log)
	{
		lv2_log_note(&handle->logger, "[%s] %s %f %f %f", __func__,
			label, *zone, min, max);
	}

	cntrl_t *cntrl = _ui_next_cntrl(dsp);
	if(!cntrl)
	{
		return;
	}

	_ui_add_common(dsp, cntrl, CNTRL_VERTICAL_BARGRAPH, label);
	cntrl->vertical_bargraph.zone = zone;
	cntrl->vertical_bargraph.min = min;
	cntrl->vertical_bargraph.max = max;
}

static void
_ui_add_sound_file(void* iface, const char* label, const char* filename,
	struct Soundfile** sf_zone __attribute__((unused)))
{
	dsp_t *dsp = iface;
	plughandle_t *handle = dsp->handle;

	if(handle->log)
	{
		lv2_log_note(&handle->logger, "[%s] %s %s", __func__,
			label, filename);
	}

	cntrl_t *cntrl = _ui_next_cntrl(dsp);
	if(!cntrl)
	{
		return;
	}

	_ui_add_common(dsp, cntrl, CNTRL_SOUND_FILE, label);
	//FIXME
}

static void
_ui_declare(void* iface, FAUSTFLOAT* zone, const char* key, const char* value)
{
	dsp_t *dsp = iface;
	plughandle_t *handle = dsp->handle;

	if(handle->log)
	{
		lv2_log_note(&handle->logger, "[%s] %s %s", __func__,
			key, value);
	}

	//FIXME
	(void)zone;
}

static int
_meta_init(dsp_t *dsp)
{
	MetaGlue *glue = &dsp->meta_glue;

	glue->metaInterface = dsp;

	glue->declare = _meta_declare;

	dsp->nvoices = 1; // assume we're a filter by default

	metadataCDSPInstance(dsp->instance, glue);

	return 0;
}

static int
_ui_init(dsp_t *dsp)
{
	UIGlue *glue = &dsp->ui_glue;

	glue->uiInterface = dsp;

	glue->openTabBox = _ui_open_tab_box;
	glue->openHorizontalBox = _ui_open_horizontal_box;
	glue->openVerticalBox = _ui_open_vertical_box;
	glue->closeBox = _ui_close_box;
	glue->addButton = _ui_add_button;
	glue->addCheckButton = _ui_add_check_button;
	glue->addVerticalSlider = _ui_add_vertical_slider;
	glue->addHorizontalSlider = _ui_add_horizontal_slider;
	glue->addNumEntry = _ui_add_num_entry;
	glue->addHorizontalBargraph = _ui_add_horizontal_bargraph;
	glue->addVerticalBargraph = _ui_add_vertical_bargraph;
	glue->addSoundFile = _ui_add_sound_file;
	glue->declare = _ui_declare;

	buildUserInterfaceCDSPInstance(dsp->instance, glue);

	return 0;
}

static int
_dsp_init(plughandle_t *handle, dsp_t *dsp, const char *code)
{
#define ARGC 5
	char err [4096];
	const char *argv [ARGC] = {
		"-I", "/usr/share/faust", //FIXME
		"-vec",
		"-lv", "1"
	};

	dsp->handle = handle;

	pthread_mutex_lock(&lock);

	dsp->factory = createCDSPFactoryFromString("mephisto", code, ARGC, argv, "", err, -1);
	if(!dsp->factory)
	{
		if(handle->log)
		{
			lv2_log_error(&handle->logger, "[%s] %s", __func__, err);
		}

		goto fail;
	}

	dsp->instance = createCDSPInstance(dsp->factory);
	if(!dsp->instance)
	{
		if(handle->log)
		{
			lv2_log_error(&handle->logger, "[%s] instance creation failed", __func__);
		}

		deleteCDSPFactory(dsp->factory);
		goto fail;
	}

	instanceInitCDSPInstance(dsp->instance, handle->srate);

	dsp->nins = getNumInputsCDSPInstance(dsp->instance);
	dsp->nouts = getNumInputsCDSPInstance(dsp->instance);

	if(_meta_init(dsp) != 0)
	{
		if(handle->log)
		{
			lv2_log_error(&handle->logger, "[%s] meta creation failed", __func__);
		}

		deleteCDSPFactory(dsp->factory);
		goto fail;
	}

	if(_ui_init(dsp) != 0)
	{
		if(handle->log)
		{
			lv2_log_error(&handle->logger, "[%s] ui creation failed", __func__);
		}

		deleteCDSPFactory(dsp->factory);
		goto fail;
	}

	if(dsp->nvoices > 1)
	{
		if(handle->log)
		{
			lv2_log_note(&handle->logger, "[%s] is an instrument (%u) ", __func__,
				dsp->nvoices);
		}

		//FIXME clone instance per voice
	}

	if(handle->log)
	{
		lv2_log_note(&handle->logger, "[%s] compilation succeeded (%u:%u)",
			__func__, dsp->nins, dsp->nouts);
	}

	pthread_mutex_unlock(&lock);
	return 0;

fail:
	pthread_mutex_unlock(&lock);
	return 1;
#undef ARGC
}

static void
_dsp_deinit(plughandle_t *handle __attribute__((unused)), const dsp_t *dsp)
{
	pthread_mutex_lock(&lock);

	if(dsp && dsp->instance)
	{
		instanceClearCDSPInstance(dsp->instance);
		deleteCDSPInstance(dsp->instance);
	}

	if(dsp && dsp->factory)
	{
		deleteCDSPFactory(dsp->factory);
	}

	pthread_mutex_unlock(&lock);
}

static void
cleanup(LV2_Handle instance)
{
	plughandle_t *handle = instance;

	munlock(handle, sizeof(plughandle_t));
	varchunk_free(handle->to_worker);
	_dsp_deinit(handle, handle->dsp[0]);
	_dsp_deinit(handle, handle->dsp[1]);
	free(handle);
}

static LV2_State_Status
_state_save(LV2_Handle instance, LV2_State_Store_Function store,
	LV2_State_Handle state, uint32_t flags,
	const LV2_Feature *const *features)
{
	plughandle_t *handle = instance;

	return props_save(&handle->props, store, state, flags, features);
}

static LV2_State_Status
_state_restore(LV2_Handle instance, LV2_State_Retrieve_Function retrieve,
	LV2_State_Handle state, uint32_t flags,
	const LV2_Feature *const *features)
{
	plughandle_t *handle = instance;

	return props_restore(&handle->props, retrieve, state, flags, features);
}

static const LV2_State_Interface state_iface = {
	.save = _state_save,
	.restore = _state_restore
};

// non-rt thread
static LV2_Worker_Status
_work(LV2_Handle instance,
	LV2_Worker_Respond_Function respond,
	LV2_Worker_Respond_Handle target,
	uint32_t body_size,
	const void *body)
{
	plughandle_t *handle = instance;

	if(body_size != sizeof(job_t))
	{
		return LV2_WORKER_ERR_UNKNOWN;
	}

	const job_t *job = body;
	switch(job->type)
	{
		case JOB_TYPE_INIT:
		{
			size_t size;
			const char *code;
			while( (code= varchunk_read_request(handle->to_worker, &size)) )
			{
				dsp_t *dsp = calloc(1, sizeof(dsp_t));
				if(dsp && (_dsp_init(handle, dsp, code) == 0) )
				{
					const job_t job2 = {
						.type = JOB_TYPE_INIT,
						.dsp = dsp
					};

					respond(target, sizeof(job2), &job2);
				}

				varchunk_read_advance(handle->to_worker);
			}
		} break;
		case JOB_TYPE_DEINIT:
		{
			_dsp_deinit(handle, job->dsp);
		} break;
	}

	return LV2_WORKER_SUCCESS;
}

// rt-thread
static LV2_Worker_Status
_work_response(LV2_Handle instance, uint32_t size, const void *body)
{
	plughandle_t *handle = instance;

	if(size != sizeof(job_t))
	{
		return LV2_WORKER_ERR_UNKNOWN;
	}

	const job_t *job = body;
	switch(job->type)
	{
		case JOB_TYPE_INIT:
		{
			const job_t job2 = {
				.type = JOB_TYPE_DEINIT,
				.dsp = handle->dsp[!handle->play]
			};
			handle->sched->schedule_work(handle->sched->handle, sizeof(job2), &job2);

			handle->dsp[!handle->play] = job->dsp;
			handle->xfade_cur = handle->xfade_max;
			handle->xfade_dst = 0;
		} break;
		case JOB_TYPE_DEINIT:
		{
			// never reached
		} break;
	}

	return LV2_WORKER_SUCCESS;
}

// rt-thread
static LV2_Worker_Status
_end_run(LV2_Handle instance __attribute__((unused)))
{
	// do nothing

	return LV2_WORKER_SUCCESS;
}

static const LV2_Worker_Interface work_iface = {
	.work = _work,
	.work_response = _work_response,
	.end_run = _end_run
};

static const void*
extension_data(const char* uri)
{
	if(!strcmp(uri, LV2_STATE__interface))
	{
		return &state_iface;
	}
	else if(!strcmp(uri, LV2_WORKER__interface))
	{
		return &work_iface;
	}

	return NULL;
}

static const LV2_Descriptor mephisto_mono = {
	.URI						= MEPHISTO__mono,
	.instantiate		= instantiate,
	.connect_port		= connect_port,
	.activate				= NULL,
	.run						= run,
	.deactivate			= NULL,
	.cleanup				= cleanup,
	.extension_data	= extension_data
};

static const LV2_Descriptor mephisto_stereo = {
	.URI						= MEPHISTO__stereo,
	.instantiate		= instantiate,
	.connect_port		= connect_port,
	.activate				= NULL,
	.run						= run,
	.deactivate			= NULL,
	.cleanup				= cleanup,
	.extension_data	= extension_data
};

LV2_SYMBOL_EXPORT const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
	switch(index)
	{
		case 0:
			return &mephisto_mono;
		case 1:
			return &mephisto_stereo;
		default:
			return NULL;
	}
}
