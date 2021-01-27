/*
 * Copyright (c) 2019-2021 Hanspeter Portner (dev@open-music-kontrollers.ch)
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

#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>
#include <utime.h>
#include <limits.h>

#include <mephisto.h>
#include <props.h>

#define SER_ATOM_IMPLEMENTATION
#include <ser_atom.lv2/ser_atom.h>

#include <d2tk/hash.h>
#include <d2tk/util.h>
#include <d2tk/frontend_pugl.h>

#define GLYPH_W 7
#define GLYPH_H (GLYPH_W * 2)

#define FPS 25

#define DEFAULT_FG 0xddddddff
#define DEFAULT_BG 0x222222ff

#define NROWS_MAX 512
#define NCOLS_MAX 512

#define MAX(x, y) (x > y ? y : x)
#define WAV_MAX 512

typedef struct _wav_t wav_t;
typedef struct _plughandle_t plughandle_t;

struct _wav_t {
	float vals [WAV_MAX];	
};

struct _plughandle_t {
	LV2_URID_Map *map;
	LV2_Atom_Forge forge;

	LV2_Log_Log *log;
	LV2_Log_Logger logger;

#ifdef _LV2_HAS_REQUEST_VALUE
	LV2UI_Request_Value *request_code;
#endif

	d2tk_pugl_config_t config;
	d2tk_frontend_t *dpugl;

	LV2UI_Controller *controller;
	LV2UI_Write_Function writer;

	PROPS_T(props, MAX_NPROPS);

	plugstate_t state;
	plugstate_t stash;

	int64_t ts;
	wav_t wavs [NCONTROLS];

	uint64_t hash;

	LV2_URID atom_eventTransfer;
	LV2_URID midi_MidiEvent;
	LV2_URID urid_code;
	LV2_URID urid_error;
	LV2_URID urid_xfadeDuration;
	LV2_URID urid_fontHeight;
	LV2_URID urid_control [NCONTROLS];

	bool reinit;
	char template [24];
	int fd;
	time_t modtime;

	float scale;
	float sample_rate;
	double frac;
	d2tk_coord_t header_height;
	d2tk_coord_t footer_height;
	d2tk_coord_t tip_height;
	d2tk_coord_t sidebar_width;
	d2tk_coord_t item_height;
	d2tk_coord_t font_height;

	uint32_t max_red;

	int done;
	int kid;
};

static inline void
_update_font_height(plughandle_t *handle)
{
	handle->font_height = handle->state.font_height * handle->scale;
}

static void
_intercept_code(void *data, int64_t frames __attribute__((unused)),
	props_impl_t *impl __attribute__((unused)))
{
	plughandle_t *handle = data;

	const ssize_t len = strlen(handle->state.code);
	const uint64_t hash = d2tk_hash(handle->state.code, len);

	if(handle->hash == hash)
	{
		return;
	}

	handle->hash = hash;

	// save code to file
	if(lseek(handle->fd, 0, SEEK_SET) == -1)
	{
		lv2_log_error(&handle->logger, "lseek: %s\n", strerror(errno));
	}
	if(ftruncate(handle->fd, 0) == -1)
	{
		lv2_log_error(&handle->logger, "ftruncate: %s\n", strerror(errno));
	}
	if(fsync(handle->fd) == -1)
	{
		lv2_log_error(&handle->logger, "fsync: %s\n", strerror(errno));
	}
	if(write(handle->fd, handle->state.code, len) == -1)
	{
		lv2_log_error(&handle->logger, "write: %s\n", strerror(errno));
	}
	if(fsync(handle->fd) == -1)
	{
		lv2_log_error(&handle->logger, "fsync: %s\n", strerror(errno));
	}

	// change modification timestamp of file
	struct stat st;
	if(stat(handle->template, &st) == -1)
	{
		lv2_log_error(&handle->logger, "stat: %s\n", strerror(errno));
	}

	handle->modtime = time(NULL);

	const struct utimbuf btime = {
	 .actime = st.st_atime,
	 .modtime = handle->modtime
	};

	if(utime(handle->template, &btime) == -1)
	{
		lv2_log_error(&handle->logger, "utime: %s\n", strerror(errno));
	}

	handle->reinit = true;
}

static void
_intercept_font_height(void *data, int64_t frames __attribute__((unused)),
	props_impl_t *impl __attribute__((unused)))
{
	plughandle_t *handle = data;

	_update_font_height(handle);
}

static void
_reset_wavs(plughandle_t *handle)
{
	for(unsigned c = 0; c < NCONTROLS; c++)
	{
		for(unsigned i = 0; i < WAV_MAX; i++)
		{
			handle->wavs[c].vals[i] = HUGE_VAL;
		}
	}
}

static void
_update_wavs(plughandle_t *handle, unsigned nframes)
{
	const double nsecs = 2.f; //FIXME make this configurable
	const double npixels = handle->frac + nframes * WAV_MAX / (handle->sample_rate * nsecs);
	double fpixels = 0.f;
	handle->frac = modf(npixels, &fpixels);
	const unsigned ipixels = fpixels;

	for(unsigned c = 0; c < NCONTROLS; c++)
	{
		for(unsigned i = 0; i < WAV_MAX-ipixels; i++)
		{
			handle->wavs[c].vals[i] = handle->wavs[c].vals[i + ipixels];
		}

		for(unsigned i = WAV_MAX-ipixels; i < WAV_MAX; i++)
		{
			handle->wavs[c].vals[i] = HUGE_VAL;
		}

		handle->wavs[c].vals[WAV_MAX-1] = handle->state.control[c];
	}
}

static void
_intercept_timestamp(void *data, int64_t frames __attribute__((unused)),
	props_impl_t *impl __attribute__((unused)))
{
	plughandle_t *handle = data;

	if(handle->ts && (handle->state.timestamp > handle->ts) )
	{
		const unsigned nframes = handle->state.timestamp - handle->ts;

		_update_wavs(handle, nframes);
	}

	handle->ts = handle->state.timestamp;
}

static void
_intercept_control(void *data, int64_t frames __attribute__((unused)),
	props_impl_t *impl)
{
	plughandle_t *handle = data;
	const uint32_t k = (float *)impl->value.body - handle->state.control;

	const cntrl_type_t type = handle->state.control_type[k];
	switch(type)
	{
		case CNTRL_VERTICAL_BARGRAPH:
			// fall-through
		case CNTRL_HORIZONTAL_BARGRAPH:
		{
			//FIXME
		} break;

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
		case CNTRL_NONE:
			// fall-through
		case CNTRL_SOUND_FILE:
		{
			// nothing to do
		} break;
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
		.type = LV2_ATOM__Int
	},
	{
		.property = MEPHISTO__fontHeight,
		.offset = offsetof(plugstate_t, font_height),
		.type = LV2_ATOM__Int,
		.event_cb = _intercept_font_height
	},
	{
		.property = MEPHISTO__timestamp,
		.access = LV2_PATCH__readable,
		.offset = offsetof(plugstate_t, timestamp),
		.type = LV2_ATOM__Long,
		.event_cb = _intercept_timestamp
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

static void
_message_midi_note(plughandle_t *handle, uint8_t chan, uint8_t msg, uint8_t note,
	uint8_t vel)
{
	const struct {
		LV2_Atom atom;
		uint8_t body [8];
	} midi = {
		.atom = {
			.type = handle->midi_MidiEvent,
			.size = 3
		},
		.body = {
			[0] = chan | msg,
			[1] = note,
			[2] = vel
		}
	};

	handle->writer(handle->controller, 0, lv2_atom_total_size(&midi.atom),
		handle->atom_eventTransfer, &midi);
}

static void
_message_midi_allnotesoff(plughandle_t *handle)
{
	for(uint8_t chan = 0x0; chan < 0x10; chan++)
	{
		const struct {
			LV2_Atom atom;
			uint8_t body [8];
		} midi = {
			.atom = {
				.type = handle->midi_MidiEvent,
				.size = 2
			},
			.body = {
				[0] = chan | LV2_MIDI_MSG_CONTROLLER,
				[1] = LV2_MIDI_CTL_ALL_NOTES_OFF
			}
		};

		handle->writer(handle->controller, 0, lv2_atom_total_size(&midi.atom),
			handle->atom_eventTransfer, &midi);
	}
}

static void
_message_set_code(plughandle_t *handle, size_t len)
{
	ser_atom_t ser;
	props_impl_t *impl = _props_impl_get(&handle->props, handle->urid_code);
	if(!impl)
	{
		return;
	}

	impl->value.size = len;

	ser_atom_init(&ser);
	ser_atom_reset(&ser, &handle->forge);

	LV2_Atom_Forge_Ref ref = 1;

	props_set(&handle->props, &handle->forge, 0, handle->urid_code, &ref);

	const LV2_Atom_Event *ev = (const LV2_Atom_Event *)ser_atom_get(&ser);
	const LV2_Atom *atom = &ev->body;
	handle->writer(handle->controller, 0, lv2_atom_total_size(atom),
		handle->atom_eventTransfer, atom);

	ser_atom_deinit(&ser);
}

static void
_message_set_key(plughandle_t *handle, LV2_URID key)
{
	ser_atom_t ser;
	props_impl_t *impl = _props_impl_get(&handle->props, key);
	if(!impl)
	{
		return;
	}

	ser_atom_init(&ser);
	ser_atom_reset(&ser, &handle->forge);

	LV2_Atom_Forge_Ref ref = 1;

	props_set(&handle->props, &handle->forge, 0, key, &ref);

	const LV2_Atom_Event *ev = (const LV2_Atom_Event *)ser_atom_get(&ser);
	const LV2_Atom *atom = &ev->body;
	handle->writer(handle->controller, 0, lv2_atom_total_size(atom),
		handle->atom_eventTransfer, atom);

	ser_atom_deinit(&ser);
}

static void
_message_set_control(plughandle_t *handle, unsigned k)
{
	const LV2_URID key = handle->urid_control[k];

	_message_set_key(handle, key);
}

static void
_message_get(plughandle_t *handle, LV2_URID key)
{
	ser_atom_t ser;
	props_impl_t *impl = _props_impl_get(&handle->props, key);
	if(!impl)
	{
		return;
	}

	ser_atom_init(&ser);
	ser_atom_reset(&ser, &handle->forge);

	LV2_Atom_Forge_Ref ref = 1;

	props_get(&handle->props, &handle->forge, 0, key, &ref);

	const LV2_Atom_Event *ev = (const LV2_Atom_Event *)ser_atom_get(&ser);
	const LV2_Atom *atom = &ev->body;
	handle->writer(handle->controller, 0, lv2_atom_total_size(atom),
		handle->atom_eventTransfer, atom);

	ser_atom_deinit(&ser);
}

static inline void
_expose_header(plughandle_t *handle, const d2tk_rect_t *rect)
{
	d2tk_frontend_t *dpugl = handle->dpugl;
	d2tk_base_t *base = d2tk_frontend_get_base(dpugl);

	const d2tk_coord_t frac [3] = { 1, 1, 1 };
	D2TK_BASE_LAYOUT(rect, 3, frac, D2TK_FLAG_LAYOUT_X_REL, lay)
	{
		const unsigned k = d2tk_layout_get_index(lay);
		const d2tk_rect_t *lrect = d2tk_layout_get_rect(lay);

		switch(k)
		{
			case 0:
			{
				d2tk_base_label(base, -1, "Open•Music•Kontrollers", 0.5f, lrect,
					D2TK_ALIGN_LEFT | D2TK_ALIGN_TOP);
			} break;
			case 1:
			{
				d2tk_base_label(base, -1, "M•E•P•H•I•S•T•O", 1.f, lrect,
					D2TK_ALIGN_CENTER | D2TK_ALIGN_TOP);
			} break;
			case 2:
			{
				d2tk_base_label(base, -1, "Version "MEPHISTO_VERSION, 0.5f, lrect,
					D2TK_ALIGN_RIGHT | D2TK_ALIGN_TOP);
			} break;
		}
	}
}

#if 0
static inline void
_expose_vkb(plughandle_t *handle, const d2tk_rect_t *rect)
{
	d2tk_frontend_t *dpugl = handle->dpugl;
	d2tk_base_t *base = d2tk_frontend_get_base(dpugl);

	static const uint8_t off_black [7] = {
		0, 1, 3, 0, 6, 8, 10,
	};
	static const uint8_t off_white [7] = {
		0, 2, 4, 5, 7, 9, 11
	};

	D2TK_BASE_TABLE(rect, 48, 2, D2TK_FLAG_TABLE_REL, tab)
	{
		const unsigned x = d2tk_table_get_index_x(tab);
		const unsigned y = d2tk_table_get_index_y(tab);
		const unsigned k = d2tk_table_get_index(tab);
		const d2tk_rect_t *trect = d2tk_table_get_rect(tab);

		switch(y)
		{
			case 0:
			{
				d2tk_rect_t trect2 = *trect;
				trect2.x -= trect2.w/2;

				switch(x%7)
				{
					case 0:
					{
						char buf [32];
						const ssize_t len = snprintf(buf, sizeof(buf), "%+i", x/7);

						d2tk_base_label(base, len, buf, 0.35f, &trect2,
							D2TK_ALIGN_TOP| D2TK_ALIGN_RIGHT);
					} break;
					case 1:
						// fall-through
					case 2:
						// fall-through
					case 4:
						// fall-through
					case 5:
						// fall-through
					case 6:
						// fall-through
					{
						const d2tk_state_t state = d2tk_base_button(base, D2TK_ID_IDX(k),
							&trect2);

						if(d2tk_state_is_down(state))
						{
							const uint8_t note = x/7*12 + off_black[x%7];

							_message_midi_note(handle, 0x0, LV2_MIDI_MSG_NOTE_ON, note, 0x7f);
						}
						if(d2tk_state_is_up(state))
						{
							const uint8_t note = x/7*12 + off_black[x%7];

							_message_midi_note(handle, 0x0, LV2_MIDI_MSG_NOTE_OFF, note, 0x0);
						}
					} break;
				}
			} break;
			case 1:
			{
				const d2tk_state_t state = d2tk_base_button(base, D2TK_ID_IDX(k), trect);

				if(d2tk_state_is_down(state))
				{
					const uint8_t note = x/7*12 + off_white[x%7];

					_message_midi_note(handle, 0x0, LV2_MIDI_MSG_NOTE_ON, note, 0x7f);
				}
				if(d2tk_state_is_up(state))
				{
					const uint8_t note = x/7*12 + off_white[x%7];

					_message_midi_note(handle, 0x0, LV2_MIDI_MSG_NOTE_OFF, note, 0x0);
				}
			} break;
		}
	}
}
#endif

static inline void
_expose_xfade(plughandle_t *handle, const d2tk_rect_t *rect)
{
	d2tk_frontend_t *dpugl = handle->dpugl;
	d2tk_base_t *base = d2tk_frontend_get_base(dpugl);

	static const char lbl [] = "crossfade•ms";

	if(d2tk_base_spinner_int32_is_changed(base, D2TK_ID, rect,
		sizeof(lbl), lbl, 10, &handle->state.xfade_dur, 1000, D2TK_FLAG_NONE))
	{
		_message_set_key(handle, handle->urid_xfadeDuration);
	}
}

static inline void
_expose_font_height(plughandle_t *handle, const d2tk_rect_t *rect)
{
	d2tk_frontend_t *dpugl = handle->dpugl;
	d2tk_base_t *base = d2tk_frontend_get_base(dpugl);

	static const char lbl [] = "font-height•px";

	if(d2tk_base_spinner_int32_is_changed(base, D2TK_ID, rect,
		sizeof(lbl), lbl, 10, &handle->state.font_height, 25, D2TK_FLAG_NONE))
	{
		_message_set_key(handle, handle->urid_fontHeight);
		_update_font_height(handle);
	}
}

static inline void
_expose_panic(plughandle_t *handle, const d2tk_rect_t *rect)
{
	d2tk_frontend_t *dpugl = handle->dpugl;
	d2tk_base_t *base = d2tk_frontend_get_base(dpugl);

	static const char path [] = "alert-triangle.png";
	static const char tip [] = "panic";

	const d2tk_state_t state = d2tk_base_button_image(base, D2TK_ID,
		sizeof(path), path, rect);

	if(d2tk_state_is_changed(state))
	{
		_message_midi_allnotesoff(handle);
	}
	if(d2tk_state_is_over(state))
	{
		d2tk_base_set_tooltip(base, sizeof(tip), tip, handle->tip_height);
	}
}

static const char *
_text_basename(plughandle_t *handle)
{
	return basename(handle->template);
}

static void
_expose_text_link(plughandle_t *handle, const d2tk_rect_t *rect)
{
	d2tk_base_t *base = d2tk_frontend_get_base(handle->dpugl);

	static const char tip [] = "open externally";
	char lbl [PATH_MAX];
	const size_t lbl_len = snprintf(lbl, sizeof(lbl), "%s",
		_text_basename(handle));

	const d2tk_state_t state = d2tk_base_link(base, D2TK_ID, lbl_len, lbl, .5f,
		rect, D2TK_ALIGN_MIDDLE | D2TK_ALIGN_LEFT);

	if(d2tk_state_is_changed(state))
	{
		char *argv [] = {
			"xdg-open",
			handle->template,
			NULL
		};

		d2tk_util_kill(&handle->kid);
		handle->kid = d2tk_util_spawn(argv);
		if(handle->kid <= 0)
		{
			lv2_log_error(&handle->logger, "[%s] failed to spawn: %s '%s'", __func__,
				argv[0], argv[1]);
		}
	}
	if(d2tk_state_is_over(state))
	{
		d2tk_base_set_tooltip(base, sizeof(tip), tip, handle->tip_height);
	}

	d2tk_util_wait(&handle->kid);
}

static void
_update_code(plughandle_t *handle, const char *txt, size_t txt_len)
{
	ser_atom_t ser;
	ser_atom_init(&ser);
	ser_atom_reset(&ser, &handle->forge);

	lv2_atom_forge_string(&handle->forge, txt, txt_len);

	const LV2_Atom *atom = ser_atom_get(&ser);

	props_impl_t *impl = _props_impl_get(&handle->props, handle->urid_code);
	_props_impl_set(&handle->props, impl, atom->type, atom->size,
		LV2_ATOM_BODY_CONST(atom));

	ser_atom_deinit(&ser);

	_message_set_key(handle, handle->urid_code);
}

static void
_expose_text_clear(plughandle_t *handle, const d2tk_rect_t *rect)
{
	d2tk_frontend_t *dpugl = handle->dpugl;
	d2tk_base_t *base = d2tk_frontend_get_base(dpugl);

	static const char path [] = "delete.png";
	static const char none [] = "";
	static const char tip [] = "clear";

	const d2tk_state_t state = d2tk_base_button_image(base, D2TK_ID,
		sizeof(path), path, rect);

	if(d2tk_state_is_changed(state))
	{
		_update_code(handle, none, sizeof(none) - 1);
	}
	if(d2tk_state_is_over(state))
	{
		d2tk_base_set_tooltip(base, sizeof(tip), tip, handle->tip_height);
	}
}

static void
_expose_text_copy(plughandle_t *handle, const d2tk_rect_t *rect)
{
	d2tk_frontend_t *dpugl = handle->dpugl;
	d2tk_base_t *base = d2tk_frontend_get_base(dpugl);

	static const char path [] = "copy.png";
	static const char tip [] = "copy to clipboard";

	const d2tk_state_t state = d2tk_base_button_image(base, D2TK_ID,
		sizeof(path), path, rect);

	if(d2tk_state_is_changed(state))
	{
		d2tk_frontend_set_clipboard(dpugl, "UTF8_STRING",
			handle->state.code, strlen(handle->state.code) + 1);
	}
	if(d2tk_state_is_over(state))
	{
		d2tk_base_set_tooltip(base, sizeof(tip), tip, handle->tip_height);
	}
}

static void
_expose_text_paste(plughandle_t *handle, const d2tk_rect_t *rect)
{
	d2tk_frontend_t *dpugl = handle->dpugl;
	d2tk_base_t *base = d2tk_frontend_get_base(dpugl);

	static const char path [] = "clipboard.png";
	static const char tip [] = "paste from clipboard";

	const d2tk_state_t state = d2tk_base_button_image(base, D2TK_ID,
		sizeof(path), path, rect);

	if(d2tk_state_is_changed(state))
	{
		size_t txt_len = 0;
		const char *mime = "UTF8_STRING";
		const char *txt = d2tk_frontend_get_clipboard(dpugl, &mime, &txt_len);

		if(txt && txt_len && mime && !strcmp(mime, "UTF8_STRING"))
		{
			_update_code(handle, txt, txt_len);
		}
		else
		{
			lv2_log_error(&handle->logger, "[%s] failed to paste text: %s", __func__,
				mime);
		}
	}
	if(d2tk_state_is_over(state))
	{
		d2tk_base_set_tooltip(base, sizeof(tip), tip, handle->tip_height);
	}
}

#ifdef _LV2_HAS_REQUEST_VALUE
static void
_expose_text_load(plughandle_t *handle, const d2tk_rect_t *rect)
{
	d2tk_frontend_t *dpugl = handle->dpugl;
	d2tk_base_t *base = d2tk_frontend_get_base(dpugl);

	static const char path [] = "save.png";
	static const char tip [] = "load from file";

	if(!handle->request_code)
	{
		return;
	}

	const d2tk_state_t state = d2tk_base_button_image(base, D2TK_ID,
		sizeof(path), path, rect);

	if(d2tk_state_is_changed(state))
	{
		const LV2_URID key = handle->urid_code;
		const LV2_URID type = handle->forge.String;

		const LV2UI_Request_Value_Status status = handle->request_code->request(
			handle->request_code->handle, key, type, NULL);

		if(  (status != LV2UI_REQUEST_VALUE_SUCCESS)
			&& (status != LV2UI_REQUEST_VALUE_BUSY) )
		{
			lv2_log_error(&handle->logger, "[%s] requestValue failed: %i", __func__, status);

			if(status == LV2UI_REQUEST_VALUE_ERR_UNSUPPORTED)
			{
				handle->request_code = NULL;
			}
		}
	}
	if(d2tk_state_is_over(state))
	{
		d2tk_base_set_tooltip(base, sizeof(tip), tip, handle->tip_height);
	}
}
#endif

static inline void
_expose_footer(plughandle_t *handle, const d2tk_rect_t *rect)
{
	const d2tk_coord_t frac [8] = {
		rect->h, 0, 0, 0, rect->h, rect->h, rect->h, rect->h
	};
	D2TK_BASE_LAYOUT(rect, 8, frac, D2TK_FLAG_LAYOUT_X_ABS, lay)
	{
		const unsigned k = d2tk_layout_get_index(lay);
		const d2tk_rect_t *lrect = d2tk_layout_get_rect(lay);

		switch(k)
		{
			case 0:
			{
				_expose_panic(handle, lrect);
			} break;
			case 1:
			{
				_expose_text_link(handle, lrect);
			} break;
			case 2:
			{
				_expose_xfade(handle, lrect);
			} break;
			case 3:
			{
				_expose_font_height(handle, lrect);
			} break;
			case 4:
			{
				_expose_text_clear(handle, lrect);
			} break;
			case 5:
			{
				_expose_text_copy(handle, lrect);
			} break;
			case 6:
			{
				_expose_text_paste(handle, lrect);
			} break;
			case 7:
			{
#ifdef _LV2_HAS_REQUEST_VALUE
				_expose_text_load(handle, lrect);
#endif
			} break;
		}
	}
}

static inline void
_expose_slot(plughandle_t *handle, const d2tk_rect_t *rect, unsigned k)
{
	d2tk_frontend_t *dpugl = handle->dpugl;
	d2tk_base_t *base = d2tk_frontend_get_base(dpugl);

	cntrl_type_t type = handle->state.control_type[k];

	if(  (handle->state.control_min[k] == 0.f)
		&& (handle->state.control_max[k] == 0.f) )
	{
		type = CNTRL_NONE;
	}

	switch(type)
	{
		case CNTRL_BUTTON:
		{
			bool val = handle->state.control[k] > 0.5;

			const d2tk_state_t state = d2tk_base_spinner_bool(base, D2TK_ID_IDX(k), rect,
				-1, handle->state.control_label[k], &val, D2TK_FLAG_NONE);

			if(d2tk_state_is_down(state))
			{
				handle->state.control[k] = 1;

				_message_set_control(handle, k);
			}
			else if(d2tk_state_is_up(state))
			{
				handle->state.control[k] = 0;

				_message_set_control(handle, k);
			}
		} break;
		case CNTRL_CHECK_BUTTON:
		{
			bool val = handle->state.control[k] > 0.5;

			if(d2tk_base_spinner_bool_is_changed(base, D2TK_ID_IDX(k), rect,
				-1, handle->state.control_label[k], &val, D2TK_FLAG_NONE))
			{
				handle->state.control[k] = val;

				_message_set_control(handle, k);
			}
		} break;

		case CNTRL_VERTICAL_SLIDER:
			// fall-through
		case CNTRL_HORIZONTAL_SLIDER:
			// fall-through
		case CNTRL_NUM_ENTRY:
		{
			const float min = handle->state.control_min[k];
			const float max = handle->state.control_max[k];
			const float range = max - min; //FIXME cache this somewhere
			float abs = handle->state.control[k] * range + min;

			if(d2tk_base_spinner_float_is_changed(base, D2TK_ID_IDX(k), rect,
				-1, handle->state.control_label[k], min, &abs, max, D2TK_FLAG_NONE))
			{
				handle->state.control[k] = (abs - min) / range;

				_message_set_control(handle, k);
			}
		} break;

		case CNTRL_VERTICAL_BARGRAPH:
			// fall-through
		case CNTRL_HORIZONTAL_BARGRAPH:
		{
			const float min = 0.f;
			const float max = 1.f;

			d2tk_base_spinner_wave_float(base, D2TK_ID_IDX(k), rect,
				-1, handle->state.control_label[k],
				min, handle->wavs[k].vals, WAV_MAX, max);
		} break;

		case CNTRL_NONE:
			// fall-through
		case CNTRL_SOUND_FILE:
		{
			char lbl [32];
			const size_t lbl_len = snprintf(lbl, sizeof(lbl), "- slot•%02d empty -", k);

			d2tk_base_label(base, lbl_len, lbl, 0.375f, rect, D2TK_ALIGN_CENTERED);
		} break;
	}
}

static inline void
_expose_sidebar_right(plughandle_t *handle, const d2tk_rect_t *rect)
{
	d2tk_frontend_t *dpugl = handle->dpugl;
	d2tk_base_t *base = d2tk_frontend_get_base(dpugl);

	const uint32_t numv = rect->h / handle->item_height;
	const uint32_t max [2] = { 0, NCONTROLS };
	const uint32_t num [2] = { 0, numv };
	D2TK_BASE_SCROLLBAR(base, rect, D2TK_ID, D2TK_FLAG_SCROLL_Y, max, num, vscroll)
	{
		const unsigned offy = d2tk_scrollbar_get_offset_y(vscroll);
		const d2tk_rect_t *vrect = d2tk_scrollbar_get_rect(vscroll);

		D2TK_BASE_TABLE(vrect, 1, numv,  D2TK_FLAG_TABLE_REL, tab)
		{
			const unsigned k = d2tk_table_get_index(tab) + offy;
			const d2tk_rect_t *trect = d2tk_table_get_rect(tab);

			if(k >= NCONTROLS)
			{
				break;
			}

			_expose_slot(handle, trect, k);
		}
	}
}

/* list of tested console editors:
 *
 * e3
 * joe
 * nano
 * vi
 * vis
 * vim
 * neovim
 * emacs
 * zile
 * mg
 * kakoune
 */

