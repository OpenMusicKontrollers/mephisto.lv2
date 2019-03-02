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
#include <math.h>
#include <inttypes.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include <mephisto.h>
#include <props.h>
#include <varchunk.h>

#include <faust/dsp/llvm-c-dsp.h>

#define MAX_CHANNEL 2

typedef struct _dsp_t dsp_t;
typedef struct _job_t job_t;
typedef struct _plughandle_t plughandle_t;

struct _dsp_t {
	llvm_dsp_factory *factory;
	llvm_dsp *instance;
};

typedef enum _job_type_t {
	JOB_TYPE_INIT,
	JOB_TYPE_DEINIT,
} job_type_t;

struct _job_t {
	job_type_t type;
	dsp_t dsp;
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
	dsp_t dsp [2];
};

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
	const float *audio_in [3] = {
		&handle->audio_in[0][from],
		&handle->audio_in[1][from],
		NULL
	};
	float *audio_out [3] = {
		&handle->audio_out[0][from],
		&handle->audio_out[1][from],
		NULL
	};

	for(uint32_t n = 0; n < handle->nchannel; n++)
	{
		for(uint32_t i = 0; i < nsamples; i++)
		{
			audio_out[n][i] = 0.f; // silence
		}
	}

	dsp_t *dsp = &handle->dsp[handle->play];
	if(dsp->instance)
	{
		computeCDSPInstance(dsp->instance, nsamples, (FAUSTFLOAT **)audio_in,
			(FAUSTFLOAT **)audio_out);
	}

	if(handle->xfade_cur > 0)
	{
		for(uint32_t i = 0;
			(handle->xfade_cur > 0) && (i < nsamples);
			i++, handle->xfade_cur--)
		{
			const float gain = (float)handle->xfade_cur / handle->xfade_max;
			const float mul = handle->xfade_dst ? (1.f - gain) : gain;

			audio_out[0][i] *= mul;
			audio_out[1][i] *= mul;
		}

		if(handle->xfade_cur == 0)
		{
			if(handle->xfade_dst == 0)
			{
				handle->play = !handle->play;
				handle->xfade_cur = handle->xfade_max;
				handle->xfade_dst = 1;
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
_dsp_init(plughandle_t *handle, dsp_t *dsp, const char *code)
{
#define ARGC 5
	char err [4096];
	const char *argv [ARGC] = {
		"-I", "/usr/share/faust", //FIXME
		"-vec",
		"-lv", "1"
	};

	dsp->factory = createCDSPFactoryFromString("mephisto", code, ARGC, argv, "", err, -1);
	if(!dsp->factory)
	{
		if(handle->log)
		{
			lv2_log_error(&handle->logger, "[%s] %s", __func__, err);
		}

		return 1;
	}

	dsp->instance = createCDSPInstance(dsp->factory);
	if(!dsp->instance)
	{
		if(handle->log)
		{
			lv2_log_error(&handle->logger, "[%s] instance creation failed", __func__);
		}

		deleteCDSPFactory(dsp->factory);
		return 1;
	}

	instanceInitCDSPInstance(dsp->instance, handle->srate);

	if(handle->log)
	{
		lv2_log_note(&handle->logger, "[%s] compilation succeeded", __func__);
	}

	return 0;
#undef ARGC
}

static void
_dsp_deinit(plughandle_t *handle __attribute__((unused)), const dsp_t *dsp)
{
	if(dsp->instance)
	{
		instanceClearCDSPInstance(dsp->instance);
		deleteCDSPInstance(dsp->instance);
	}

	if(dsp->factory)
	{
		deleteCDSPFactory(dsp->factory);
	}
}

static void
cleanup(LV2_Handle instance)
{
	plughandle_t *handle = instance;

	munlock(handle, sizeof(plughandle_t));
	varchunk_free(handle->to_worker);
	_dsp_deinit(handle, &handle->dsp[0]);
	_dsp_deinit(handle, &handle->dsp[1]);
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
				dsp_t dsp;
				if(_dsp_init(handle, &dsp, code) == 0)
				{
					respond(target, sizeof(dsp), &dsp);
				}

				varchunk_read_advance(handle->to_worker);
			}
		} break;
		case JOB_TYPE_DEINIT:
		{
			_dsp_deinit(handle, &job->dsp);
		} break;
	}

	return LV2_WORKER_SUCCESS;
}

// rt-thread
static LV2_Worker_Status
_work_response(LV2_Handle instance, uint32_t size, const void *body)
{
	plughandle_t *handle = instance;

	if(size != sizeof(dsp_t))
	{
		return LV2_WORKER_ERR_UNKNOWN;
	}

	const job_t job = {
		.type = JOB_TYPE_DEINIT,
		.dsp = handle->dsp[!handle->play]
	};
	handle->sched->schedule_work(handle->sched->handle, sizeof(job), &job);

	const dsp_t *dsp_new = body;
	handle->dsp[!handle->play] = *dsp_new;

	handle->xfade_cur = handle->xfade_max;
	handle->xfade_dst = 0;

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
