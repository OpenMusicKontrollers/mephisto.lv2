/*
 * Copyright (c) 2019-2020 Hanspeter Portner (dev@open-music-kontrollers.ch)
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
#include <timely.h>
#include <varchunk.h>

#include <faust/dsp/llvm-c-dsp.h>

#define MAX_CHANNEL 8
#define MAX_VOICES 64

//#define MDI_MPE

typedef union _hash_t hash_t;
typedef struct _voice_t voice_t;
typedef struct _dsp_t dsp_t;
typedef struct _job_t job_t;
typedef struct _pos_t pos_t;
typedef struct _plughandle_t plughandle_t;

typedef struct _cntrl_vertical_slider_t cntrl_vertical_slider_t;
typedef struct _cntrl_horizontal_slider_t cntrl_horizontal_slider_t;
typedef struct _cntrl_num_entry_t cntrl_num_entry_t;
typedef struct _cntrl_horizontal_bargraph_t cntrl_horizontal_bargraph_t;
typedef struct _cntrl_vertical_bargraph_t cntrl_vertical_bargraph_t;
typedef struct _cntrl_sound_file_t cntrl_sound_file_t;

typedef struct _cntrl_t cntrl_t;

struct _cntrl_vertical_slider_t {
	float init;
	float min;
	float max;
	float ran;
	float step;
};

struct _cntrl_horizontal_slider_t {
	float init;
	float min;
	float max;
	float ran;
	float step;
};

struct _cntrl_num_entry_t {
	float init;
	float min;
	float max;
	float ran;
	float step;
};

struct _cntrl_horizontal_bargraph_t {
	float min;
	float max;
	float ran_1;
};

struct _cntrl_vertical_bargraph_t {
	float min;
	float max;
	float ran_1;
};

struct _cntrl_sound_file_t {
	uint32_t dummy; //FIXME
};

struct _cntrl_t {
	char label [LABEL_SIZE];
	cntrl_type_t type;
	bool readonly;
	float *zone;
	union {
		cntrl_vertical_slider_t vertical_slider;
		cntrl_horizontal_slider_t horizontal_slider;
		cntrl_num_entry_t num_entry;
		cntrl_horizontal_bargraph_t horizontal_bargraph;
		cntrl_vertical_bargraph_t vertical_bargraph;
		cntrl_sound_file_t sound_file;
	};
};

typedef enum _voice_state_t {
	VOICE_STATE_INACTIVE     = 0,
	VOICE_STATE_ACTIVE       = (1 << 0),
	VOICE_STATE_SUSTAIN      = (1 << 1)
} voice_state_t;

union _hash_t {
	struct {
		uint8_t chn;
		uint8_t key;
	};
	uint16_t id;
};

struct _pos_t {
	cntrl_t bar_beat;
	cntrl_t bar;
	cntrl_t beat_unit;
	cntrl_t beats_per_bar;
	cntrl_t beats_per_minute;
	cntrl_t frame;
	cntrl_t frames_per_second;
	cntrl_t speed;
};

struct _voice_t {
	llvm_dsp *instance;

	cntrl_t gate;
	cntrl_t gain;
	cntrl_t freq;
	cntrl_t pressure;
	cntrl_t timbre;
	cntrl_t d_freq;
	cntrl_t d_pressure;
	cntrl_t d_timbre;

	cntrl_t cntrls [NCONTROLS];

	pos_t pos;

	voice_state_t state;
	hash_t hash;
	bool retrigger;
};

struct _dsp_t {
	plughandle_t *handle;
	llvm_dsp_factory *factory;
	UIGlue ui_glue;
	MetaGlue meta_glue;
	uint32_t nins;
	uint32_t nouts;
	uint32_t nvoices;
	uint32_t cvoices;
	voice_t voices [MAX_VOICES];
	bool midi_on;
	bool time_on;
	bool is_instrument;
	timely_mask_t timely_mask;
	int32_t idx;
	uint32_t ivoice;
};

typedef enum _job_type_t {
	JOB_TYPE_INIT,
	JOB_TYPE_DEINIT,
	JOB_TYPE_ERROR_CLEAR,
	JOB_TYPE_ERROR_APPEND,
	JOB_TYPE_ERROR_FREE
} job_type_t;

struct _job_t {
	job_type_t type;
	union {
		dsp_t *dsp;
		char *error;
	};
};

struct _plughandle_t {
	LV2_URID_Map *map;
	LV2_Worker_Schedule *sched;
	LV2_Atom_Forge forge;
	LV2_Atom_Forge_Ref ref;

	LV2_Log_Log *log;
	LV2_Log_Logger logger;

	LV2_URID midi_MidiEvent;

	char dsp_dir [FILENAME_MAX];

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

	uint32_t srate;
	char bundle_path [PATH_MAX];

	LV2_URID mephisto_error;
	LV2_URID mephisto_control [NCONTROLS];
	LV2_URID mephisto_controlMin [NCONTROLS];
	LV2_URID mephisto_controlMax [NCONTROLS];
	LV2_URID mephisto_controlStep [NCONTROLS];
	LV2_URID mephisto_controlType [NCONTROLS];
	LV2_URID mephisto_controlLabel [NCONTROLS];

	struct {
		bool error;
		bool attributes;
	} dirty;

	bool play;
	dsp_t *dsp [2];

	uint16_t rpn_lsb [0x10];
	uint16_t rpn_msb [0x10];
	uint16_t data_lsb [0x10];
	uint16_t pressure [0x10];
	uint16_t timbre [0x10];
	float bend [0x10];
	float range [0x10];
	bool sustain [0x10];

	timely_t timely;

	int64_t offset;

	FAUSTFLOAT *faudio_in [MAX_CHANNEL];
	FAUSTFLOAT *faudio_out [MAX_CHANNEL];
};

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

#if 0
#	define DBG(HANDLE, FMT, ...) \
	if((HANDLE)->log) \
	{ \
		lv2_log_note(&((HANDLE)->logger), (FMT), __VA_ARGS__); \
	}
#else
#	define DBG(HANDLE, FMT, ...) \
	if(!(HANDLE)) \
	{ \
		lv2_log_note(&((HANDLE)->logger), (FMT), __VA_ARGS__); \
	}
#endif

static inline voice_t *
_voice_begin(dsp_t *dsp)
{
	return dsp->voices;
}

static inline bool
_voice_not_end(dsp_t *dsp, voice_t *voice)
{
	const uint32_t voice_offset = voice - dsp->voices;

	return voice_offset < dsp->nvoices;
}

static inline voice_t *
_voice_next(voice_t *voice)
{
	return voice + 1;
}

#define VOICE_FOREACH(DSP, VOICE) \
	for(voice_t *(VOICE) = _voice_begin((DSP)); \
		_voice_not_end((DSP), (VOICE)); \
		(VOICE) = _voice_next((VOICE)))

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
_cntrl_refresh_value_abs(cntrl_t *cntrl, float val)
{
	if(cntrl->zone)
	{
		*cntrl->zone = val;
	}
}

static float
_cntrl_get_value_abs(cntrl_t *cntrl)
{
	if(cntrl->zone)
	{
		return *cntrl->zone;
	}

	return 0.f;
}

static float
_cntrl_get_value_rel(cntrl_t *cntrl)
{
	switch(cntrl->type)
	{
		case CNTRL_VERTICAL_BARGRAPH:
		{
			if(cntrl->zone)
			{
				return (*cntrl->zone - cntrl->vertical_bargraph.min) * cntrl->vertical_bargraph.ran_1;
			}
		} break;
		case CNTRL_HORIZONTAL_BARGRAPH:
		{
			if(cntrl->zone)
			{
				return (*cntrl->zone - cntrl->horizontal_bargraph.min) * cntrl->horizontal_bargraph.ran_1;
			}
		} break;

		case CNTRL_NONE:
			// fall-through
		case CNTRL_BUTTON:
			// fall-through
		case CNTRL_CHECK_BUTTON:
			// fall-through
		case CNTRL_VERTICAL_SLIDER:
			// fall-through
		case CNTRL_HORIZONTAL_SLIDER:
			// fall-through
		case CNTRL_NUM_ENTRY:
			// fall-through
		case CNTRL_SOUND_FILE:
			// fall-through
		{
			// nothing to do
		} break;
	}

	return 0.f;
}

static void
_cntrl_refresh_value_rel(cntrl_t *cntrl, float val)
{
	switch(cntrl->type)
	{
		case CNTRL_NONE:
		{
			// do nothing
		} break;
		case CNTRL_BUTTON:
		{
			val = val > 0.5f
				? 1.f
				: 0.0;

			if(cntrl->zone)
			{
				*cntrl->zone = val;
			}
		} break;
		case CNTRL_CHECK_BUTTON:
		{
			val = val > 0.5f
				? 1.f
				: 0.0;

			if(cntrl->zone)
			{
				*cntrl->zone = val;
			}
		} break;
		case CNTRL_VERTICAL_SLIDER:
		{
			val = val * cntrl->vertical_slider.ran
				+ cntrl->vertical_slider.min;

			if(cntrl->zone)
			{
				*cntrl->zone = val;
			}
		} break;
		case CNTRL_HORIZONTAL_SLIDER:
		{
			val = val * cntrl->horizontal_slider.ran
				+ cntrl->horizontal_slider.min;

			if(cntrl->zone)
			{
				*cntrl->zone = val;
			}
		} break;
		case CNTRL_NUM_ENTRY:
		{
			val = val * cntrl->num_entry.ran
				+ cntrl->num_entry.min;

			if(cntrl->zone)
			{
				*cntrl->zone = val;
			}
		} break;
		case CNTRL_SOUND_FILE:
		{
			//FIXME
		} break;

		case CNTRL_HORIZONTAL_BARGRAPH:
			// fall-through
		case CNTRL_VERTICAL_BARGRAPH:
		{
			// nothing to do
		} break;
	}
}

static void
_cntrl_refresh_attributes(cntrl_t *cntrl, float *min, float *max, float *step,
	int32_t *type, char *label)
{
	*type = cntrl->type;
	strncpy(label, cntrl->label, LABEL_SIZE - 1);

	switch(cntrl->type)
	{
		case CNTRL_BUTTON:
			// fall-through
		case CNTRL_CHECK_BUTTON:
		{
			*min = 0.f;
			*max = 1.f;
			*step = 1.f;
		} break;

		case CNTRL_VERTICAL_SLIDER:
		{
			*min = cntrl->vertical_slider.min;
			*max = cntrl->vertical_slider.max;
			*step = cntrl->vertical_slider.step;
		} break;
		case CNTRL_HORIZONTAL_SLIDER:
		{
			*min = cntrl->horizontal_slider.min;
			*max = cntrl->horizontal_slider.max;
			*step = cntrl->horizontal_slider.step;
		} break;
		case CNTRL_NUM_ENTRY:
		{
			*min = cntrl->num_entry.min;
			*max = cntrl->num_entry.max;
			*step = cntrl->num_entry.step;
		} break;
		case CNTRL_HORIZONTAL_BARGRAPH:
		{
			*min = cntrl->horizontal_bargraph.min;
			*max = cntrl->horizontal_bargraph.max;
			*step = 1.f;
		} break;
		case CNTRL_VERTICAL_BARGRAPH:
		{
			*min = cntrl->vertical_bargraph.min;
			*max = cntrl->vertical_bargraph.max;
			*step = 1.f;
		} break;

		case CNTRL_NONE:
			// fall-through
		case CNTRL_SOUND_FILE:
		{
			// do nothing
		} break;
	}
}

static void
_refresh_attributes(plughandle_t *handle, uint32_t idx)
{
	float min = 0.f;
	float max = 0.f;
	float step = 0.f;
	int32_t type = CNTRL_NONE;
	char label [LABEL_SIZE] = "";

	dsp_t *dsp = handle->dsp[!handle->play];

	if(!dsp)
	{
		return;
	}

	VOICE_FOREACH(dsp, voice)
	{
		cntrl_t *cntrl = &voice->cntrls[idx];

		if(cntrl->type == CNTRL_NONE)
		{
			continue;
		}

		_cntrl_refresh_attributes(cntrl, &min, &max, &step, &type, label);
	}


	props_impl_t *impl = NULL;

	impl = _props_impl_get(&handle->props, handle->mephisto_controlMin[idx]);
	if(impl)
	{
		_props_impl_set(&handle->props, impl, handle->forge.Float, sizeof(float), &min);
	}

	impl = _props_impl_get(&handle->props, handle->mephisto_controlMax[idx]);
	if(impl)
	{
		_props_impl_set(&handle->props, impl, handle->forge.Float, sizeof(float), &max);
	}

	impl = _props_impl_get(&handle->props, handle->mephisto_controlStep[idx]);
	if(impl)
	{
		_props_impl_set(&handle->props, impl, handle->forge.Float, sizeof(float), &step);
	}

	impl = _props_impl_get(&handle->props, handle->mephisto_controlType[idx]);
	if(impl)
	{
		_props_impl_set(&handle->props, impl, handle->forge.Int, sizeof(int32_t), &type);
	}

	impl = _props_impl_get(&handle->props, handle->mephisto_controlLabel[idx]);
	if(impl)
	{
		_props_impl_set(&handle->props, impl, handle->forge.String, strlen(label) + 1, label);
	}
}

static void
_refresh_value(plughandle_t *handle, uint32_t idx)
{
	const bool off [2] = {
		handle->play,
		!handle->play
	};

	const float val = handle->state.control[idx];

	for(uint32_t d = 0; d < 2; d++)
	{
		dsp_t *dsp = handle->dsp[off[d]];

		if(!dsp)
		{
			continue;
		}

		VOICE_FOREACH(dsp, voice)
		{
			cntrl_t *cntrl = &voice->cntrls[idx];

			if(cntrl->type == CNTRL_NONE)
			{
				continue;
			}

			_cntrl_refresh_value_rel(cntrl, val);
		}
	}
}

static void
_refresh_time_position(plughandle_t *handle)
{
	const bool off [2] = {
		handle->play,
		!handle->play
	};

	for(uint32_t d = 0; d < 2; d++)
	{
		dsp_t *dsp = handle->dsp[off[d]];

		if(!dsp || !dsp->time_on)
		{
			continue;
		}

		VOICE_FOREACH(dsp, voice)
		{
			_cntrl_refresh_value_abs(&voice->pos.bar_beat,
				TIMELY_BAR_BEAT(&handle->timely));
			_cntrl_refresh_value_abs(&voice->pos.bar,
				TIMELY_BAR(&handle->timely));
			_cntrl_refresh_value_abs(&voice->pos.beat_unit,
				TIMELY_BEAT_UNIT(&handle->timely));
			_cntrl_refresh_value_abs(&voice->pos.beats_per_bar,
				TIMELY_BEATS_PER_BAR(&handle->timely));
			_cntrl_refresh_value_abs(&voice->pos.beats_per_minute,
				TIMELY_BEATS_PER_MINUTE(&handle->timely));
			_cntrl_refresh_value_abs(&voice->pos.frame,
				TIMELY_FRAME(&handle->timely));
			_cntrl_refresh_value_abs(&voice->pos.frames_per_second,
				TIMELY_FRAMES_PER_SECOND(&handle->timely));
			_cntrl_refresh_value_abs(&voice->pos.speed,
				TIMELY_SPEED(&handle->timely));
		}
	}
}

static void
_intercept_xfade_duration(void *data, int64_t frames __attribute__((unused)),
	props_impl_t *impl __attribute__((unused)))
{
	plughandle_t *handle = data;

	handle->xfade_max = handle->srate * handle->state.xfade_dur / 1000;
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
	{
		.property = MEPHISTO__error,
		.access = LV2_PATCH__readable,
		.offset = offsetof(plugstate_t, error),
		.type = LV2_ATOM__String,
		.max_size = ERROR_SIZE
	},
	{
		.property = MEPHISTO__xfadeDuration,
		.offset = offsetof(plugstate_t, xfade_dur),
		.type = LV2_ATOM__Int,
		.event_cb = _intercept_xfade_duration
	},
	{
		.property = MEPHISTO__fontHeight,
		.offset = offsetof(plugstate_t, font_height),
		.type = LV2_ATOM__Int
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

	FAUSTFLOAT *audio_in [32];
	FAUSTFLOAT *audio_out [32];

	// libFAUST does not check for valid audio buffer pointers,
	// so we must provide a plethora (32) of them in case the user makes a patch
	// with more than 2 ins and outs
	for(uint32_t i = 0; i < 32; i++)
	{
		const uint32_t idx = i % handle->nchannel;

		audio_in[i] = handle->faudio_in[idx];
		audio_out[i] = handle->faudio_out[idx];
	}

	// fill audio in, clear audio out
	for(uint32_t n = 0; n < handle->nchannel; n++)
	{
		for(uint32_t i = 0; i < nsamples; i++)
		{
			audio_in[n][i] = handle->audio_in[n][from + i];
			handle->audio_out[n][from + i] = 0.f;
		}
	}

	const bool off [2] = {
		handle->play,
		!handle->play
	};

	for(uint32_t d = 0; d < 2; d++)
	{
		dsp_t *dsp = handle->dsp[off[d]];

		if(!dsp)
		{
			continue;
		}

		float gain = 1.f;

		if(handle->xfade_cur > 0)
		{
			const float t = 2.f * handle->xfade_cur / handle->xfade_max - 1.f;

			if(d == 0) // fade-out
			{
				gain = sqrtf(0.5f * (1.f + t) );
			}
			else // fade-in
			{
				gain = sqrtf(0.5f * (1.f - t) );
			}
		}
		else
		{
			if(d == 1)
			{
				continue; // skip this
			}
		}

		VOICE_FOREACH(dsp, voice)
		{
			{
				if(voice->retrigger)
				{
					_cntrl_refresh_value_abs(&voice->gate, 0.f);
					computeCDSPInstance(voice->instance, 1, audio_in, audio_out);
					_cntrl_refresh_value_abs(&voice->gate, 1.f);

					voice->retrigger = false;
				}

				computeCDSPInstance(voice->instance, nsamples, audio_in, audio_out);

				// add to master out
				for(uint32_t n = 0; n < handle->nchannel; n++)
				{
					for(uint32_t i = 0; i < nsamples; i++)
					{
						handle->audio_out[n][from + i] += gain * audio_out[n][i];
					}
				}
			}
		}
	}

	if(handle->xfade_cur > 0)
	{
		if(nsamples >= handle->xfade_cur)
		{
			handle->xfade_cur = 0;

			// switch dsps
			handle->play = !handle->play;
		}
		else
		{
			handle->xfade_cur -= nsamples;
		}
	}
}

static void
_timely_cb(timely_t *timely __attribute__((unused)),
	int64_t frames __attribute__((unused)), LV2_URID type __attribute__((unused)),
	void *data __attribute__((unused)))
{
	// nothing to do
}

static int
_dsp_dir(char *buf, size_t len)
{
	FILE *fin = popen("faust -dspdir", "r");
	if(!fin)
	{
		return -1;
	}

	const size_t sz = fread(buf, 1, len, fin);
	pclose(fin);

	if(sz == 0)
	{
		return 1;
	}

	buf[sz] = '\0';

	char *esc = strpbrk(buf, "\n\r");
	if(esc)
	{
		*esc = '\0';
	}

	return 0;
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
	if(  !strcmp(descriptor->URI, MEPHISTO__audio_2x2)
		|| !strcmp(descriptor->URI, MEPHISTO__cv_2x2) )
	{
		handle->nchannel = 2;
	}
	else if(!strcmp(descriptor->URI, MEPHISTO__audio_4x4)
		|| !strcmp(descriptor->URI, MEPHISTO__cv_4x4) )
	{
		handle->nchannel = 4;
	}
	else if(!strcmp(descriptor->URI, MEPHISTO__audio_8x8)
		|| !strcmp(descriptor->URI, MEPHISTO__cv_8x8) )
	{
		handle->nchannel = 8;
	}

	strncpy(handle->bundle_path, bundle_path, sizeof(handle->bundle_path) - 1);

	LV2_Options_Option *opts = NULL;
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
		else if(!strcmp(features[i]->URI, LV2_OPTIONS__options))
		{
			opts = features[i]->data;
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

	if(_dsp_dir(handle->dsp_dir, sizeof(handle->dsp_dir)) != 0)
	{
		fprintf(stderr,
			"%s: failed to get FAUST DSP directory\n", descriptor->URI);
		free(handle);
		return NULL;
	}

	if(handle->log)
	{
		lv2_log_logger_init(&handle->logger, handle->map, handle->log);
	}

	lv2_atom_forge_init(&handle->forge, handle->map);

	handle->midi_MidiEvent = handle->map->map(handle->map->handle,
		LV2_MIDI__MidiEvent);
	const LV2_URID bufsz_maxBlockLength = handle->map->map(handle->map->handle,
		LV2_BUF_SIZE__maxBlockLength);

	int32_t max_block_length = 0;
	for(LV2_Options_Option *opt = opts;
		(opt->key != 0) && (opt->value != NULL);
		opt++)
	{
		if(  (opt->key == bufsz_maxBlockLength)
			&& (opt->size == sizeof(int32_t))
			&& (opt->type == handle->forge.Int) )
		{
			max_block_length = *(const int32_t *)opt->value;

			break;
		}
	}

	if(max_block_length == 0)
	{
		fprintf(stderr,
			"%s: Host does not provide bufsz:maxBlockLength\n", descriptor->URI);
		free(handle);
		return NULL;
	}

	const size_t buflen = sizeof(FAUSTFLOAT) * max_block_length;
	for(uint32_t n = 0; n < handle->nchannel; n++)
	{
		handle->faudio_in[n] = malloc(buflen);
		handle->faudio_out[n] = malloc(buflen);

		if(!handle->faudio_in[n] || !handle->faudio_out[n])
		{
			for(n = 0; n < handle->nchannel; n++)
			{
				free(handle->faudio_in[n]);
				free(handle->faudio_out[n]);
			}

			free(handle);
			return NULL;
		}
	}

	if(!props_init(&handle->props, descriptor->URI,
		defs, MAX_NPROPS, &handle->state, &handle->stash,
		handle->map, handle))
	{
		fprintf(stderr, "failed to initialize property structure\n");
		free(handle);
		return NULL;
	}

	handle->mephisto_error = props_map(&handle->props, MEPHISTO__error);

	handle->mephisto_control[0] = props_map(&handle->props, MEPHISTO__control_1);
	handle->mephisto_control[1] = props_map(&handle->props, MEPHISTO__control_2);
	handle->mephisto_control[2] = props_map(&handle->props, MEPHISTO__control_3);
	handle->mephisto_control[3] = props_map(&handle->props, MEPHISTO__control_4);
	handle->mephisto_control[4] = props_map(&handle->props, MEPHISTO__control_5);
	handle->mephisto_control[5] = props_map(&handle->props, MEPHISTO__control_6);
	handle->mephisto_control[6] = props_map(&handle->props, MEPHISTO__control_7);
	handle->mephisto_control[7] = props_map(&handle->props, MEPHISTO__control_8);
	handle->mephisto_control[8] = props_map(&handle->props, MEPHISTO__control_9);
	handle->mephisto_control[9] = props_map(&handle->props, MEPHISTO__control_10);
	handle->mephisto_control[10] = props_map(&handle->props, MEPHISTO__control_11);
	handle->mephisto_control[11] = props_map(&handle->props, MEPHISTO__control_12);
	handle->mephisto_control[12] = props_map(&handle->props, MEPHISTO__control_13);
	handle->mephisto_control[13] = props_map(&handle->props, MEPHISTO__control_14);
	handle->mephisto_control[14] = props_map(&handle->props, MEPHISTO__control_15);
	handle->mephisto_control[15] = props_map(&handle->props, MEPHISTO__control_16);

	handle->mephisto_controlMin[0] = props_map(&handle->props, MEPHISTO__controlMin_1);
	handle->mephisto_controlMin[1] = props_map(&handle->props, MEPHISTO__controlMin_2);
	handle->mephisto_controlMin[2] = props_map(&handle->props, MEPHISTO__controlMin_3);
	handle->mephisto_controlMin[3] = props_map(&handle->props, MEPHISTO__controlMin_4);
	handle->mephisto_controlMin[4] = props_map(&handle->props, MEPHISTO__controlMin_5);
	handle->mephisto_controlMin[5] = props_map(&handle->props, MEPHISTO__controlMin_6);
	handle->mephisto_controlMin[6] = props_map(&handle->props, MEPHISTO__controlMin_7);
	handle->mephisto_controlMin[7] = props_map(&handle->props, MEPHISTO__controlMin_8);
	handle->mephisto_controlMin[8] = props_map(&handle->props, MEPHISTO__controlMin_9);
	handle->mephisto_controlMin[9] = props_map(&handle->props, MEPHISTO__controlMin_10);
	handle->mephisto_controlMin[10] = props_map(&handle->props, MEPHISTO__controlMin_11);
	handle->mephisto_controlMin[11] = props_map(&handle->props, MEPHISTO__controlMin_12);
	handle->mephisto_controlMin[12] = props_map(&handle->props, MEPHISTO__controlMin_13);
	handle->mephisto_controlMin[13] = props_map(&handle->props, MEPHISTO__controlMin_14);
	handle->mephisto_controlMin[14] = props_map(&handle->props, MEPHISTO__controlMin_15);
	handle->mephisto_controlMin[15] = props_map(&handle->props, MEPHISTO__controlMin_16);

	handle->mephisto_controlMax[0] = props_map(&handle->props, MEPHISTO__controlMax_1);
	handle->mephisto_controlMax[1] = props_map(&handle->props, MEPHISTO__controlMax_2);
	handle->mephisto_controlMax[2] = props_map(&handle->props, MEPHISTO__controlMax_3);
	handle->mephisto_controlMax[3] = props_map(&handle->props, MEPHISTO__controlMax_4);
	handle->mephisto_controlMax[4] = props_map(&handle->props, MEPHISTO__controlMax_5);
	handle->mephisto_controlMax[5] = props_map(&handle->props, MEPHISTO__controlMax_6);
	handle->mephisto_controlMax[6] = props_map(&handle->props, MEPHISTO__controlMax_7);
	handle->mephisto_controlMax[7] = props_map(&handle->props, MEPHISTO__controlMax_8);
	handle->mephisto_controlMax[8] = props_map(&handle->props, MEPHISTO__controlMax_9);
	handle->mephisto_controlMax[9] = props_map(&handle->props, MEPHISTO__controlMax_10);
	handle->mephisto_controlMax[10] = props_map(&handle->props, MEPHISTO__controlMax_11);
	handle->mephisto_controlMax[11] = props_map(&handle->props, MEPHISTO__controlMax_12);
	handle->mephisto_controlMax[12] = props_map(&handle->props, MEPHISTO__controlMax_13);
	handle->mephisto_controlMax[13] = props_map(&handle->props, MEPHISTO__controlMax_14);
	handle->mephisto_controlMax[14] = props_map(&handle->props, MEPHISTO__controlMax_15);
	handle->mephisto_controlMax[15] = props_map(&handle->props, MEPHISTO__controlMax_16);

	handle->mephisto_controlStep[0] = props_map(&handle->props, MEPHISTO__controlStep_1);
	handle->mephisto_controlStep[1] = props_map(&handle->props, MEPHISTO__controlStep_2);
	handle->mephisto_controlStep[2] = props_map(&handle->props, MEPHISTO__controlStep_3);
	handle->mephisto_controlStep[3] = props_map(&handle->props, MEPHISTO__controlStep_4);
	handle->mephisto_controlStep[4] = props_map(&handle->props, MEPHISTO__controlStep_5);
	handle->mephisto_controlStep[5] = props_map(&handle->props, MEPHISTO__controlStep_6);
	handle->mephisto_controlStep[6] = props_map(&handle->props, MEPHISTO__controlStep_7);
	handle->mephisto_controlStep[7] = props_map(&handle->props, MEPHISTO__controlStep_8);
	handle->mephisto_controlStep[8] = props_map(&handle->props, MEPHISTO__controlStep_9);
	handle->mephisto_controlStep[9] = props_map(&handle->props, MEPHISTO__controlStep_10);
	handle->mephisto_controlStep[10] = props_map(&handle->props, MEPHISTO__controlStep_11);
	handle->mephisto_controlStep[11] = props_map(&handle->props, MEPHISTO__controlStep_12);
	handle->mephisto_controlStep[12] = props_map(&handle->props, MEPHISTO__controlStep_13);
	handle->mephisto_controlStep[13] = props_map(&handle->props, MEPHISTO__controlStep_14);
	handle->mephisto_controlStep[14] = props_map(&handle->props, MEPHISTO__controlStep_15);
	handle->mephisto_controlStep[15] = props_map(&handle->props, MEPHISTO__controlStep_16);

	handle->mephisto_controlType[0] = props_map(&handle->props, MEPHISTO__controlType_1);
	handle->mephisto_controlType[1] = props_map(&handle->props, MEPHISTO__controlType_2);
	handle->mephisto_controlType[2] = props_map(&handle->props, MEPHISTO__controlType_3);
	handle->mephisto_controlType[3] = props_map(&handle->props, MEPHISTO__controlType_4);
	handle->mephisto_controlType[4] = props_map(&handle->props, MEPHISTO__controlType_5);
	handle->mephisto_controlType[5] = props_map(&handle->props, MEPHISTO__controlType_6);
	handle->mephisto_controlType[6] = props_map(&handle->props, MEPHISTO__controlType_7);
	handle->mephisto_controlType[7] = props_map(&handle->props, MEPHISTO__controlType_8);
	handle->mephisto_controlType[8] = props_map(&handle->props, MEPHISTO__controlType_9);
	handle->mephisto_controlType[9] = props_map(&handle->props, MEPHISTO__controlType_10);
	handle->mephisto_controlType[10] = props_map(&handle->props, MEPHISTO__controlType_11);
	handle->mephisto_controlType[11] = props_map(&handle->props, MEPHISTO__controlType_12);
	handle->mephisto_controlType[12] = props_map(&handle->props, MEPHISTO__controlType_13);
	handle->mephisto_controlType[13] = props_map(&handle->props, MEPHISTO__controlType_14);
	handle->mephisto_controlType[14] = props_map(&handle->props, MEPHISTO__controlType_15);
	handle->mephisto_controlType[15] = props_map(&handle->props, MEPHISTO__controlType_16);

	handle->mephisto_controlLabel[0] = props_map(&handle->props, MEPHISTO__controlLabel_1);
	handle->mephisto_controlLabel[1] = props_map(&handle->props, MEPHISTO__controlLabel_2);
	handle->mephisto_controlLabel[2] = props_map(&handle->props, MEPHISTO__controlLabel_3);
	handle->mephisto_controlLabel[3] = props_map(&handle->props, MEPHISTO__controlLabel_4);
	handle->mephisto_controlLabel[4] = props_map(&handle->props, MEPHISTO__controlLabel_5);
	handle->mephisto_controlLabel[5] = props_map(&handle->props, MEPHISTO__controlLabel_6);
	handle->mephisto_controlLabel[6] = props_map(&handle->props, MEPHISTO__controlLabel_7);
	handle->mephisto_controlLabel[7] = props_map(&handle->props, MEPHISTO__controlLabel_8);
	handle->mephisto_controlLabel[8] = props_map(&handle->props, MEPHISTO__controlLabel_9);
	handle->mephisto_controlLabel[9] = props_map(&handle->props, MEPHISTO__controlLabel_10);
	handle->mephisto_controlLabel[10] = props_map(&handle->props, MEPHISTO__controlLabel_11);
	handle->mephisto_controlLabel[11] = props_map(&handle->props, MEPHISTO__controlLabel_12);
	handle->mephisto_controlLabel[12] = props_map(&handle->props, MEPHISTO__controlLabel_13);
	handle->mephisto_controlLabel[13] = props_map(&handle->props, MEPHISTO__controlLabel_14);
	handle->mephisto_controlLabel[14] = props_map(&handle->props, MEPHISTO__controlLabel_15);
	handle->mephisto_controlLabel[15] = props_map(&handle->props, MEPHISTO__controlLabel_16);

	handle->to_worker = varchunk_new(BUF_SIZE, true);
	handle->srate = rate;

	for(uint32_t chn = 0; chn < 0x10; chn++)
	{
		handle->range[chn] = 48.f; // semitones
	}

	const timely_mask_t mask = 0;
	timely_init(&handle->timely, handle->map, rate, mask, _timely_cb, handle);
	timely_set_multiplier(&handle->timely, 1.f);

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

		case 6:
			handle->audio_in[2] = (const float *)data;
			break;
		case 7:
			handle->audio_out[2] = (float *)data;
			break;

		case 8:
			handle->audio_in[3] = (const float *)data;
			break;
		case 9:
			handle->audio_out[3] = (float *)data;
			break;

		case 10:
			handle->audio_in[4] = (const float *)data;
			break;
		case 11:
			handle->audio_out[4] = (float *)data;
			break;

		case 12:
			handle->audio_in[5] = (const float *)data;
			break;
		case 13:
			handle->audio_out[5] = (float *)data;
			break;

		case 14:
			handle->audio_in[6] = (const float *)data;
			break;
		case 15:
			handle->audio_out[6] = (float *)data;
			break;

		case 16:
			handle->audio_in[7] = (const float *)data;
			break;
		case 17:
			handle->audio_out[7] = (float *)data;
			break;

		default:
			break;
	}
}

static inline float
_midi2cps(float pitch)
{
	return exp2f( (pitch - 69.f) / 12.f) * 440.f;
}

static inline voice_t *
_next_available_voice(dsp_t *dsp)
{
	uint32_t i;

	for(i = 0, dsp->ivoice = (dsp->ivoice + i + 1) % dsp->nvoices;
		i < dsp->nvoices;
		i++, dsp->ivoice = (dsp->ivoice + i) % dsp->nvoices)
	{
		voice_t *voice = &dsp->voices[dsp->ivoice];

		if(voice->state == VOICE_STATE_INACTIVE)
		{
			return voice;
		}
	}

	for(i = 0, dsp->ivoice = (dsp->ivoice + i) % dsp->nvoices;
		i < dsp->nvoices;
		i++, dsp->ivoice = (dsp->ivoice + i) % dsp->nvoices)
	{
		voice_t *voice = &dsp->voices[dsp->ivoice];

		if(voice->state & VOICE_STATE_SUSTAIN)
		{
			return voice;
		}
	}

	for(i = 0, dsp->ivoice = (dsp->ivoice + i) % dsp->nvoices;
		i < dsp->nvoices;
		i++, dsp->ivoice = (dsp->ivoice + i) % dsp->nvoices)
	{
		voice_t *voice = &dsp->voices[dsp->ivoice];

		if(voice->state & VOICE_STATE_ACTIVE)
		{
			return voice;
		}
	}

	return NULL;
}

static inline voice_t *
_find_active_voice(dsp_t *dsp, const hash_t *hash)
{
	VOICE_FOREACH(dsp, voice)
	{
		if(  (voice->state == VOICE_STATE_ACTIVE)
			&& (voice->hash.id == hash->id) )
		{
			return voice;
		}
	}

	return NULL;
}

static inline void
_update_frequency(plughandle_t *handle, dsp_t *dsp, uint8_t chn)
{
	VOICE_FOREACH(dsp, voice)
	{
		if(voice->state & VOICE_STATE_ACTIVE)
		{
			if(voice->hash.chn == chn)
			{
				const float freq = _midi2cps(voice->hash.key
					+ handle->bend[chn]*handle->range[chn]);

				_cntrl_refresh_value_abs(&voice->freq, freq);
			}
		}
	}
}

static inline void
_update_pressure(plughandle_t *handle, dsp_t *dsp, uint8_t chn)
{
	VOICE_FOREACH(dsp, voice)
	{
		if(voice->state & VOICE_STATE_ACTIVE)
		{
			if(voice->hash.chn == chn)
			{
				const float pressure = handle->pressure[chn] * 0x1p-14;

				_cntrl_refresh_value_abs(&voice->pressure, pressure);
			}
		}
	}
}

static inline void
_update_timbre(plughandle_t *handle, dsp_t *dsp, uint8_t chn)
{
	VOICE_FOREACH(dsp, voice)
	{
		if(voice->state & VOICE_STATE_ACTIVE)
		{
			if(voice->hash.chn == chn)
			{
				const float timbre = handle->timbre[chn] * 0x1p-14;

				_cntrl_refresh_value_abs(&voice->timbre, timbre);
			}
		}
	}
}

static inline void
_voice_off(plughandle_t *handle, voice_t *voice)
{
	if(handle->sustain[voice->hash.chn])
	{
		voice->state |= VOICE_STATE_SUSTAIN;
	}
	else
	{
		_cntrl_refresh_value_abs(&voice->gate, 0.f);

		voice->state = VOICE_STATE_INACTIVE;
	}
}

static inline void
_voice_off_panic(voice_t *voice)
{
	_cntrl_refresh_value_abs(&voice->gate, 0.f);

	voice->state = VOICE_STATE_INACTIVE;
}

static inline void
_voice_off_force(voice_t *voice)
{
	_cntrl_refresh_value_abs(&voice->gate, 0.f);

	voice->state = VOICE_STATE_INACTIVE;
}

static void
_handle_midi_2(dsp_t *dsp,
	int64_t frames __attribute__((unused)), const uint8_t *msg)
{
	const uint8_t cmd = msg[0] & 0xf0;

	if(!dsp || !dsp->is_instrument || !dsp->midi_on)
	{
		return;
	}

	switch(cmd)
	{
		case LV2_MIDI_MSG_CONTROLLER:
		{
			const uint8_t ctr = msg[1];

			switch(ctr)
			{
				case LV2_MIDI_CTL_ALL_NOTES_OFF:
				{
					VOICE_FOREACH(dsp, voice)
					{
						_voice_off_panic(voice);
					}
				} break;
				case LV2_MIDI_CTL_ALL_SOUNDS_OFF:
				{
					VOICE_FOREACH(dsp, voice)
					{
						_voice_off_force(voice);
					}
				} break;
			}
		} break;
	}
}

static void
_handle_midi_3(plughandle_t *handle, dsp_t *dsp,
	int64_t frames __attribute__((unused)), const uint8_t *msg)
{
	const uint8_t cmd = msg[0] & 0xf0;
	const uint8_t chn = msg[0] & 0x0f;

	if(!dsp || !dsp->is_instrument || !dsp->midi_on)
	{
		return;
	}

	switch(cmd)
	{
		case LV2_MIDI_MSG_NOTE_ON:
		{
			const uint8_t key = msg[1];
			const uint8_t vel = msg[2];

			voice_t *voice = _next_available_voice(dsp);
			if(voice)
			{
				const float freq = _midi2cps((float)key
					+ handle->bend[chn]*handle->range[chn]);

				_cntrl_refresh_value_abs(&voice->freq, freq);
				_cntrl_refresh_value_abs(&voice->gain, vel * 0x1p-7);
				_cntrl_refresh_value_abs(&voice->gate, 0.f);

				voice->hash.key = key;
				voice->hash.chn = chn;
				voice->state = VOICE_STATE_ACTIVE;
				voice->retrigger = true;
			}
		} break;
		case LV2_MIDI_MSG_NOTE_OFF:
		{
			const uint8_t key = msg[1];

			const hash_t hash = {
				.key= key,
				.chn = chn
			};

			voice_t *voice = _find_active_voice(dsp, &hash);

			if(voice)
			{
				_voice_off(handle, voice);
			}
		} break;
		case LV2_MIDI_MSG_NOTE_PRESSURE:
		{
			const uint8_t key = msg[1];
			const uint8_t pre = msg[2];

			const hash_t hash = {
				.key= key,
				.chn = chn
			};

			voice_t *voice = _find_active_voice(dsp, &hash);

			if(voice)
			{
				_cntrl_refresh_value_abs(&voice->pressure, pre * 0x1p-7);
			}
		} break;
		case LV2_MIDI_MSG_BENDER:
		{
			const uint8_t lsb = msg[1];
			const uint8_t msb = msg[2];
			const int16_t bend = (msb << 7) | lsb;

			handle->bend[chn] = (bend - 0x2000) * 0x1p-13;
			_update_frequency(handle, dsp, chn);
		} break;
		case LV2_MIDI_MSG_CONTROLLER:
		{
			const uint8_t ctr = msg[1];
			const uint8_t val = msg[2];

			switch(ctr)
			{
				case LV2_MIDI_CTL_SUSTAIN:
				{
					handle->sustain[chn] = (val > 0x3f)
						? true
						: false;

					if(handle->sustain[chn] == false)
					{
						VOICE_FOREACH(dsp, voice)
						{
							if(  (voice->hash.chn == chn)
								&& (voice->state & VOICE_STATE_SUSTAIN) )
							{
								_voice_off(handle, voice);
							}
						}
					}
				} break;
				case LV2_MIDI_CTL_RPN_LSB:
				{
					handle->rpn_lsb[chn] = val;
				} break;
				case LV2_MIDI_CTL_RPN_MSB:
				{
					handle->rpn_msb[chn] = val;
				} break;
				case LV2_MIDI_CTL_LSB_DATA_ENTRY:
				{
					handle->data_lsb[chn] = val;
				} break;
				case LV2_MIDI_CTL_MSB_DATA_ENTRY:
				{
					// pitch-bend range
					if( (handle->rpn_msb[chn] == 0x0) && (handle->rpn_lsb[chn] == 0x0) )
					{
						const uint8_t semi = val;
						const uint8_t cent = handle->data_lsb[chn];

						handle->range[chn] = (float)semi + cent*0.01f;
						_update_frequency(handle, dsp, chn);
					}
				} break;

				case LV2_MIDI_CTL_SC1_SOUND_VARIATION | 0x20:
				{
					handle->pressure[chn] &= 0x7f;
					handle->pressure[chn] |= val;
				} break;
				case LV2_MIDI_CTL_SC1_SOUND_VARIATION:
				{
					handle->pressure[chn] &= 0x3f80;
					handle->pressure[chn] |= val << 7;

					_update_pressure(handle, dsp, chn);
				} break;
				case LV2_MIDI_CTL_SC5_BRIGHTNESS | 0x20:
				{
					handle->timbre[chn] &= 0x7f;
					handle->timbre[chn] |= val;
				} break;
				case LV2_MIDI_CTL_SC5_BRIGHTNESS:
				{
					handle->timbre[chn] &= 0x3f80;
					handle->timbre[chn] |= val << 7;

					_update_timbre(handle, dsp, chn);
				} break;
			}
		} break;
	}
}

static void
_handle_midi(plughandle_t *handle, dsp_t *dsp,
	int64_t frames, const uint8_t *msg, uint32_t len)
{
	switch(len)
	{
		case 2:
		{
			_handle_midi_2(dsp, frames, msg);
		} break;
		case 3:
		{
			_handle_midi_3(handle, dsp, frames, msg);
		} break;
	}
}

static void
_tuple_set(LV2_Atom_Forge *forge, uint32_t frames, int64_t offset,
	LV2_URID key, float val, LV2_Atom_Forge_Ref *ref)
{
	LV2_Atom_Forge_Frame frm;

	if(*ref)
		*ref = lv2_atom_forge_frame_time(forge, frames);
	if(*ref)
		*ref = lv2_atom_forge_tuple(forge, &frm);
	if(*ref)
		*ref = lv2_atom_forge_long(forge, offset);
	if(*ref)
		*ref = lv2_atom_forge_urid(forge, key);
	if(*ref)
		*ref = lv2_atom_forge_float(forge, val);
	if(*ref)
		lv2_atom_forge_pop(forge, &frm);
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

	int64_t from = 0;
	LV2_ATOM_SEQUENCE_FOREACH(handle->control, ev)
	{
		const int64_t to = ev->time.frames;
		const LV2_Atom *atom = &ev->body;
		const LV2_Atom_Object *obj = (const LV2_Atom_Object *)&ev->body;

		if(atom->type == handle->midi_MidiEvent)
		{
			const bool off [2] = {
				handle->play,
				!handle->play
			};

			for(uint32_t d = 0; d < 2; d++)
			{
				dsp_t *dsp = handle->dsp[off[d]];

				if(!dsp)
				{
					continue;
				}

				_handle_midi(handle, dsp, to, LV2_ATOM_BODY_CONST(atom),
					atom->size);
			}
		}
		else
		{
			props_advance(&handle->props, &handle->forge, to, obj,
				&handle->ref);
		}

		timely_advance(&handle->timely, obj, from, to);
		_refresh_time_position(handle);
		_play(handle, from, to);

		from = to;
	}

	timely_advance(&handle->timely, NULL, from, nsamples);
	_refresh_time_position(handle);
	_play(handle, from, nsamples);

	// send error if applicable
	if(handle->dirty.error)
	{
		props_set(&handle->props, &handle->forge, nsamples-1, handle->mephisto_error,
			&handle->ref);

		handle->dirty.error = false;
	}

	if(handle->dirty.attributes)
	{
		for(unsigned i = 0; i < NCONTROLS; i++)
		{
			props_set(&handle->props, &handle->forge, nsamples-1, handle->mephisto_controlMin[i],
				&handle->ref);
			props_set(&handle->props, &handle->forge, nsamples-1, handle->mephisto_controlMax[i],
				&handle->ref);
			props_set(&handle->props, &handle->forge, nsamples-1, handle->mephisto_controlStep[i],
				&handle->ref);
			props_set(&handle->props, &handle->forge, nsamples-1, handle->mephisto_controlType[i],
				&handle->ref);
			props_set(&handle->props, &handle->forge, nsamples-1, handle->mephisto_controlLabel[i],
				&handle->ref);
		}

		handle->dirty.attributes = false;
	}

	for(unsigned i = 0; i < NCONTROLS; i++)
	{
		dsp_t *dsp = handle->dsp[handle->play];

		if(!dsp)
		{
			continue;
		}

		voice_t *voice = &dsp->voices[0];
		cntrl_t *cntrl = &voice->cntrls[i];

		if(!cntrl->readonly)
		{
			continue;
		}

		const float new = _cntrl_get_value_rel(&voice->cntrls[i]);

		if(new != handle->state.control[i])
		{
			handle->state.control[i] = new;

			props_set(&handle->props, &handle->forge, nsamples-1, handle->mephisto_control[i],
				&handle->ref);

			_tuple_set(&handle->forge, nsamples-1, handle->offset, handle->mephisto_control[i],
				handle->state.control[i], &handle->ref);
		}
	}

	handle->offset += nsamples;

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

static void
_meta_declare(void *iface, const char *key, const char *val)
{
	dsp_t *dsp = iface;
	plughandle_t *handle = dsp->handle;

	DBG(handle, "[%s] %s %s", __func__, key, val);

	if(!strcmp(key, "options"))
	{
		// iterate over options values
		for(const char *ptr = strchr(val, '['); ptr; ptr = strchr(++ptr, '['))
		{
			if(sscanf(ptr, "[nvoices:%"SCNu32"]", &dsp->nvoices) == 1)
			{
				if( (dsp->nvoices == 0) || (dsp->nvoices > MAX_VOICES) )
				{
					dsp->nvoices = MAX_VOICES;
				}
			}
			else if(strcasestr(ptr, "[midi:on]") == ptr)
			{
				dsp->midi_on = true;
			}
			else if(strcasestr(ptr, "[time:on]") == ptr)
			{
				dsp->time_on = true;
			}
		}
	}
}

static const char *
_strendswith(const char *haystack, const char *needle)
{
	const char *match = strcasestr(haystack, needle);

	if(match)
	{
		const size_t needle_len = strlen(needle);

		if(match[needle_len] == '\0')
		{
			return match;
		}
	}

	return NULL;
}

static voice_t *
_current_voice(dsp_t *dsp)
{
	if(dsp->cvoices < dsp->nvoices)
	{
		return &dsp->voices[dsp->cvoices];
	}

	return NULL;
}

static cntrl_t *
_ui_next_cntrl(dsp_t *dsp, cntrl_type_t type, const char *label)
{
	cntrl_t *cntrl = NULL;
	voice_t *voice = _current_voice(dsp);

	if(!voice)
	{
		return NULL;
	}

	if(dsp->is_instrument && _strendswith(label, "gain"))
	{
		cntrl = &voice->gain;
	}
	else if(dsp->is_instrument && _strendswith(label, "gate"))
	{
		cntrl = &voice->gate;
	}
	else if(dsp->is_instrument && _strendswith(label, "dfreq"))
	{
		cntrl = &voice->d_freq;
	}
	else if(dsp->is_instrument && _strendswith(label, "dpressure"))
	{
		cntrl = &voice->d_pressure;
	}
	else if(dsp->is_instrument && _strendswith(label, "dtimbre"))
	{
		cntrl = &voice->d_timbre;
	}
	else if(dsp->is_instrument && _strendswith(label, "freq"))
	{
		cntrl = &voice->freq;
	}
	else if(dsp->is_instrument && _strendswith(label, "pressure"))
	{
		cntrl = &voice->pressure;
	}
	else if(dsp->is_instrument && _strendswith(label, "timbre"))
	{
		cntrl = &voice->timbre;
	}
	else if(dsp->timely_mask)
	{
		switch(dsp->timely_mask)
		{
			case TIMELY_MASK_BAR_BEAT:
			{
				cntrl = &voice->pos.bar_beat;
			} break;
			case TIMELY_MASK_BAR:
			{
				cntrl = &voice->pos.bar;
			} break;
			case TIMELY_MASK_BEAT_UNIT:
			{
				cntrl = &voice->pos.beat_unit;
			} break;
			case TIMELY_MASK_BEATS_PER_BAR:
			{
				cntrl = &voice->pos.beats_per_bar;
			} break;
			case TIMELY_MASK_BEATS_PER_MINUTE:
			{
				cntrl = &voice->pos.beats_per_minute;
			} break;
			case TIMELY_MASK_FRAME:
			{
				cntrl = &voice->pos.frame;
			} break;
			case TIMELY_MASK_FRAMES_PER_SECOND:
			{
				cntrl = &voice->pos.frames_per_second;
			} break;
			case TIMELY_MASK_SPEED:
			{
				cntrl = &voice->pos.speed;
			} break;
			case TIMELY_MASK_BAR_BEAT_WHOLE:
			{
				// not used
			} break;
			case TIMELY_MASK_BAR_WHOLE:
			{
				// not used
			} break;
		}

		dsp->timely_mask = 0; // reset flag
	}
	else if( (dsp->idx >= 0) && (dsp->idx < NCONTROLS) )
	{
		cntrl = &voice->cntrls[dsp->idx];

		dsp->idx = -1;
	}

	if(!cntrl)
	{
		return NULL;
	}

	cntrl->type = type;
	strncpy(cntrl->label, label, sizeof(cntrl->label) - 1);

	return cntrl;
}

static void
_ui_open_tab_box(void* iface, const char* label)
{
	dsp_t *dsp = iface;
	plughandle_t *handle = dsp->handle;

	DBG(handle, "[%s] %s", __func__, label);
}

static void
_ui_open_horizontal_box(void* iface, const char* label)
{
	dsp_t *dsp = iface;
	plughandle_t *handle = dsp->handle;

	DBG(handle, "[%s] %s", __func__, label);
}

static void
_ui_open_vertical_box(void* iface, const char* label)
{
	dsp_t *dsp = iface;
	plughandle_t *handle = dsp->handle;

	DBG(handle, "[%s] %s", __func__, label);
}

static void
_ui_close_box(void* iface)
{
	dsp_t *dsp = iface;
	plughandle_t *handle = dsp->handle;

	DBG(handle, "[%s]", __func__);
}

static void
_ui_add_button(void* iface, const char* label, FAUSTFLOAT* zone)
{
	dsp_t *dsp = iface;
	plughandle_t *handle = dsp->handle;

	DBG(handle, "[%s] %s %f", __func__, label, *zone);

	cntrl_t *cntrl = _ui_next_cntrl(dsp, CNTRL_BUTTON, label);
	if(!cntrl)
	{
		return;
	}

	cntrl->zone = zone;
}

static void
_ui_add_check_button(void* iface, const char* label, FAUSTFLOAT* zone)
{
	dsp_t *dsp = iface;
	plughandle_t *handle = dsp->handle;

	DBG(handle, "[%s] %s %f", __func__, label, *zone);

	cntrl_t *cntrl = _ui_next_cntrl(dsp, CNTRL_CHECK_BUTTON, label);
	if(!cntrl)
	{
		return;
	}

	cntrl->zone = zone;
}

static void
_ui_add_vertical_slider(void* iface, const char* label, FAUSTFLOAT* zone,
	FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step)
{
	dsp_t *dsp = iface;
	plughandle_t *handle = dsp->handle;

	DBG(handle, "[%s] %s %f %f %f %f %f", __func__,
		label, *zone, init, min, max, step);

	cntrl_t *cntrl = _ui_next_cntrl(dsp, CNTRL_VERTICAL_SLIDER, label);
	if(!cntrl)
	{
		return;
	}

	cntrl->zone = zone;
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

	DBG(handle, "[%s] %s %f %f %f %f %f", __func__,
		label, *zone, init, min, max, step);

	cntrl_t *cntrl = _ui_next_cntrl(dsp, CNTRL_HORIZONTAL_SLIDER, label);
	if(!cntrl)
	{
		return;
	}

	cntrl->zone = zone;
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

	DBG(handle, "[%s] %s %f %f %f %f %f", __func__,
		label, *zone, init, min, max, step);

	cntrl_t *cntrl = _ui_next_cntrl(dsp, CNTRL_NUM_ENTRY, label);
	if(!cntrl)
	{
		return;
	}

	cntrl->zone = zone;
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

	DBG(handle, "[%s] %s %f %f %f", __func__,
		label, *zone, min, max);

	cntrl_t *cntrl = _ui_next_cntrl(dsp, CNTRL_HORIZONTAL_BARGRAPH, label);
	if(!cntrl)
	{
		return;
	}

	cntrl->zone = zone;
	cntrl->readonly = true;
	cntrl->horizontal_bargraph.min = min;
	cntrl->horizontal_bargraph.max = max;
	cntrl->horizontal_bargraph.ran_1 = 1.f / (max - min);
}

static void
_ui_add_vertical_bargraph(void* iface, const char* label, FAUSTFLOAT* zone,
	FAUSTFLOAT min, FAUSTFLOAT max)
{
	dsp_t *dsp = iface;
	plughandle_t *handle = dsp->handle;

	DBG(handle, "[%s] %s %f %f %f", __func__,
		label, *zone, min, max);

	cntrl_t *cntrl = _ui_next_cntrl(dsp, CNTRL_VERTICAL_BARGRAPH, label);
	if(!cntrl)
	{
		return;
	}

	cntrl->zone = zone;
	cntrl->readonly = true;
	cntrl->vertical_bargraph.min = min;
	cntrl->vertical_bargraph.max = max;
	cntrl->vertical_bargraph.ran_1 = 1.f / (max - min);
}

static void
_ui_add_sound_file(void* iface, const char* label, const char* filename,
	struct Soundfile** sf_zone __attribute__((unused)))
{
	dsp_t *dsp = iface;
	plughandle_t *handle = dsp->handle;

	DBG(handle, "[%s] %s %s", __func__,
		label, filename);

	cntrl_t *cntrl = _ui_next_cntrl(dsp, CNTRL_SOUND_FILE, label);
	if(!cntrl)
	{
		return;
	}

	//FIXME
}

static void
_ui_declare(void* iface, FAUSTFLOAT* zone __attribute__((unused)),
	const char* key, const char* value)
{
	dsp_t *dsp = iface;
	plughandle_t *handle = dsp->handle;

	DBG(handle, "[%s] %s %s", __func__,
		key, value);

	if(!strcasecmp(value, ""))
	{
		char *endptr = NULL;
		const long int idx = strtol(key, &endptr, 10);

		if(endptr != key)
		{
			dsp->idx = idx;
		}
		else if(handle->log)
		{
			lv2_log_error(&handle->logger, "[%s] invalid index key %s",
				__func__, key);
		}
	}
	else if(!strcasecmp(key, "time"))
	{
		if(!strcasecmp(value, "barBeat"))
		{
			dsp->timely_mask = TIMELY_MASK_BAR_BEAT;
		}
		else if(!strcasecmp(value, "bar"))
		{
			dsp->timely_mask = TIMELY_MASK_BAR;
		}
		else if(!strcasecmp(value, "beatUnit"))
		{
			dsp->timely_mask = TIMELY_MASK_BEAT_UNIT;
		}
		else if(!strcasecmp(value, "beatsPerBar"))
		{
			dsp->timely_mask = TIMELY_MASK_BEATS_PER_BAR;
		}
		else if(!strcasecmp(value, "beatsPerMinute"))
		{
			dsp->timely_mask = TIMELY_MASK_BEATS_PER_MINUTE;
		}
		else if(!strcasecmp(value, "frame"))
		{
			dsp->timely_mask = TIMELY_MASK_FRAME;
		}
		else if(!strcasecmp(value, "framesPerSecond"))
		{
			dsp->timely_mask = TIMELY_MASK_FRAMES_PER_SECOND;
		}
		else if(!strcasecmp(value, "speed"))
		{
			dsp->timely_mask = TIMELY_MASK_SPEED;
		}
		else if(handle->log)
		{
			lv2_log_error(&handle->logger, "[%s] invalid time value %s",
				__func__, value);
		}
	}
	else if(handle->log)
	{
			lv2_log_note(&handle->logger, "[%s] unknown key:value pair %s:%s",
				__func__, key, value);
	}
}

static int
_meta_init(dsp_t *dsp, voice_t *base_voice)
{
	MetaGlue *glue = &dsp->meta_glue;

	glue->metaInterface = dsp;

	glue->declare = _meta_declare;

	dsp->nvoices = 1; // assume we're a filter by default
	dsp->timely_mask = 0;
	dsp->idx = -1;

	metadataCDSPInstance(base_voice->instance, glue);

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
	glue->FAUST_ADDSOUNDFILE= _ui_add_sound_file;
	glue->declare = _ui_declare;

	dsp->cvoices = 0;

	VOICE_FOREACH(dsp, voice)
	{
		if(voice->instance)
		{
			buildUserInterfaceCDSPInstance(voice->instance, glue);
		}

		dsp->cvoices++;
	}

	return 0;
}

static int
_dsp_init(plughandle_t *handle, dsp_t *dsp, const char *code,
	LV2_Worker_Respond_Function respond, LV2_Worker_Respond_Handle target)
{
#define ARGC 5
	char err [4096];

	const char *argv [ARGC] = {
		"-I", handle->dsp_dir,
		"-vec",
		"-lv", "1"
	};

	{
		const job_t job = {
			.type = JOB_TYPE_ERROR_CLEAR,
			.error = NULL
		};

		respond(target, sizeof(job), &job);
	}

	dsp->handle = handle;
	memset(err, 0x0, sizeof(err));

	pthread_mutex_lock(&lock);

	dsp->factory = createCDSPFactoryFromString("mephisto", code, ARGC, argv, "", err, -1);
	if(!dsp->factory)
	{
		if(handle->log)
		{
			lv2_log_error(&handle->logger, "[%s] %s", __func__, err);

			const job_t job = {
				.type = JOB_TYPE_ERROR_APPEND,
				.error = strdup(err)
			};

			respond(target, sizeof(job), &job);
		}

		goto fail;
	}

	voice_t *base_voice = _voice_begin(dsp);
	base_voice->instance = createCDSPInstance(dsp->factory);
	if(!base_voice->instance)
	{
		if(handle->log)
		{
			lv2_log_error(&handle->logger, "[%s] instance creation failed", __func__);
		}

		deleteCDSPFactory(dsp->factory);
		goto fail;
	}

	instanceInitCDSPInstance(base_voice->instance, handle->srate);

	dsp->nins = getNumInputsCDSPInstance(base_voice->instance);
	dsp->nouts = getNumInputsCDSPInstance(base_voice->instance);

	if(_meta_init(dsp, base_voice) != 0)
	{
		if(handle->log)
		{
			lv2_log_error(&handle->logger, "[%s] meta creation failed", __func__);
		}

		deleteCDSPFactory(dsp->factory);
		goto fail;
	}

	dsp->is_instrument = (dsp->nvoices > 1);

	if(dsp->is_instrument)
	{
		VOICE_FOREACH(dsp, voice)
		{
			if(voice == base_voice) // skip base voice
			{
				continue;
			}

			voice->instance = cloneCDSPInstance(base_voice->instance);
			if(!voice->instance)
			{
				if(handle->log)
				{
					lv2_log_error(&handle->logger, "[%s] instance creation failed", __func__);
				}

				break;
			}

			instanceInitCDSPInstance(voice->instance, handle->srate);
		}
	}
	else
	{
		base_voice->state = VOICE_STATE_ACTIVE;
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

	if(handle->log)
	{
		lv2_log_note(&handle->logger,
			"[%s] compilation succeeded (ins: %u, outs: %u, type: %s)", __func__,
			dsp->nins, dsp->nouts, dsp->is_instrument ? "instrument" : "filter");
	}

	pthread_mutex_unlock(&lock);
	return 0;

fail:
	pthread_mutex_unlock(&lock);
	return 1;
#undef ARGC
}

static void
_dsp_deinit(plughandle_t *handle __attribute__((unused)), dsp_t *dsp)
{
	if(dsp)
	{
		pthread_mutex_lock(&lock);

		VOICE_FOREACH(dsp, voice)
		{
			if(voice->instance)
			{
				instanceClearCDSPInstance(voice->instance);
				deleteCDSPInstance(voice->instance);
			}
		}

		if(dsp->factory)
		{
			deleteCDSPFactory(dsp->factory);
		}

		pthread_mutex_unlock(&lock);
	}
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
				if(dsp && (_dsp_init(handle, dsp, code, respond, target) == 0) )
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

		case JOB_TYPE_ERROR_CLEAR:
		{
			// never reached
		} break;
		case JOB_TYPE_ERROR_APPEND:
		{
			// never reached
		} break;
		case JOB_TYPE_ERROR_FREE:
		{
			if(job->error)
			{
				free(job->error);
			}
		} break;
		default:
		{
			// never reached
		} return LV2_WORKER_ERR_UNKNOWN;
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

			for(uint32_t i = 0; i < NCONTROLS; i++)
			{
				_refresh_value(handle, i);
				_refresh_attributes(handle, i);
			}

			dsp_t *cur_dsp = handle->dsp[handle->play];
			dsp_t *new_dsp = handle->dsp[!handle->play];

			if(cur_dsp && new_dsp)
			{
				voice_t *cur_voice = _voice_begin(cur_dsp);

				VOICE_FOREACH(new_dsp, new_voice)
				{
					_cntrl_refresh_value_abs(&new_voice->freq,
						_cntrl_get_value_abs(&cur_voice->freq));
					_cntrl_refresh_value_abs(&new_voice->pressure,
						_cntrl_get_value_abs(&cur_voice->pressure));
					_cntrl_refresh_value_abs(&new_voice->timbre,
						_cntrl_get_value_abs(&cur_voice->timbre));
					_cntrl_refresh_value_abs(&new_voice->d_freq,
						_cntrl_get_value_abs(&cur_voice->d_freq));
					_cntrl_refresh_value_abs(&new_voice->d_pressure,
						_cntrl_get_value_abs(&cur_voice->d_pressure));
					_cntrl_refresh_value_abs(&new_voice->d_timbre,
						_cntrl_get_value_abs(&cur_voice->d_timbre));
					_cntrl_refresh_value_abs(&new_voice->gate,
						_cntrl_get_value_abs(&cur_voice->gate));
					_cntrl_refresh_value_abs(&new_voice->gain,
						_cntrl_get_value_abs(&cur_voice->gain));

					new_voice->state = cur_voice->state;
					new_voice->hash = cur_voice->hash;

					if(!_voice_not_end(cur_dsp, cur_voice))
					{
						break;
					}

					cur_voice = _voice_next(cur_voice);
				}
			}

			handle->dirty.attributes = true;
		} break;
		case JOB_TYPE_DEINIT:
		{
			// never reached
		} break;
		case JOB_TYPE_ERROR_CLEAR:
		{
			props_impl_t *impl = _props_impl_get(&handle->props, handle->mephisto_error);

			if(impl)
			{
				static const char *empty = "";

				strncpy(handle->state.error, empty, impl->def->max_size);
				impl->value.size = strlen(handle->state.error) + 1;

				handle->dirty.error = true;
			}
		} break;
		case JOB_TYPE_ERROR_APPEND:
		{
			props_impl_t *impl = _props_impl_get(&handle->props, handle->mephisto_error);

			if(impl)
			{
				strncat(handle->state.error, job->error, impl->def->max_size - strlen(job->error) - 1);
				impl->value.size = strlen(handle->state.error) + 1;

				handle->dirty.error = true;

				const job_t job2 = {
					.type = JOB_TYPE_ERROR_FREE,
					.error = job->error
				};

				handle->sched->schedule_work(handle->sched->handle, sizeof(job2), &job2);
			}
		} break;
		case JOB_TYPE_ERROR_FREE:
		{
			// never reached
		} break;
		default:
		{
			// never reached
		} return LV2_WORKER_ERR_UNKNOWN;
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

static const LV2_Descriptor mephisto_audio_1x1 = {
	.URI						= MEPHISTO__audio_1x1,
	.instantiate		= instantiate,
	.connect_port		= connect_port,
	.activate				= NULL,
	.run						= run,
	.deactivate			= NULL,
	.cleanup				= cleanup,
	.extension_data	= extension_data
};

static const LV2_Descriptor mephisto_audio_2x2 = {
	.URI						= MEPHISTO__audio_2x2,
	.instantiate		= instantiate,
	.connect_port		= connect_port,
	.activate				= NULL,
	.run						= run,
	.deactivate			= NULL,
	.cleanup				= cleanup,
	.extension_data	= extension_data
};

static const LV2_Descriptor mephisto_audio_4x4 = {
	.URI						= MEPHISTO__audio_4x4,
	.instantiate		= instantiate,
	.connect_port		= connect_port,
	.activate				= NULL,
	.run						= run,
	.deactivate			= NULL,
	.cleanup				= cleanup,
	.extension_data	= extension_data
};

static const LV2_Descriptor mephisto_audio_8x8 = {
	.URI						= MEPHISTO__audio_8x8,
	.instantiate		= instantiate,
	.connect_port		= connect_port,
	.activate				= NULL,
	.run						= run,
	.deactivate			= NULL,
	.cleanup				= cleanup,
	.extension_data	= extension_data
};

static const LV2_Descriptor mephisto_cv_1x1 = {
	.URI						= MEPHISTO__cv_1x1,
	.instantiate		= instantiate,
	.connect_port		= connect_port,
	.activate				= NULL,
	.run						= run,
	.deactivate			= NULL,
	.cleanup				= cleanup,
	.extension_data	= extension_data
};

static const LV2_Descriptor mephisto_cv_2x2 = {
	.URI						= MEPHISTO__cv_2x2,
	.instantiate		= instantiate,
	.connect_port		= connect_port,
	.activate				= NULL,
	.run						= run,
	.deactivate			= NULL,
	.cleanup				= cleanup,
	.extension_data	= extension_data
};

static const LV2_Descriptor mephisto_cv_4x4 = {
	.URI						= MEPHISTO__cv_4x4,
	.instantiate		= instantiate,
	.connect_port		= connect_port,
	.activate				= NULL,
	.run						= run,
	.deactivate			= NULL,
	.cleanup				= cleanup,
	.extension_data	= extension_data
};

static const LV2_Descriptor mephisto_cv_8x8 = {
	.URI						= MEPHISTO__cv_8x8,
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
			return &mephisto_audio_1x1;
		case 1:
			return &mephisto_audio_2x2;
		case 2:
			return &mephisto_audio_4x4;
		case 3:
			return &mephisto_audio_8x8;

		case 4:
			return &mephisto_cv_1x1;
		case 5:
			return &mephisto_cv_2x2;
		case 6:
			return &mephisto_cv_4x4;
		case 7:
			return &mephisto_cv_8x8;

		default:
			return NULL;
	}
}