/* list of tested graphical editors:
 *
 * acme
 * adie
 * beaver
 * deepin-editor
 * gedit         (does not work properly)
 * gobby
 * howl
 * jedit         (does not work properly)
 * xed           (does not work properly)
 * leafpad
 * mousepad
 * nedit
 * notepadqq
 * pluma         (does not work properly)
 * sublime3      (needs to be started with -w)
 */

static inline void
_expose_term(plughandle_t *handle, const d2tk_rect_t *rect)
{
	d2tk_frontend_t *dpugl = handle->dpugl;
	d2tk_base_t *base = d2tk_frontend_get_base(dpugl);

	char *editor = getenv("EDITOR");

	char *args [] = {
		editor ? editor : "vi",
		handle->template,
		NULL
	};

	d2tk_flag_t flag = D2TK_FLAG_NONE;
	if(handle->reinit)
	{
		flag = D2TK_FLAG_PTY_REINIT;
	}

	D2TK_BASE_PTY(base, D2TK_ID, NULL, args, handle->font_height, rect, flag, pty)
	{
		const d2tk_state_t state = d2tk_pty_get_state(pty);
		const uint32_t max_red = d2tk_pty_get_max_red(pty);

		if(max_red != handle->max_red)
		{
			handle->max_red = max_red;
			d2tk_frontend_redisplay(handle->dpugl);
		}

		if(d2tk_state_is_close(state))
		{
			handle->done = 1;
		}
	}

	handle->reinit = false;
}

