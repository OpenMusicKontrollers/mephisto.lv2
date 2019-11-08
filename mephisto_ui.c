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

#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <math.h>

#include <mephisto.h>
#include <props.h>

#define SER_ATOM_IMPLEMENTATION
#include <ser_atom.lv2/ser_atom.h>

#include <d2tk/frontend_pugl.h>

#define GLYPH_W 7
#define GLYPH_H (GLYPH_W * 2)

#define HEADER 32
#define SIDEBAR 150

#define FPS 25

#define DEFAULT_FG 0xddddddff
#define DEFAULT_BG 0x222222ff

#define NROWS_MAX 512
#define NCOLS_MAX 512

#define MAX(x, y) (x > y ? y : x)

typedef struct _plughandle_t plughandle_t;

struct _plughandle_t {
	LV2_URID_Map *map;
	LV2_Atom_Forge forge;

	LV2_Log_Log *log;
	LV2_Log_Logger logger;

	d2tk_pugl_config_t config;
	d2tk_pugl_t *dpugl;

	LV2UI_Controller *controller;
	LV2UI_Write_Function writer;

	PROPS_T(props, MAX_NPROPS);

	plugstate_t state;
	plugstate_t stash;

	LV2_URID atom_eventTransfer;
	LV2_URID urid_code;
	LV2_URID urid_error;
	LV2_URID urid_control [NCONTROLS];
};

static void
_intercept_code(void *data, int64_t frames __attribute__((unused)),
	props_impl_t *impl __attribute__((unused)))
{
	plughandle_t *handle = data;

	(void)handle; //FIXME
}

static void
_intercept_error(void *data, int64_t frames __attribute__((unused)),
	props_impl_t *impl __attribute__((unused)))
{
	plughandle_t *handle = data;

	(void)handle; //FIXME
}

