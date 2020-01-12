/*
 * Copyright (c) 2018-2019 Hanspeter Portner (dev@open-music-kontrollers.ch)
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
#include <string.h>
#include <time.h>

#if defined(__APPLE__)
// FIXME
#elif defined(_WIN32)
# include <windows.h>
#else
# include <X11/Xresource.h>
#endif

#include <pugl/pugl.h>
#if defined(PUGL_HAVE_CAIRO)
#	include <pugl/pugl_cairo_backend.h>
#else
#	include <pugl/pugl_gl_backend.h>
#endif

#include "core_internal.h"
#include <d2tk/frontend_pugl.h>

#include <d2tk/backend.h>

struct _d2tk_pugl_t {
	const d2tk_pugl_config_t *config;
	bool done;
	PuglWorld *world;
	PuglView *view;
	d2tk_base_t *base;
	void *ctx;
};

static inline void
_d2tk_pugl_close(d2tk_pugl_t *dpugl)
{
	dpugl->done = true;
}

static inline void
_d2tk_pugl_expose(d2tk_pugl_t *dpugl)
{
	d2tk_base_t *base = dpugl->base;

	d2tk_coord_t w;
	d2tk_coord_t h;
	d2tk_base_get_dimensions(base, &w, &h);

	d2tk_base_pre(base);

	dpugl->config->expose(dpugl->config->data, w, h);

	d2tk_base_post(base);
}

static void
_d2tk_pugl_modifiers(d2tk_pugl_t *dpugl, unsigned state)
{
	d2tk_base_t *base = dpugl->base;

	d2tk_base_set_modmask(base, D2TK_MODMASK_SHIFT,
		(state & PUGL_MOD_SHIFT) ? true : false);
	d2tk_base_set_modmask(base, D2TK_MODMASK_CTRL,
		(state & PUGL_MOD_CTRL) ? true : false);
	d2tk_base_set_modmask(base, D2TK_MODMASK_ALT,
		(state & PUGL_MOD_ALT) ? true : false);
}

static PuglStatus
_d2tk_pugl_event_func(PuglView *view, const PuglEvent *e)
{
	d2tk_pugl_t *dpugl = puglGetHandle(view);
	d2tk_base_t *base = dpugl->base;

	switch(e->type)
	{
		case PUGL_CONFIGURE:
		{
			d2tk_coord_t w, h;
			d2tk_base_get_dimensions(base, &w, &h);

			// only redisplay if size has changed
			if( (w == e->configure.width) && (h == e->configure.height) )
			{
				break;
			}

			d2tk_base_set_dimensions(base, e->configure.width, e->configure.height);
			puglPostRedisplay(dpugl->view);
		}	break;
		case PUGL_EXPOSE:
		{
			_d2tk_pugl_expose(dpugl);
		}	break;
		case PUGL_CLOSE:
		{
			_d2tk_pugl_close(dpugl);
		}	break;

		case PUGL_FOCUS_IN:
			// fall-through
		case PUGL_FOCUS_OUT:
		{
			d2tk_base_set_full_refresh(base);

			puglPostRedisplay(dpugl->view);
		} break;

		case PUGL_ENTER_NOTIFY:
		case PUGL_LEAVE_NOTIFY:
		{
			_d2tk_pugl_modifiers(dpugl, e->crossing.state);
			d2tk_base_set_mouse_pos(base, e->crossing.x, e->crossing.y);
			d2tk_base_set_full_refresh(base);

			puglPostRedisplay(dpugl->view);
		} break;

		case PUGL_BUTTON_PRESS:
		case PUGL_BUTTON_RELEASE:
		{
			_d2tk_pugl_modifiers(dpugl, e->button.state);
			d2tk_base_set_mouse_pos(base, e->button.x, e->button.y);

			switch(e->button.button)
			{
				case 3:
				{
					d2tk_base_set_butmask(base, D2TK_BUTMASK_RIGHT,
						(e->type == PUGL_BUTTON_PRESS) );
				} break;
				case 2:
				{
					d2tk_base_set_butmask(base, D2TK_BUTMASK_MIDDLE,
						(e->type == PUGL_BUTTON_PRESS) );
				} break;
				case 1:
					// fall-through
				default:
				{
					d2tk_base_set_butmask(base, D2TK_BUTMASK_LEFT,
						(e->type == PUGL_BUTTON_PRESS) );
				} break;
			}

			puglPostRedisplay(dpugl->view);
		} break;

		case PUGL_MOTION_NOTIFY:
		{
			_d2tk_pugl_modifiers(dpugl, e->motion.state);
			d2tk_base_set_mouse_pos(base, e->motion.x, e->motion.y);

			puglPostRedisplay(dpugl->view);
		} break;

		case PUGL_SCROLL:
		{
			_d2tk_pugl_modifiers(dpugl, e->scroll.state);
			d2tk_base_set_mouse_pos(base, e->scroll.x, e->scroll.y);
			d2tk_base_add_mouse_scroll(base, e->scroll.dx, e->scroll.dy);

			puglPostRedisplay(dpugl->view);
		} break;

		case PUGL_KEY_PRESS:
		{
			_d2tk_pugl_modifiers(dpugl, e->key.state);

			bool handled = false;

			switch(e->key.key)
			{
				case PUGL_KEY_BACKSPACE:
				{
					d2tk_base_set_keymask(base, D2TK_KEYMASK_BACKSPACE, true);
					handled = true;
				} break;
				case PUGL_KEY_TAB:
				{
					d2tk_base_set_keymask(base, D2TK_KEYMASK_TAB, true);
					handled = true;
				} break;
				case PUGL_KEY_RETURN:
				{
					d2tk_base_set_keymask(base, D2TK_KEYMASK_ENTER, true);
					handled = true;
				} break;
				case PUGL_KEY_ESCAPE:
				{
					d2tk_base_set_keymask(base, D2TK_KEYMASK_ESCAPE, true);
					handled = true;
				} break;
				case PUGL_KEY_DELETE:
				{
					d2tk_base_set_keymask(base, D2TK_KEYMASK_DEL, true);
					handled = true;
				} break;

				case PUGL_KEY_LEFT:
				{
					d2tk_base_set_keymask(base, D2TK_KEYMASK_LEFT, true);
					handled = true;
				} break;
				case PUGL_KEY_RIGHT:
				{
					d2tk_base_set_keymask(base, D2TK_KEYMASK_RIGHT, true);
					handled = true;
				} break;
				case PUGL_KEY_UP:
				{
					d2tk_base_set_keymask(base, D2TK_KEYMASK_UP, true);
					handled = true;
				} break;
				case PUGL_KEY_DOWN:
				{
					d2tk_base_set_keymask(base, D2TK_KEYMASK_DOWN, true);
					handled = true;
				} break;

				case PUGL_KEY_PAGE_UP:
				{
					d2tk_base_set_keymask(base, D2TK_KEYMASK_PAGEUP, true);
					handled = true;
				} break;
				case PUGL_KEY_PAGE_DOWN:
				{
					d2tk_base_set_keymask(base, D2TK_KEYMASK_PAGEDOWN, true);
					handled = true;
				} break;
				case PUGL_KEY_HOME:
				{
					d2tk_base_set_keymask(base, D2TK_KEYMASK_HOME, true);
					handled = true;
				} break;
				case PUGL_KEY_END:
				{
					d2tk_base_set_keymask(base, D2TK_KEYMASK_END, true);
					handled = true;
				} break;
				case PUGL_KEY_INSERT:
				{
					d2tk_base_set_keymask(base, D2TK_KEYMASK_INS, true);
					handled = true;
				} break;

				case PUGL_KEY_SHIFT:
				{
					d2tk_base_set_modmask(base, D2TK_MODMASK_SHIFT, true);
					handled = true;
				} break;
				case PUGL_KEY_CTRL:
				{
					d2tk_base_set_modmask(base, D2TK_MODMASK_CTRL, true);
					handled = true;
				} break;
				case PUGL_KEY_ALT:
				{
					d2tk_base_set_modmask(base, D2TK_MODMASK_ALT, true);
					handled = true;
				} break;
				default:
				{
					// nothing
				} break;
			}

			if(handled)
			{
				puglPostRedisplay(dpugl->view);
			}
		} break;
		case PUGL_KEY_RELEASE:
		{
			_d2tk_pugl_modifiers(dpugl, e->key.state);

			bool handled = false;

			switch(e->key.key)
			{
				case PUGL_KEY_BACKSPACE:
				{
					d2tk_base_set_keymask(base, D2TK_KEYMASK_BACKSPACE, false);
					handled = true;
				} break;
				case PUGL_KEY_TAB:
				{
					d2tk_base_set_keymask(base, D2TK_KEYMASK_TAB, false);
					handled = true;
				} break;
				case PUGL_KEY_RETURN:
				{
					d2tk_base_set_keymask(base, D2TK_KEYMASK_ENTER, false);
					handled = true;
				} break;
				case PUGL_KEY_ESCAPE:
				{
					d2tk_base_set_keymask(base, D2TK_KEYMASK_ESCAPE, false);
					handled = true;
				} break;
				case PUGL_KEY_DELETE:
				{
					d2tk_base_set_keymask(base, D2TK_KEYMASK_DEL, false);
					handled = true;
				} break;

				case PUGL_KEY_LEFT:
				{
					d2tk_base_set_keymask(base, D2TK_KEYMASK_LEFT, false);
					handled = true;
				} break;
				case PUGL_KEY_RIGHT:
				{
					d2tk_base_set_keymask(base, D2TK_KEYMASK_RIGHT, false);
					handled = true;
				} break;
				case PUGL_KEY_UP:
				{
					d2tk_base_set_keymask(base, D2TK_KEYMASK_UP, false);
					handled = true;
				} break;
				case PUGL_KEY_DOWN:
				{
					d2tk_base_set_keymask(base, D2TK_KEYMASK_DOWN, false);
					handled = true;
				} break;

				case PUGL_KEY_PAGE_UP:
				{
					d2tk_base_set_keymask(base, D2TK_KEYMASK_PAGEUP, false);
					handled = true;
				} break;
				case PUGL_KEY_PAGE_DOWN:
				{
					d2tk_base_set_keymask(base, D2TK_KEYMASK_PAGEDOWN, false);
					handled = true;
				} break;
				case PUGL_KEY_HOME:
				{
					d2tk_base_set_keymask(base, D2TK_KEYMASK_HOME, false);
					handled = true;
				} break;
				case PUGL_KEY_END:
				{
					d2tk_base_set_keymask(base, D2TK_KEYMASK_END, false);
					handled = true;
				} break;
				case PUGL_KEY_INSERT:
				{
					d2tk_base_set_keymask(base, D2TK_KEYMASK_INS, true);
					handled = true;
				} break;

				case PUGL_KEY_SHIFT:
				{
					d2tk_base_set_modmask(base, D2TK_MODMASK_SHIFT, false);
					handled = true;
				} break;
				case PUGL_KEY_CTRL:
				{
					d2tk_base_set_modmask(base, D2TK_MODMASK_CTRL, false);
					handled = true;
				} break;
				case PUGL_KEY_ALT:
				{
					d2tk_base_set_modmask(base, D2TK_MODMASK_ALT, false);
					handled = true;
				} break;
				default:
				{
					// nothing
				} break;
			}

			if(handled)
			{
				puglPostRedisplay(dpugl->view);
			}
		} break;
		case PUGL_TEXT:
		{
			d2tk_base_append_utf8(base, e->text.character);

			puglPostRedisplay(dpugl->view);
		} break;
		case PUGL_NOTHING:
		{
			// nothing
		}	break;
	}

	return PUGL_SUCCESS;
}

D2TK_API int
d2tk_pugl_step(d2tk_pugl_t *dpugl)
{
	d2tk_base_probe(dpugl->base);

	if(d2tk_base_get_again(dpugl->base))
	{
		puglPostRedisplay(dpugl->view);
	}

	const PuglStatus stat = puglDispatchEvents(dpugl->world);
	(void)stat;

	return dpugl->done;
}

D2TK_API int
d2tk_pugl_poll(d2tk_pugl_t *dpugl, double timeout)
{
	const PuglStatus stat = puglPollEvents(dpugl->world, timeout);
	(void)stat;

	return dpugl->done;
}

D2TK_API void
d2tk_pugl_run(d2tk_pugl_t *dpugl, const sig_atomic_t *done)
{
	const unsigned fps = 25;
	const double timeout = 1.0 / fps;

	while(!*done)
	{
		if(d2tk_pugl_poll(dpugl, timeout))
		{
			break;
		}

		if(d2tk_pugl_step(dpugl))
		{
			break;
		}
	}
}

D2TK_API void
d2tk_pugl_free(d2tk_pugl_t *dpugl)
{
	if(dpugl->ctx)
	{
		puglEnterContext(dpugl->view, false);
		if(dpugl->base)
		{
			d2tk_base_free(dpugl->base);
		}
		d2tk_core_driver.free(dpugl->ctx);
		puglLeaveContext(dpugl->view, false);
	}

	if(dpugl->world)
	{
		if(dpugl->view)
		{
			if(puglGetVisible(dpugl->view))
			{
				puglHideWindow(dpugl->view);
			}
			puglFreeView(dpugl->view);
		}
		puglFreeWorld(dpugl->world);
	}

	free(dpugl);
}

D2TK_API float
d2tk_pugl_get_scale()
{
	const char *D2TK_SCALE = getenv("D2TK_SCALE");
	const float scale = D2TK_SCALE ? atof(D2TK_SCALE) : 1.f;
	const float dpi0 = 96.f; // reference DPI we're designing for
	float dpi1 = dpi0;

#if defined(__APPLE__)
	// FIXME
#elif defined(_WIN32)
	// GetDpiForSystem/Monitor/Window is Win10 only
	HDC screen = GetDC(NULL);
	dpi1 = GetDeviceCaps(screen, LOGPIXELSX);
	ReleaseDC(NULL, screen);
#else
	Display *disp = XOpenDisplay(0);
	if(disp)
	{
		// modern X actually lies here, but proprietary nvidia
		dpi1 = XDisplayWidth(disp, 0) * 25.4f / XDisplayWidthMM(disp, 0);

		// read DPI from users's ~/.Xresources
		char *resource_string = XResourceManagerString(disp);
		XrmInitialize();
		if(resource_string)
		{
			XrmDatabase db = XrmGetStringDatabase(resource_string);
			if(db)
			{
				char *type = NULL;
				XrmValue value;

				XrmGetResource(db, "Xft.dpi", "String", &type, &value);
				if(value.addr)
				{
					dpi1 = atof(value.addr);
				}
			}
		}

		XCloseDisplay(disp);
	}
#endif

	return scale * dpi1 / dpi0;
}

D2TK_API d2tk_pugl_t *
d2tk_pugl_new(const d2tk_pugl_config_t *config, uintptr_t *widget)
{
	d2tk_pugl_t *dpugl = calloc(1, sizeof(d2tk_pugl_t));
	if(!dpugl)
	{
		goto fail;
	}

	dpugl->config = config;

	dpugl->world = puglNewWorld();
	if(!dpugl->world)
	{
		fprintf(stderr, "puglNewWorld failed\n");
		goto fail;
	}

	puglSetClassName(dpugl->world, "d2tk");

	dpugl->view = puglNewView(dpugl->world);
	if(!dpugl->view)
	{
		fprintf(stderr, "puglNewView failed\n");
		goto fail;
	}

	const PuglRect frame = {
		.x = 0,
		.y = 0,
		.width = config->w,
		.height = config->h
	};

	puglSetFrame(dpugl->view, frame);
	if(config->min_w && config->min_h)
	{
		puglSetMinSize(dpugl->view, config->min_w, config->min_h);
	}
	if(config->parent)
	{
		puglSetParentWindow(dpugl->view, config->parent);
#if 0 // not yet implemented for mingw, darwin
		puglSetTransientFor(dpugl->view, config->parent);
#endif
	}
	if(config->fixed_aspect)
	{
		puglSetAspectRatio(dpugl->view, config->w, config->h,
			config->w, config->h);
	}
	puglSetViewHint(dpugl->view, PUGL_RESIZABLE, !config->fixed_size);
	puglSetHandle(dpugl->view, dpugl);
	puglSetEventFunc(dpugl->view, _d2tk_pugl_event_func);

#if defined(PUGL_HAVE_CAIRO)
	puglSetBackend(dpugl->view, puglCairoBackend());
#else
	puglSetBackend(dpugl->view, puglGlBackend());
#endif
	const int stat = puglCreateWindow(dpugl->view, "d2tk");

	if(stat != 0)
	{
		fprintf(stderr, "puglCreateWindow failed\n");
		goto fail;
	}
	puglShowWindow(dpugl->view);

	if(widget)
	{
		*widget = puglGetNativeWindow(dpugl->view);
	}

	puglEnterContext(dpugl->view, false);
	void *ctx = puglGetContext(dpugl->view);
	dpugl->ctx = d2tk_core_driver.new(config->bundle_path, ctx);
	puglLeaveContext(dpugl->view, false);

	if(!dpugl->ctx)
	{
		goto fail;
	}

	dpugl->base = d2tk_base_new(&d2tk_core_driver, dpugl->ctx);
	if(!dpugl->base)
	{
		goto fail;
	}

	d2tk_base_set_dimensions(dpugl->base, config->w, config->h);

	return dpugl;

fail:
	if(dpugl)
	{
		if(dpugl->world)
		{
			if(dpugl->view)
			{
				if(dpugl->ctx)
				{
					puglEnterContext(dpugl->view, false);
					d2tk_core_driver.free(dpugl->ctx);
					puglLeaveContext(dpugl->view, false);
				}

				puglFreeView(dpugl->view);
			}
			puglFreeWorld(dpugl->world);
		}

		free(dpugl);
	}

	return NULL;
}

D2TK_API void
d2tk_pugl_redisplay(d2tk_pugl_t *dpugl)
{
	puglPostRedisplay(dpugl->view);
}

D2TK_API int
d2tk_pugl_set_size(d2tk_pugl_t *dpugl, d2tk_coord_t w, d2tk_coord_t h)
{
	d2tk_base_set_dimensions(dpugl->base, w, h);
	puglPostRedisplay(dpugl->view);

	return 0;
}

D2TK_API int
d2tk_pugl_get_size(d2tk_pugl_t *dpugl, d2tk_coord_t *w, d2tk_coord_t *h)
{
	const PuglRect rect = puglGetFrame(dpugl->view);

	if(w)
	{
		*w = rect.width;
	}

	if(h)
	{
		*h = rect.height;
	}

	return 0;
}

D2TK_API d2tk_base_t *
d2tk_pugl_get_base(d2tk_pugl_t *dpugl)
{
	return dpugl->base;
}