static inline unsigned
_num_lines(const char *err)
{
	unsigned nlines = 1;

	for(const char *from = err, *to = strchr(from, '\n');
		to;
		from = &to[1],
		to = strchr(from, '\n'))
	{
		nlines += 1;
	}

	return nlines;
}

static inline void
_expose_error(plughandle_t *handle, const d2tk_rect_t *rect)
{
	d2tk_frontend_t *dpugl = handle->dpugl;
	d2tk_base_t *base = d2tk_frontend_get_base(dpugl);

	const char *from = handle->state.error;
	const unsigned nlines = _num_lines(from);

	//FIXME wrap in scroll widget
	D2TK_BASE_TABLE(rect, rect->w, handle->font_height, D2TK_FLAG_TABLE_ABS, tab)
	{
		const d2tk_rect_t *trect = d2tk_table_get_rect(tab);
		const unsigned k = d2tk_table_get_index(tab);

		if(k >= nlines)
		{
			break;
		}

		const char *to = strchr(from, '\n');
		const size_t len = to ? to - from : -1;

		d2tk_base_label(base, len, from, 1.f, trect,
			D2TK_ALIGN_LEFT | D2TK_ALIGN_MIDDLE);

		from = &to[1];
	}
}

static inline void
_expose_editor(plughandle_t *handle, const d2tk_rect_t *rect)
{
	const size_t err_len = strlen(handle->state.error);
	const unsigned n = err_len > 0 ? 2 : 1;
	const d2tk_coord_t frac [2] = { 2, 1 };
	D2TK_BASE_LAYOUT(rect, n, frac, D2TK_FLAG_LAYOUT_Y_REL, lay)
	{
		const unsigned k = d2tk_layout_get_index(lay);
		const d2tk_rect_t *lrect = d2tk_layout_get_rect(lay);

		switch(k)
		{
			case 0:
			{
				_expose_term(handle, lrect);
			} break;
			case 1:
			{
				_expose_error(handle, lrect);
			} break;
		}
	}
}

