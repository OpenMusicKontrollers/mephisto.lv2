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

#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <mephisto.h>
#include <private_ui.h>

#include "lv2_external_ui.h" // kxstudio external-ui extension

#include <props.h>
#define SER_ATOM_IMPLEMENTATION
#include <ser_atom.lv2/ser_atom.h>

typedef struct _ui_t ui_t;

struct _ui_t {
	LV2_URID mephisto_code;
	LV2_URID atom_eventTransfer;

	LV2_Log_Log *log;
	LV2_Log_Logger logger;

	LV2_Atom_Forge forge;
	LV2_URID_Map *map;

	LV2UI_Write_Function writer;
	LV2UI_Controller controller;

	LV2UI_Port_Map *port_map;
	uint32_t control_port;
	uint32_t notify_port;

	struct stat stat;
	int done;

	spawn_t spawn;

	struct {
		LV2_External_UI_Widget widget;
		const LV2_External_UI_Host *host;
	} kx;

	plugstate_t state;
	plugstate_t stash;

	PROPS_T(props, MAX_NPROPS);

	char path [PATH_MAX];
};

static const LV2UI_Descriptor simple_ui;
static const LV2UI_Descriptor simple_kx;

static void
_intercept_code(void *data, int64_t frames __attribute__((unused)),
	props_impl_t *impl)
{
	ui_t *ui = data;

	FILE *f = fopen(ui->path, "wb");
	if(f)
	{
		if( (fwrite(ui->state.code, impl->value.size - 1, 1, f) != 1)
			&& ui->log )
		{
			lv2_log_error(&ui->logger, "simple_ui: fwrite failed\n");
		}

		fclose(f);

		if(stat(ui->path, &ui->stat) < 0) // update modification timestamp
			lv2_log_error(&ui->logger, "simple_ui: stat failed\n");
	}
	else if(ui->log)
	{
		lv2_log_error(&ui->logger, "simple_ui: fopen failed\n");
	}
}

static void
_intercept_control(void *data __attribute__((unused)),
	int64_t frames __attribute__((unused)),
	props_impl_t *impl __attribute__((unused)))
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
_mephisto_message_send(ui_t *handle, LV2_URID key, const char *str, uint32_t size)
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

	if(str)
	{
		impl->value.size = size;
		memcpy(handle->state.code, str, size);

		props_set(&handle->props, &handle->forge, 0, key, &ref);
	}
	else
	{
		props_get(&handle->props, &handle->forge, 0, key, &ref);
	}

	const LV2_Atom_Event *ev = (const LV2_Atom_Event *)ser_atom_get(&ser);
	const LV2_Atom *atom = &ev->body;
	handle->writer(handle->controller, 0, lv2_atom_total_size(atom),
		handle->atom_eventTransfer, atom);

	ser_atom_deinit(&ser);
}

static void
_load_chosen(ui_t *ui, const char *path)
{
	if(!path)
		return;

	// load file
	FILE *f = fopen(path, "rb");
	if(f)
	{
		fseek(f, 0, SEEK_END);
		long fsize = ftell(f);
		fseek(f, 0, SEEK_SET);

		char *str = malloc(fsize + 1);
		if(str)
		{
			if(fread(str, fsize, 1, f) == 1)
			{
				str[fsize] = 0;
				_mephisto_message_send(ui, ui->mephisto_code, str, fsize + 1);
			}

			free(str);
		}

		fclose(f);
	}
}

// Show Interface
static inline int
_show_cb(LV2UI_Handle instance)
{
	ui_t *ui = instance;

	if(!ui->done)
		return 0; // already showing

#if defined(_WIN32)
	const char *command = "cmd /c start /wait";
#elif defined(__APPLE__)
	const char *command = "open -nW";
#else // Linux/BSD
	//const char *command = "xdg-open";
	const char *command = "xterm -e vi";
#endif

	// get default editor from environment
	const char *mephisto_editor = getenv("MEPHISTO_EDITOR");
	if(!mephisto_editor)
		mephisto_editor = getenv("EDITOR");
	if(!mephisto_editor)
		mephisto_editor = command;
	char *dup = strdup(mephisto_editor);
	char **args = dup ? _spawn_parse_env(dup, ui->path) : NULL;

	const int status = _spawn_spawn(&ui->spawn, args);

	if(args)
		free(args);
	if(dup)
		free(dup);

	if(status)
		return -1; // failed to spawn

	ui->done = 0;

	return 0;
}