static void
_intercept_control(void *data __attribute__((unused)),
	int64_t frames __attribute__((unused)), props_impl_t *impl __attribute__((unused)))
{
	// nothing to do, yet
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
		.event_cb = _intercept_error,
		.max_size = ERROR_SIZE
	},
	{
		.property = MEPHISTO__xfadeDuration,
		.offset = offsetof(plugstate_t, xfade_dur),
		.type = LV2_ATOM__Int
	},
	{
		.property = MEPHISTO__releaseDuration,
		.offset = offsetof(plugstate_t, release_dur),
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

#if 0
static void
_message_set_str(plughandle_t *handle, LV2_URID key, const char *str, uint32_t size)
{
	ser_atom_t ser;
	props_impl_t *impl = _props_impl_get(&handle->props, key);
	if(!impl || !str)
	{
		return;
	}

	ser_atom_init(&ser);
	ser_atom_reset(&ser, &handle->forge);

	LV2_Atom_Forge_Ref ref = 1;

	impl->value.size = size;
	memcpy(handle->state.code, str, size);

	props_set(&handle->props, &handle->forge, 0, key, &ref);

	const LV2_Atom_Event *ev = (const LV2_Atom_Event *)ser_atom_get(&ser);
	const LV2_Atom *atom = &ev->body;
	handle->writer(handle->controller, 0, lv2_atom_total_size(atom),
		handle->atom_eventTransfer, atom);

	ser_atom_deinit(&ser);
}
#endif

static void
_message_set_control(plughandle_t *handle, unsigned k)
{
	const LV2_URID key = handle->urid_control[k];

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
	d2tk_pugl_t *dpugl = handle->dpugl;
	d2tk_base_t *base = d2tk_pugl_get_base(dpugl);

	const d2tk_coord_t frac [3] = { 1, 2, 1 }; 
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

static inline void
_expose_slot(plughandle_t *handle, const d2tk_rect_t *rect, unsigned k)
{
	d2tk_pugl_t *dpugl = handle->dpugl;
	d2tk_base_t *base = d2tk_pugl_get_base(dpugl);

	static const char lbl [16][9] = {
		"slot•01",
		"slot•02",
		"slot•03",
		"slot•04",
		"slot•05",
		"slot•06",
		"slot•07",
		"slot•08",
		"slot•09",
		"slot•10",
		"slot•11",
		"slot•12",
		"slot•13",
		"slot•14",
		"slot•15",
		"slot•16",
	};

	D2TK_BASE_FRAME(base, rect, sizeof(lbl[k]), lbl[k], frm)
	{
		const d2tk_rect_t *frect = d2tk_frame_get_rect(frm);

		if(d2tk_base_dial_float_is_changed(base, D2TK_ID_IDX(k), frect,
			0.f, &handle->state.control[k], 1.f))
		{
			_message_set_control(handle, k);
		}
	}
}

static inline void
_expose_sidebar(plughandle_t *handle, const d2tk_rect_t *rect)
{
	D2TK_BASE_TABLE(rect, 2, 8,  D2TK_FLAG_TABLE_REL, tab)
	{
		const unsigned k = d2tk_table_get_index(tab);
		const d2tk_rect_t *trect = d2tk_table_get_rect(tab);

		_expose_slot(handle, trect, k);
	}
}

static inline void
_expose_body(plughandle_t *handle, const d2tk_rect_t *rect)
{
	const d2tk_coord_t frac [2] = { 0, SIDEBAR }; 
	D2TK_BASE_LAYOUT(rect, 2, frac, D2TK_FLAG_LAYOUT_X_ABS, lay)
	{
		const unsigned k = d2tk_layout_get_index(lay);
		const d2tk_rect_t *lrect = d2tk_layout_get_rect(lay);

		switch(k)
		{
			case 0:
			{
				//_expose_term(handle, lrect);
			} break;
			case 1:
			{
				_expose_sidebar(handle, lrect);
			} break;
		}
	}
}

static int
_expose(void *data, d2tk_coord_t w, d2tk_coord_t h)
{
	plughandle_t *handle = data;
	const d2tk_rect_t rect = D2TK_RECT(0, 0, w, h);

	const d2tk_coord_t frac [2] = { HEADER, 0 }; 
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

	return 0;
}

static LV2UI_Handle
instantiate(const LV2UI_Descriptor *descriptor,
	const char *plugin_uri,
	const char *bundle_path __attribute__((unused)),
	LV2UI_Write_Function write_function,
	LV2UI_Controller controller, LV2UI_Widget *widget,
	const LV2_Feature *const *features)
{
	plughandle_t *handle = calloc(1, sizeof(plughandle_t));
	if(!handle)
		return NULL;

	void *parent = NULL;
	LV2UI_Resize *host_resize = NULL;
	for(int i=0; features[i]; i++)
	{
		if(!strcmp(features[i]->URI, LV2_UI__parent))
			parent = features[i]->data;
		else if(!strcmp(features[i]->URI, LV2_UI__resize))
			host_resize = features[i]->data;
		else if(!strcmp(features[i]->URI, LV2_URID__map))
			handle->map = features[i]->data;
		else if(!strcmp(features[i]->URI, LV2_LOG__log))
			handle->log = features[i]->data;
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
		lv2_log_logger_init(&handle->logger, handle->map, handle->log);

	lv2_atom_forge_init(&handle->forge, handle->map);

	handle->atom_eventTransfer = handle->map->map(handle->map->handle,
		LV2_ATOM__eventTransfer);
	handle->urid_code = handle->map->map(handle->map->handle,
		MEPHISTO__code);
	handle->urid_error = handle->map->map(handle->map->handle,
		MEPHISTO__error);
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

	const d2tk_coord_t w = 800;
	const d2tk_coord_t h = 450;

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

	if(host_resize)
	{
		host_resize->ui_resize(host_resize->handle, w, h);
	}

	_message_get(handle, handle->urid_code);
	_message_get(handle, handle->urid_error);
	//FIXME controls

	return handle;
}

static void
cleanup(LV2UI_Handle instance)
{
	plughandle_t *handle = instance;

	d2tk_pugl_free(handle->dpugl);

	free(handle);
}

static void
port_event(LV2UI_Handle instance, uint32_t index __attribute__((unused)),
	uint32_t size __attribute__((unused)), uint32_t protocol, const void *buf)
{
	plughandle_t *handle = instance;

	if(protocol != handle->atom_eventTransfer)
	{
		return;
	}

	const LV2_Atom_Object *obj = buf;

	ser_atom_t ser;
	ser_atom_init(&ser);
	ser_atom_reset(&ser, &handle->forge);

	LV2_Atom_Forge_Ref ref = 0;
	props_advance(&handle->props, &handle->forge, 0, obj, &ref);

	ser_atom_deinit(&ser);

	d2tk_pugl_redisplay(handle->dpugl);
}

static int
_idle(LV2UI_Handle instance)
{
	plughandle_t *handle = instance;

	d2tk_base_t *base = d2tk_pugl_get_base(handle->dpugl);
	d2tk_style_t style = *d2tk_base_get_default_style(base);
	/*
	style.fill_color[D2TK_TRIPLE_ACTIVE] = handle.dark_reddest;
	style.fill_color[D2TK_TRIPLE_ACTIVE_HOT] = handle.light_reddest;
	style.fill_color[D2TK_TRIPLE_ACTIVE_FOCUS] = handle.dark_reddest;
	style.fill_color[D2TK_TRIPLE_ACTIVE_HOT_FOCUS] = handle.light_reddest;
	*/
	style.font_face = "FiraCode-Regular.ttf";
	d2tk_base_set_style(base, &style);

	return d2tk_pugl_step(handle->dpugl);
}

static const LV2UI_Idle_Interface idle_ext = {
	.idle = _idle
};

static int
_resize(LV2UI_Handle instance, int width, int height)
{
	plughandle_t *handle = instance;

	return d2tk_pugl_set_size(handle->dpugl, width, height);
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