static inline void
_expose_upper(plughandle_t *handle, const d2tk_rect_t *rect)
{
	d2tk_frontend_t *dpugl = handle->dpugl;
	d2tk_base_t *base = d2tk_frontend_get_base(dpugl);

	const d2tk_coord_t frac [3] = { 0, 5, handle->sidebar_width };
	D2TK_BASE_LAYOUT(rect, 3, frac, D2TK_FLAG_LAYOUT_X_ABS, lay)
	{
		const unsigned k = d2tk_layout_get_index(lay);
		const d2tk_rect_t *lrect = d2tk_layout_get_rect(lay);

		switch(k)
		{
			case 0:
			{
				_expose_editor(handle, lrect);
			} break;
			case 1:
			{
				d2tk_base_separator(base, lrect, D2TK_FLAG_SEPARATOR_X);
			} break;
			case 2:
			{
				_expose_sidebar_right(handle, lrect);
			} break;
		}
	}
}

static inline void
_expose_body(plughandle_t *handle, const d2tk_rect_t *rect)
{
	const d2tk_coord_t frac [2] = { 0, handle->footer_height };
	D2TK_BASE_LAYOUT(rect, 2, frac, D2TK_FLAG_LAYOUT_Y_ABS, lay)
	{
		const unsigned k = d2tk_layout_get_index(lay);
		const d2tk_rect_t *lrect = d2tk_layout_get_rect(lay);

		switch(k)
		{
			case 0:
			{
				_expose_upper(handle, lrect);
			} break;
			case 1:
			{
				_expose_footer(handle, lrect);
			} break;
		}
	}
}