static inline int
_hide_cb(LV2UI_Handle instance)
{
	ui_t *ui = instance;

	if(_spawn_has_child(&ui->spawn))
	{
		_spawn_kill(&ui->spawn);

		_spawn_waitpid(&ui->spawn, true);

		_spawn_invalidate_child(&ui->spawn);
	}

	ui->done = 1;

	return 0;
}

static const LV2UI_Show_Interface show_ext = {
	.show = _show_cb,
	.hide = _hide_cb
};

// Idle interface
static inline int
_idle_cb(LV2UI_Handle instance)
{
	ui_t *ui = instance;

	if(_spawn_has_child(&ui->spawn))
	{
		int res;
		if((res = _spawn_waitpid(&ui->spawn, false)) < 0)
		{
			_spawn_invalidate_child(&ui->spawn);
			ui->done = 1; // xdg-open may return immediately
		}
	}

	if(!ui->done)
	{
		struct stat stat1;
		memset(&stat1, 0x0, sizeof(struct stat));

		if(stat(ui->path, &stat1) < 0)
		{
			lv2_log_error(&ui->logger, "simple_ui: stat failed\n");

			ui->done = 1; // no file or other error
		}
		else if(stat1.st_mtime != ui->stat.st_mtime)
		{
			_load_chosen(ui, ui->path);

			ui->stat = stat1; // update stat
		}
	}

	return ui->done;
}

static const LV2UI_Idle_Interface idle_ext = {
	.idle = _idle_cb
};

// External-ui_t Interface
static inline void
_kx_run(LV2_External_UI_Widget *widget)
{
	ui_t *handle = (ui_t *)((uint8_t *)widget - offsetof(ui_t, kx.widget));

	if(_idle_cb(handle))
	{
		if(handle->kx.host && handle->kx.host->ui_closed)
			handle->kx.host->ui_closed(handle->controller);
		_hide_cb(handle);
	}
}

static inline void
_kx_hide(LV2_External_UI_Widget *widget)
{
	ui_t *handle = (ui_t *)((uint8_t *)widget - offsetof(ui_t, kx.widget));

	_hide_cb(handle);
}

static inline void
_kx_show(LV2_External_UI_Widget *widget)
{
	ui_t *handle = (ui_t *)((uint8_t *)widget - offsetof(ui_t, kx.widget));

	_show_cb(handle);
}