static int
_expose(void *data, d2tk_coord_t w, d2tk_coord_t h)
{
	plughandle_t *handle = data;
	d2tk_base_t *base = d2tk_frontend_get_base(handle->dpugl);
	const d2tk_rect_t rect = D2TK_RECT(0, 0, w, h);

	const d2tk_style_t *old_style = d2tk_base_get_style(base);
	d2tk_style_t style = *old_style;
	const uint32_t light = handle->max_red;
	const uint32_t dark = (light & ~0xff) | 0x7f;
	style.fill_color[D2TK_TRIPLE_ACTIVE]           = dark;
	style.fill_color[D2TK_TRIPLE_ACTIVE_HOT]       = light;
	style.fill_color[D2TK_TRIPLE_ACTIVE_FOCUS]     = dark;
	style.fill_color[D2TK_TRIPLE_ACTIVE_HOT_FOCUS] = light;
	style.text_stroke_color[D2TK_TRIPLE_HOT]       = light;
	style.text_stroke_color[D2TK_TRIPLE_HOT_FOCUS] = light;

	d2tk_base_set_style(base, &style);

	const d2tk_coord_t frac [2] = { handle->header_height, 0 };
	D2TK_BASE_LAYOUT(&rect, 2, frac, D2TK_FLAG_LAYOUT_Y_ABS, lay)
	{
		const unsigned k = d2tk_layout_get_index(lay);
		const d2tk_rect_t *lrect = d2tk_layout_get_rect(lay);

		switch(k)
		{
			case 0:
			{
				_expose_header(handle, lrect);
			} break;
			case 1:
			{
				_expose_body(handle, lrect);
			} break;
		}
	}

	d2tk_base_set_style(base, old_style);

	return 0;
}