static LV2UI_Handle
instantiate(const LV2UI_Descriptor *descriptor, const char *plugin_uri,
	const char *bundle_path __attribute__((unused)),
	LV2UI_Write_Function write_function,
	LV2UI_Controller controller, LV2UI_Widget *widget,
	const LV2_Feature *const *features)
{
	ui_t *ui = calloc(1, sizeof(ui_t));
	if(!ui)
		return NULL;

	for(int i=0; features[i]; i++)
	{
		if(!strcmp(features[i]->URI, LV2_URID__map))
			ui->map = features[i]->data;
		else if(!strcmp(features[i]->URI, LV2_UI__portMap))
			ui->port_map = features[i]->data;
		else if(!strcmp(features[i]->URI, LV2_LOG__log))
			ui->log = features[i]->data;
		else if(!strcmp(features[i]->URI, LV2_EXTERNAL_UI__Host) && (descriptor == &simple_kx))
			ui->kx.host = features[i]->data;
	}

	ui->kx.widget.run = _kx_run;
	ui->kx.widget.show = _kx_show;
	ui->kx.widget.hide = _kx_hide;

	if(descriptor == &simple_kx)
		*(LV2_External_UI_Widget **)widget = &ui->kx.widget;
	else
		*(void **)widget = NULL;

	if(!ui->map)
	{
		fprintf(stderr, "%s: Host does not support urid:map\n", descriptor->URI);
		free(ui);
		return NULL;
	}
	if(!ui->port_map)
	{
		fprintf(stderr, "%s: Host does not support ui:portMap\n", descriptor->URI);
		free(ui);
		return NULL;
	}

	// query port index of "control" port
	ui->control_port = ui->port_map->port_index(ui->port_map->handle, "control");
	ui->notify_port = ui->port_map->port_index(ui->port_map->handle, "notify");

	ui->mephisto_code = ui->map->map(ui->map->handle, MEPHISTO__code);
	ui->atom_eventTransfer = ui->map->map(ui->map->handle, LV2_ATOM__eventTransfer);

	lv2_atom_forge_init(&ui->forge, ui->map);
	if(ui->log)
		lv2_log_logger_init(&ui->logger, ui->map, ui->log);

	if(!props_init(&ui->props, plugin_uri,
		defs, MAX_NPROPS, &ui->state, &ui->stash,
		ui->map, ui))
	{
		fprintf(stderr, "failed to initialize property structure\n");
		free(ui);
		return NULL;
	}

	ui->writer = write_function;
	ui->controller = controller;

	char *tmp_template;
#if defined(_WIN32)
	char tmp_dir[MAX_PATH + 1];
	GetTempPath(MAX_PATH + 1, tmp_dir);
	const char *sep = tmp_dir[strlen(tmp_dir) - 1] == '\\' ? "" : "\\";
#else
	const char *tmp_dir = P_tmpdir;
	const char *sep = tmp_dir[strlen(tmp_dir) - 1] == '/' ? "" : "/";
#endif
	asprintf(&tmp_template, "%s%smephisto_XXXXXX.dsp", tmp_dir, sep);

	if(!tmp_template)
	{
		fprintf(stderr, "%s: out of memory\n", descriptor->URI);
		free(ui);
		return NULL;
	}

	int fd = mkstemps(tmp_template, 4);
	if(fd)
		close(fd);

	snprintf(ui->path, sizeof(ui->path), "%s", tmp_template);

	if(ui->log)
		lv2_log_note(&ui->logger, "simple_ui: opening %s\n", ui->path);

	if(stat(ui->path, &ui->stat) < 0) // update modification timestamp
		lv2_log_error(&ui->logger, "simple_ui: stat failed\n");

	free(tmp_template);

	_mephisto_message_send(ui, ui->mephisto_code, NULL, 0);
	ui->done = 1;

	return ui;
}

static void
cleanup(LV2UI_Handle handle)
{
	ui_t *ui = handle;

	unlink(ui->path);

	if(ui->log)
		lv2_log_note(&ui->logger, "simple_ui: closing %s\n", ui->path);

	free(ui);
}

static void
port_event(LV2UI_Handle instance, uint32_t index __attribute__((unused)),
	uint32_t size __attribute__((unused)), uint32_t protocol, const void *buf)
{
	ui_t *handle = instance;

	if(protocol == handle->atom_eventTransfer)
	{
		const LV2_Atom_Object *obj = buf;

		ser_atom_t ser;
		ser_atom_init(&ser);
		ser_atom_reset(&ser, &handle->forge);

		LV2_Atom_Forge_Ref ref = 0;
		props_advance(&handle->props, &handle->forge, 0, obj, &ref);

		ser_atom_deinit(&ser);
	}
}

static const void *
ui_extension_data(const char *uri)
{
	if(!strcmp(uri, LV2_UI__idleInterface))
	{
		return &idle_ext;
	}
	else if(!strcmp(uri, LV2_UI__showInterface))
	{
		return &show_ext;
	}
		
	return NULL;
}

static const LV2UI_Descriptor simple_ui = {
	.URI						= MEPHISTO__showUI,
	.instantiate		= instantiate,
	.cleanup				= cleanup,
	.port_event			= port_event,
	.extension_data	= ui_extension_data
};

static const LV2UI_Descriptor simple_kx = {
	.URI						= MEPHISTO__kxUI,
	.instantiate		= instantiate,
	.cleanup				= cleanup,
	.port_event			= port_event,
	.extension_data	= NULL
};

LV2_SYMBOL_EXPORT const LV2UI_Descriptor*
lv2ui_descriptor(uint32_t index)
{
	switch(index)
	{
		case 0:
			return &simple_ui;
		case 1:
			return &simple_kx;
		default:
			return NULL;
	}
}