static LV2UI_Handle
instantiate(const LV2UI_Descriptor *descriptor,
	const char *plugin_uri,
	const char *bundle_path,
	LV2UI_Write_Function write_function,
	LV2UI_Controller controller, LV2UI_Widget *widget,
	const LV2_Feature *const *features)
{
	plughandle_t *handle = calloc(1, sizeof(plughandle_t));
	if(!handle)
		return NULL;

	_reset_wavs(handle);

	void *parent = NULL;
	LV2UI_Resize *host_resize = NULL;
	LV2_Options_Option *opts = NULL;
	for(int i=0; features[i]; i++)
	{
		if(!strcmp(features[i]->URI, LV2_UI__parent))
		{
			parent = features[i]->data;
		}
		else if(!strcmp(features[i]->URI, LV2_UI__resize))
		{
			host_resize = features[i]->data;
		}
		else if(!strcmp(features[i]->URI, LV2_URID__map))
		{
			handle->map = features[i]->data;
		}
		else if(!strcmp(features[i]->URI, LV2_LOG__log))
		{
			handle->log = features[i]->data;
		}
		else if(!strcmp(features[i]->URI, LV2_OPTIONS__options))
		{
			opts = features[i]->data;
		}
#ifdef _LV2_HAS_REQUEST_VALUE
		else if(!strcmp(features[i]->URI, LV2_UI__requestValue))
		{
			handle->request_code = features[i]->data;
		}
#endif
	}

	if(!parent)
	{
		fprintf(stderr,
			"%s: Host does not support ui:parent\n", descriptor->URI);
		free(handle);
		return NULL;
	}
	if(!handle->map)
	{
		fprintf(stderr,
			"%s: Host does not support urid:map\n", descriptor->URI);
		free(handle);
		return NULL;
	}

	if(handle->log)
	{
		lv2_log_logger_init(&handle->logger, handle->map, handle->log);
	}

	lv2_atom_forge_init(&handle->forge, handle->map);

	handle->atom_eventTransfer = handle->map->map(handle->map->handle,
		LV2_ATOM__eventTransfer);
	handle->midi_MidiEvent = handle->map->map(handle->map->handle,
		LV2_MIDI__MidiEvent);
	handle->urid_code = handle->map->map(handle->map->handle,
		MEPHISTO__code);
	handle->urid_error = handle->map->map(handle->map->handle,
		MEPHISTO__error);
	handle->urid_xfadeDuration = handle->map->map(handle->map->handle,
		MEPHISTO__xfadeDuration);
	handle->urid_fontHeight = handle->map->map(handle->map->handle,
		MEPHISTO__fontHeight);
	handle->urid_control[0] = handle->map->map(handle->map->handle,
		MEPHISTO__control_1);
	handle->urid_control[1] = handle->map->map(handle->map->handle,
		MEPHISTO__control_2);
	handle->urid_control[2] = handle->map->map(handle->map->handle,
		MEPHISTO__control_3);
	handle->urid_control[3] = handle->map->map(handle->map->handle,
		MEPHISTO__control_4);
	handle->urid_control[4] = handle->map->map(handle->map->handle,
		MEPHISTO__control_5);
	handle->urid_control[5] = handle->map->map(handle->map->handle,
		MEPHISTO__control_6);
	handle->urid_control[6] = handle->map->map(handle->map->handle,
		MEPHISTO__control_7);
	handle->urid_control[7] = handle->map->map(handle->map->handle,
		MEPHISTO__control_8);
	handle->urid_control[8] = handle->map->map(handle->map->handle,
		MEPHISTO__control_9);
	handle->urid_control[9] = handle->map->map(handle->map->handle,
		MEPHISTO__control_10);
	handle->urid_control[10] = handle->map->map(handle->map->handle,
		MEPHISTO__control_11);
	handle->urid_control[11] = handle->map->map(handle->map->handle,
		MEPHISTO__control_12);
	handle->urid_control[12] = handle->map->map(handle->map->handle,
		MEPHISTO__control_13);
	handle->urid_control[13] = handle->map->map(handle->map->handle,
		MEPHISTO__control_14);
	handle->urid_control[14] = handle->map->map(handle->map->handle,
		MEPHISTO__control_15);
	handle->urid_control[15] = handle->map->map(handle->map->handle,
		MEPHISTO__control_16);

	if(!props_init(&handle->props, plugin_uri,
		defs, MAX_NPROPS, &handle->state, &handle->stash,
		handle->map, handle))
	{
		fprintf(stderr, "failed to initialize property structure\n");
		free(handle);
		return NULL;
	}

	handle->controller = controller;
	handle->writer = write_function;

	const d2tk_coord_t w = 1024;
	const d2tk_coord_t h = 720;

	d2tk_pugl_config_t *config = &handle->config;
	config->parent = (uintptr_t)parent;
	config->bundle_path = bundle_path;
	config->min_w = w/2;
	config->min_h = h/2;
	config->w = w;
	config->h = h;
	config->fixed_size = false;
	config->fixed_aspect = false;
	config->expose = _expose;
	config->data = handle;

	handle->dpugl = d2tk_pugl_new(config, (uintptr_t *)widget);
	if(!handle->dpugl)
	{
		free(handle);
		return NULL;
	}

	const LV2_URID ui_scaleFactor = handle->map->map(handle->map->handle,
		LV2_UI__scaleFactor);
	const LV2_URID params_sample_rate = handle->map->map(handle->map->handle,
		LV2_PARAMETERS__sampleRate);

	// fall-back
	handle->sample_rate = 48000.f;

	for(LV2_Options_Option *opt = opts;
		opt && (opt->key != 0) && (opt->value != NULL);
		opt++)
	{
		if( (opt->key == ui_scaleFactor) && (opt->type == handle->forge.Float) )
		{
			handle->scale = *(float*)opt->value;
		}
		else if( (opt->key == params_sample_rate) && (opt->type == handle->forge.Float) )
		{
			handle->sample_rate = *(float*)opt->value;
		}
	}

	if(handle->scale == 0.f)
	{
		handle->scale = d2tk_frontend_get_scale(handle->dpugl);
	}

	handle->header_height = 32 * handle->scale;
	handle->footer_height = 32 * handle->scale;
	handle->tip_height = 20 * handle->scale;
	handle->sidebar_width = 256 * handle->scale;
	handle->item_height = 40 * handle->scale;

	handle->state.font_height = 16;
	_update_font_height(handle);

	if(host_resize)
	{
		host_resize->ui_resize(host_resize->handle, w, h);
	}

	strncpy(handle->template, "/tmp/XXXXXX.dsp", sizeof(handle->template));
	handle->fd = mkstemps(handle->template, 4);
	if(handle->fd == -1)
	{
		free(handle);
		return NULL;
	}

	lv2_log_note(&handle->logger, "template: %s\n", handle->template);

	for(unsigned i = 0; i < MAX_NPROPS; i++)
	{
		const props_def_t *def = &defs[i];
		const LV2_URID urid = props_map(&handle->props, def->property);

		_message_get(handle, urid);
	}

	return handle;
}

static void
cleanup(LV2UI_Handle instance)
{
	plughandle_t *handle = instance;

	d2tk_util_kill(&handle->kid);
	d2tk_frontend_free(handle->dpugl);

	unlink(handle->template);
	close(handle->fd);
	free(handle);
}

static void
port_event(LV2UI_Handle instance, uint32_t index __attribute__((unused)),
	uint32_t size __attribute__((unused)), uint32_t protocol, const void *buf)
{
	plughandle_t *handle = instance;
	const LV2_Atom_Object *obj = buf;

	if(protocol != handle->atom_eventTransfer)
	{
		return;
	}

	ser_atom_t ser;
	ser_atom_init(&ser);
	ser_atom_reset(&ser, &handle->forge);

	LV2_Atom_Forge_Ref ref = 0;
	props_advance(&handle->props, &handle->forge, 0, obj, &ref);

	ser_atom_deinit(&ser);

	d2tk_frontend_redisplay(handle->dpugl);
}

static void
_file_read(plughandle_t *handle)
{
	lseek(handle->fd, 0, SEEK_SET);
	const size_t len = lseek(handle->fd, 0, SEEK_END);

	lseek(handle->fd, 0, SEEK_SET);

	read(handle->fd, handle->state.code, len);
	handle->state.code[len] = '\0';

	handle->hash = d2tk_hash(handle->state.code, len);

	_message_set_code(handle, len + 1);
}

static int
_idle(LV2UI_Handle instance)
{
	plughandle_t *handle = instance;

	struct stat st;
	if(stat(handle->template, &st) == -1)
	{
		lv2_log_error(&handle->logger, "stat: %s\n", strerror(errno));
	}

	if( (st.st_mtime > handle->modtime) && (handle->modtime > 0) )
	{
		_file_read(handle);

		handle->modtime = st.st_mtime;
	}

	if(d2tk_frontend_step(handle->dpugl))
	{
		handle->done = 1;
	}

	return handle->done;
}

static const LV2UI_Idle_Interface idle_ext = {
	.idle = _idle
};

static int
_resize(LV2UI_Handle instance, int width, int height)
{
	plughandle_t *handle = instance;

	return d2tk_frontend_set_size(handle->dpugl, width, height);
}

static const LV2UI_Resize resize_ext = {
	.ui_resize = _resize
};

static const void *
extension_data(const char *uri)
{
	if(!strcmp(uri, LV2_UI__idleInterface))
		return &idle_ext;
	else if(!strcmp(uri, LV2_UI__resize))
		return &resize_ext;
		
	return NULL;
}

static const LV2UI_Descriptor mephisto_ui= {
	.URI            = MEPHISTO__ui,
	.instantiate    = instantiate,
	.cleanup        = cleanup,
	.port_event     = port_event,
	.extension_data = extension_data
};

LV2_SYMBOL_EXPORT const LV2UI_Descriptor*
lv2ui_descriptor(uint32_t index)
{
	switch(index)
	{
		case 0:
			return &mephisto_ui;
		default:
			return NULL;
	}
}
