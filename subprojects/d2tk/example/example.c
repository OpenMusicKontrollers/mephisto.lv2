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
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <inttypes.h>
#include <dirent.h>
#include <string.h>

#include <d2tk/frontend_pugl.h>
#include "example/example.h"

#if D2TK_EVDEV
#	include <libevdev/libevdev.h>
#	include <libevdev/libevdev-uinput.h>

typedef struct _fake_t fake_t;

struct _fake_t {
	struct libevdev *dev;
	struct libevdev_uinput *uidev;
};

static fake_t fake = {
	.dev = NULL,
	.uidev = NULL
};
#endif

typedef union _val_t val_t;

union _val_t {
	bool b;
	int32_t i;
	int64_t h;
	float f;
	double d;
	char s [32];
};

typedef enum _bar_t {
	BAR_MIX,
	BAR_SEQ,
	BAR_TABLE_ABS,
	BAR_SCROLL,
	BAR_PANE,
	BAR_LAYOUT,
	BAR_FLOWMATRIX,
	BAR_METER,
	BAR_FRAME,
	BAR_UTF8,
#if D2TK_PTY
	BAR_PTY,
#endif
#if !defined(_WIN32) && !defined(__APPLE__)
	BAR_BROWSER,
#endif
#if D2TK_EVDEV
	BAR_KEYBOARD,
#endif

	BAR_MAX
} bar_t;

static bar_t bar = BAR_MIX;
static const char *bar_lbl [BAR_MAX] = {
	[BAR_MIX]        = "Mix of many",
	[BAR_SEQ]        = "Sequencer",
	[BAR_TABLE_ABS]  = "Table Abs",
	[BAR_SCROLL]     = "Scrollbar",
	[BAR_PANE]       = "Pane",
	[BAR_LAYOUT]     = "Layout",
	[BAR_FLOWMATRIX] = "Flowmatrix",
	[BAR_METER]      = "Meter",
	[BAR_FRAME]      = "Frame",
	[BAR_UTF8]       = "UTF-8",
#if D2TK_PTY
	[BAR_PTY]       = "PTY",
#endif
#if !defined(_WIN32) && !defined(__APPLE__)
	[BAR_BROWSER]    = "Browser",
#endif
#if D2TK_EVDEV
	[BAR_KEYBOARD]   = "Keyboard"
#endif
};

static inline void
_render_c_mix(d2tk_base_t *base, const d2tk_rect_t *rect)
{
#define N 15
#define M 24
	static val_t value [N*M];

	D2TK_BASE_TABLE(rect, N, M, D2TK_FLAG_TABLE_REL, tab)
	{
		const unsigned k = d2tk_table_get_index(tab);
		const d2tk_rect_t *bnd = d2tk_table_get_rect(tab);
		const d2tk_id_t id = D2TK_ID_IDX(k);
		val_t *val = &value[k];

		switch(k % N)
		{
			case 0:
			{
				char lbl [32];
				const size_t lbl_len = snprintf(lbl, sizeof(lbl), "%03X", k);

				if(d2tk_base_toggle_label_is_changed(
					base, id, lbl_len, lbl, D2TK_ALIGN_CENTERED, bnd, &val->b))
				{
					fprintf(stdout, "toggle %016"PRIx64" %s\n", id, val->b ? "ON" : "OFF");
				};
			} break;
			case 1:
			{
				char lbl [32];
				const size_t lbl_len = snprintf(lbl, sizeof(lbl), "%03X", k);

				if(d2tk_base_button_label_is_changed(
					base, id, lbl_len, lbl, D2TK_ALIGN_CENTERED, bnd))
				{
					fprintf(stdout, "button %016"PRIx64" DOWN\n", id);
				}
			} break;
			case 2:
			{
				if(d2tk_base_dial_bool_is_changed(
					base, id, bnd, &val->b))
				{
					fprintf(stdout, "dial %016"PRIx64" %s\n", id, val->b ? "ON" : "OFF");
				}
			} break;
			case 3:
			{
				if(d2tk_base_dial_int32_is_changed(
					base, id, bnd, -5, &val->i, 5))
				{
					fprintf(stdout, "dial %016"PRIx64" %"PRIi32"\n", id, val->i);
				}
			} break;
			case 4:
			{
				if(d2tk_base_dial_int64_is_changed(
					base, id, bnd, -10, &val->h, 10))
				{
					fprintf(stdout, "dial %016"PRIx64" %"PRIi64"\n", id, val->h);
				}
			} break;
			case 5:
			{
				if(d2tk_base_dial_float_is_changed(
					base, id, bnd, -1.f, &val->f, 1.f))
				{
					fprintf(stdout, "dial %016"PRIx64" %f\n", id, val->f);
				}
			} break;
			case 6:
			{
				if(d2tk_base_dial_double_is_changed(
					base, id, bnd, -10.0, &val->d, 10.0))
				{
					fprintf(stdout, "dial %016"PRIx64" %lf\n", id, val->d);
				}
			} break;
			case 7:
			{
				if(d2tk_base_text_field_is_changed(
					base, id, bnd, 32, val->s, D2TK_ALIGN_LEFT | D2TK_ALIGN_MIDDLE, NULL))
				{
					fprintf(stdout, "text %016"PRIx64" %s\n", id, val->s);
				}
			} break;
			case 8:
			{
				if(d2tk_base_prop_int32_is_changed(
					base, id, bnd, -5, &val->i, 5))
				{
					fprintf(stdout, "prop %016"PRIx64" %"PRIi32"\n", id, val->i);
				}
			} break;
			case 9:
			{
				if(d2tk_base_prop_float_is_changed(
					base, id, bnd, -1.f, &val->f, 1.f))
				{
					fprintf(stdout, "prop %016"PRIx64" %f\n", id, val->f);
				}
			} break;
			case 10:
			{
				char lbl [32];
				const size_t lbl_len = snprintf(lbl, sizeof(lbl), "%03X", k);

				d2tk_base_label(base, lbl_len, lbl, 0.5f, bnd, D2TK_ALIGN_CENTERED);
			} break;
			case 11:
			{
#define NITMS 5
				static const char *itms [NITMS] = {
					"one",
					"two",
					"three",
					"four",
					"five"
				};
				if(d2tk_base_combo_is_changed(base, id, NITMS, itms, bnd, &val->i))
				{
					fprintf(stdout, "combo %016"PRIx64" %s (%"PRIi32")\n", id,
						itms[val->i], val->i);
				}
#undef NITMS
			} break;
			case 12:
			{
				d2tk_base_image(base, -1, "libre-gui-folder.png", bnd,
					D2TK_ALIGN_CENTERED);
			} break;
			case 13:
			{
				static const uint32_t argb [4] = {
					0xffffcf00, 0x7f7f6700,
					0x7f7f6700, 0x00000000
				};
				static const uint64_t rev = 0;

				d2tk_base_bitmap(base, 2, 2, 2*sizeof(uint32_t), argb, rev, bnd,
					D2TK_ALIGN_CENTERED);
			} break;
			case 14:
			{
				char lbl [32];
				const size_t lbl_len = snprintf(lbl, sizeof(lbl), "%03X", k);

				if(d2tk_base_link_is_changed(base, id, lbl_len, lbl, 0.5f, bnd,
					D2TK_ALIGN_MIDDLE | D2TK_ALIGN_LEFT))
				{
					fprintf(stdout, "link %016"PRIx64" DOWN\n", id);
				}
			} break;
			default:
			{
				// nothing to do
			} break;
		}
	}
#undef M
#undef N
}

static inline void
_render_c_seq(d2tk_base_t *base, const d2tk_rect_t *rect)
{
#define N (8*12)
#define NN (N*N)
	const unsigned M = N * rect->h / rect->w;
	static val_t value [N*N];
	static bool drag = false;
	static d2tk_pos_t from_pos;
	static d2tk_pos_t to_pos;

	d2tk_style_t style = *d2tk_base_get_default_style();
	style.border_width = 1;
	style.padding = 0;
	style.rounding = 0;
	d2tk_base_set_style(base, &style);

	D2TK_BASE_TABLE(rect, N, M, D2TK_FLAG_TABLE_REL, tab)
	{
		const unsigned k = d2tk_table_get_index(tab);
		const d2tk_coord_t x = d2tk_table_get_index_x(tab);
		const d2tk_coord_t y = d2tk_table_get_index_y(tab);
		const d2tk_rect_t *bnd = d2tk_table_get_rect(tab);
		const d2tk_id_t id = D2TK_ID_IDX(k);

		if(k >= NN)
		{
			break;
		}

		val_t *val = &value[k];

		if(y == 0)
		{
			char lbl [32];
			const size_t lbl_len = snprintf(lbl, sizeof(lbl), "%u", x % 10);
			d2tk_base_label(base, lbl_len, lbl, 1.f, bnd, D2TK_ALIGN_CENTERED);

			continue;
		}
		else if(x == 0)
		{
			char lbl [32];
			const size_t lbl_len = snprintf(lbl, sizeof(lbl), "%u", y % 10);
			d2tk_base_label(base, lbl_len, lbl, 1.f, bnd, D2TK_ALIGN_CENTERED);

			continue;
		}

		bool clone = val->b;

		const uint32_t col_active = style.fill_color[D2TK_TRIPLE_ACTIVE];

		if(drag)
		{
			const d2tk_coord_t y0 = from_pos.y <= to_pos.y ? from_pos.y : to_pos.y;
			const d2tk_coord_t y1 = from_pos.y >  to_pos.y ? from_pos.y : to_pos.y;
			const d2tk_coord_t x0 = from_pos.x <= to_pos.x ? from_pos.x : to_pos.x;
			const d2tk_coord_t x1 = from_pos.x >  to_pos.x ? from_pos.x : to_pos.x;

			for(d2tk_coord_t Y = y0; Y <= y1; Y++)
			{
				for(d2tk_coord_t X = x0; X <= x1; X++)
				{
					if( (y == from_pos.y) && (x == from_pos.x) )
					{
						continue;
					}

					if( (y != Y) || (x != X) )
					{
						continue;
					}

					clone = true;

					style.fill_color[D2TK_TRIPLE_ACTIVE] = 0x9f00cfff;
					break;
				}
			}
		}

		const d2tk_state_t state = d2tk_base_toggle(base, id, bnd, &clone);

		style.fill_color[D2TK_TRIPLE_ACTIVE] = col_active;

		if(d2tk_state_is_changed(state))
		{
			val->b = clone;
		}

		if(d2tk_state_is_down(state))
		{
			drag = true;

			from_pos.x = to_pos.x = x;
			from_pos.y = to_pos.y = y;

			fprintf(stdout, "toggle %016"PRIx64" DOWN\n", id);
		}

		if(drag && d2tk_state_is_over(state))
		{
			to_pos.x = x;
			to_pos.y = y;
		}

		if(d2tk_state_is_up(state))
		{
			drag = false;

			const d2tk_coord_t y0 = from_pos.y <= to_pos.y ? from_pos.y : to_pos.y;
			const d2tk_coord_t y1 = from_pos.y >  to_pos.y ? from_pos.y : to_pos.y;
			const d2tk_coord_t x0 = from_pos.x <= to_pos.x ? from_pos.x : to_pos.x;
			const d2tk_coord_t x1 = from_pos.x >  to_pos.x ? from_pos.x : to_pos.x;

			for(d2tk_coord_t Y = y0; Y <= y1; Y++)
			{
				const d2tk_coord_t O = N*Y;

				for(d2tk_coord_t X = x0; X <= x1; X++)
				{
					const d2tk_coord_t K = O + X;

					value[K] = *val;
				}
			}

			fprintf(stdout, "toggle %016"PRIx64" UP\n", id);
		}
	}

	d2tk_base_set_default_style(base);
#undef NN
#undef N
}

static inline void
_render_c_table_abs(d2tk_base_t *base, const d2tk_rect_t *rect)
{
	static int32_t ntile = 64;

	if(d2tk_base_get_modmask(base, D2TK_MODMASK_CTRL, false))
	{
		if(d2tk_base_get_keymask(base, D2TK_KEYMASK_LEFT, true))
		{
			ntile >>= 1;
		}
		if(d2tk_base_get_keymask(base, D2TK_KEYMASK_UP, true))
		{
			ntile <<= 1;
		}
	}

	d2tk_clip_int32(16, &ntile, 256);

	D2TK_BASE_TABLE(rect, ntile, ntile, D2TK_FLAG_TABLE_ABS, tab)
	{
		const d2tk_rect_t *rect = d2tk_table_get_rect(tab);
		const unsigned k = d2tk_table_get_index(tab);

		d2tk_base_button(base, D2TK_ID_IDX(k), rect);
	}
}

static inline void
_render_c_scroll(d2tk_base_t *base, const d2tk_rect_t *rect)
{
#define N 12 // number of columns
#define N_2 (N / 2) // number of visible columns
#define M 512// total elements
#define O 24 // elements per page

	d2tk_style_t style = *d2tk_base_get_default_style();

	const uint32_t hmax [2] = { N, 0 };
	const uint32_t hnum [2] = { N_2, 0 };
	D2TK_BASE_SCROLLBAR(base, rect, D2TK_ID, D2TK_FLAG_SCROLL_X,
		hmax, hnum, hscroll)
	{
		const float hoffset = d2tk_scrollbar_get_offset_x(hscroll);
		const d2tk_rect_t *row = d2tk_scrollbar_get_rect(hscroll);

		D2TK_BASE_TABLE(row, N_2, 1, D2TK_FLAG_TABLE_REL, tcol)
		{
			const unsigned j = d2tk_table_get_index_x(tcol) + hoffset;
			const d2tk_rect_t *col = d2tk_table_get_rect(tcol);

			const uint32_t vmax [2] = { 0, M/(j+1) };
			const uint32_t vnum [2] = { 0, O };
			D2TK_BASE_SCROLLBAR(base, col, D2TK_ID_IDX(j), D2TK_FLAG_SCROLL_Y,
				vmax, vnum, vscroll)
			{
				const float voffset = d2tk_scrollbar_get_offset_y(vscroll);
				const d2tk_rect_t *sub = d2tk_scrollbar_get_rect(vscroll);

				d2tk_base_set_style(base, &style);

				D2TK_BASE_TABLE(sub, 1, O, D2TK_FLAG_TABLE_REL, tlist)
				{
					const unsigned k = d2tk_table_get_index_y(tlist) + voffset;
					const d2tk_rect_t *bnd = d2tk_table_get_rect(tlist);
					const d2tk_id_t id = D2TK_ID_IDX(j*M + k + 1);

					if(k % 2)
					{
						style.fill_color[D2TK_TRIPLE_NONE] = 0x4f4f4fff;
					}
					else
					{
						style.fill_color[D2TK_TRIPLE_NONE] = 0x3f3f3fff;
					}

					char lbl [32];
					const size_t lbl_len = snprintf(lbl, sizeof(lbl), "%u-%03u", j, k);
					if(d2tk_base_button_label_image_is_changed( base, id, lbl_len, lbl,
						D2TK_ALIGN_MIDDLE | D2TK_ALIGN_LEFT,
						-1, "libre-arrow-circle-right.png", bnd))
					{
						fprintf(stdout, "button %016"PRIx64" DOWN\n", id);
					}
				}

				d2tk_base_set_style(base, NULL);
			}
		}
	}
#undef N
#undef N_2
#undef M
#undef O
}

static inline void
_render_c_pane(d2tk_base_t *base, const d2tk_rect_t *rect)
{
	D2TK_BASE_PANE(base, rect, D2TK_ID, D2TK_FLAG_PANE_X, 0.1f, 0.9f, 0.1f, hpane)
	{
		const unsigned x = d2tk_pane_get_index(hpane);
		const d2tk_rect_t *hrect = d2tk_pane_get_rect(hpane);

		// 1st
		if(x == 0)
		{
			if(d2tk_base_button_label_is_changed(base, D2TK_ID, -1, "1st",
				D2TK_ALIGN_CENTERED, hrect))
			{
				fprintf(stdout, "button 1st DOWN\n");
			}

			continue;
		}

		// 2nd
		D2TK_BASE_PANE(base, hrect, D2TK_ID, D2TK_FLAG_PANE_Y, 0.25f, 0.75f, 0.125f,
			vpane)
		{
			const unsigned y = d2tk_pane_get_index(vpane);
			const d2tk_rect_t *vrect = d2tk_pane_get_rect(vpane);

			// 1st
			if(y == 0)
			{
				if(d2tk_base_button_label_is_changed(base, D2TK_ID, -1, "2nd",
					D2TK_ALIGN_CENTERED, vrect))
				{
					fprintf(stdout, "button 2nd DOWN\n");
				}

				continue;
			}

			// 2nd
			if(d2tk_base_button_label_is_changed(base, D2TK_ID, -1, "3rd",
				D2TK_ALIGN_CENTERED, vrect))
			{
				fprintf(stdout, "button 3rd DOWN\n");
			}
		}
	}
}

static inline void
_render_c_layout(d2tk_base_t *base, const d2tk_rect_t *rect)
{
#define N 4
	static const d2tk_coord_t hfrac [N] = { 4, 2, 1, 1 };
	static const d2tk_coord_t vfrac [N] = { 100, 0, 0, 100};

	D2TK_BASE_LAYOUT(rect, N, hfrac, D2TK_FLAG_LAYOUT_X_REL, hlay)
	{
		const d2tk_rect_t *hrect = d2tk_layout_get_rect(hlay);
		const unsigned x = d2tk_layout_get_index(hlay);

		switch(x)
		{
			case 0:
			{
				D2TK_BASE_LAYOUT(hrect, N, vfrac, D2TK_FLAG_LAYOUT_Y_ABS, vlay)
				{
					const d2tk_rect_t *vrect = d2tk_layout_get_rect(vlay);
					const unsigned y = d2tk_layout_get_index(vlay);
					char lbl [16];
					const ssize_t lbl_len = snprintf(lbl, sizeof(lbl), "%"PRIu32, vfrac[y]);

					if(d2tk_base_button_label_is_changed(base, D2TK_ID_IDX(x*N + y),
						lbl_len, lbl, D2TK_ALIGN_CENTERED, vrect))
					{
						fprintf(stdout, "button DOWN\n");
					}
				}
			} break;

			default:
			{
				char lbl [16];
				const ssize_t lbl_len = snprintf(lbl, sizeof(lbl), "%"PRIu32, hfrac[x]);

				if(d2tk_base_button_label_is_changed(base, D2TK_ID_IDX(x*N + 0),
					lbl_len, lbl, D2TK_ALIGN_CENTERED, hrect))
				{
					fprintf(stdout, "button DOWN\n");
				}
			}
		}
	}
#undef N
}

static inline void
_render_c_flowmatrix(d2tk_base_t *base, const d2tk_rect_t *rect)
{
#define N 4
	static d2tk_pos_t pos_nodes [N] = {
		[0] = { .x = -500, .y =  200 },
		[1] = { .x = -250, .y = -100 },
		[2] = { .x =    0, .y =  100 },
		[3] = { .x =  500, .y =    0 }
	};
	static d2tk_pos_t pos_arcs [N][N] = {
		[0] = {
			[3] = { .x = 150, .y = 250 }
		}
	};
	static bool value [N][N][N*N];
	static bool toggle [N];

	D2TK_BASE_FLOWMATRIX(base, rect, D2TK_ID, flowm)
	{
		// draw arcs
		for(unsigned i = 0; i < N; i++)
		{
			const unsigned nin = i + 1;

			for(unsigned j = i + 1; j < N; j++)
			{
				const unsigned nout = j + 1;

				d2tk_state_t state = D2TK_STATE_NONE;
				D2TK_BASE_FLOWMATRIX_ARC(base, flowm, nin, nout, &pos_nodes[i],
					&pos_nodes[j], &pos_arcs[i][j], arc, &state)
				{
					const d2tk_rect_t *bnd = d2tk_flowmatrix_arc_get_rect(arc);
					const unsigned k = d2tk_flowmatrix_arc_get_index(arc);
					const d2tk_id_t id = D2TK_ID_IDX((i*N + j)*N*N + k);
					const unsigned x = d2tk_flowmatrix_arc_get_index_x(arc);
					const unsigned y = d2tk_flowmatrix_arc_get_index_y(arc);

					if(y == nout) // source label
					{
						char lbl [16];
						const ssize_t lbl_len = snprintf(lbl, sizeof(lbl), "Source port %u", x);

						d2tk_base_label(base, lbl_len, lbl, 0.8f, bnd,
							D2TK_ALIGN_BOTTOM | D2TK_ALIGN_RIGHT);
					}
					else if(x == nin) // sink label
					{
						char lbl [16];
						const ssize_t lbl_len = snprintf(lbl, sizeof(lbl), "Sink port %u", y);

						d2tk_base_label(base, lbl_len, lbl, 0.8f, bnd,
							D2TK_ALIGN_BOTTOM | D2TK_ALIGN_LEFT);
					}
					else // connector
					{
						bool *val = &value[i][j][k];

						state = d2tk_base_dial_bool(base, id, bnd, val);
						if(d2tk_state_is_changed(state))
						{
							fprintf(stderr, "Arc %u/%u %s\n", x, y, *val ? "ON" : "OFF");
						}
					}
				}
			}
		}

		// draw nodes
		for(unsigned i = 0; i < N; i++)
		{
			d2tk_state_t state = D2TK_STATE_NONE;
			D2TK_BASE_FLOWMATRIX_NODE(base, flowm, &pos_nodes[i], node, &state)
			{
				char lbl [32];
				const ssize_t lbl_len = snprintf(lbl, sizeof(lbl), "Node %u", i);
				const d2tk_rect_t *bnd = d2tk_flowmatrix_node_get_rect(node);
				const d2tk_id_t id = D2TK_ID_IDX(i);
				bool *val = &toggle[i];

				state = d2tk_base_toggle_label(base, id, lbl_len, lbl,
					D2TK_ALIGN_CENTERED, bnd, val);
				if(d2tk_state_is_active(state))
				{
					d2tk_flowmatrix_set_src(flowm, id, &pos_nodes[i]);
				}
				if(d2tk_state_is_over(state))
				{
					d2tk_flowmatrix_set_dst(flowm, id, &pos_nodes[i]);
				}
				if(d2tk_state_is_up(state))
				{
					const d2tk_id_t dst_id = d2tk_flowmatrix_get_dst(flowm, NULL);

					if(dst_id)
					{
						fprintf(stderr, "Connecting nodes %016"PRIx64" -> %016"PRIx64"\n",
							id, dst_id);
					}
				}
				state = D2TK_STATE_NONE;
				//else if(d2tk_state_is_changed(state))
				//{
				//	fprintf(stderr, "Node %u %s\n", i, *val ? "ON" : "OFF");
				//}
			}
		}
	}
#undef N
}

static inline void
_render_c_meter(d2tk_base_t *base, const d2tk_rect_t *rect)
{
#define N 4
#define M 40
	d2tk_coord_t x = 0;
	d2tk_coord_t y = 0;
	d2tk_coord_t w = 0;
	d2tk_coord_t h = 0;

	d2tk_base_get_mouse_pos(base, &x, &y);
	d2tk_base_get_dimensions(base, &w, &h);

	const float fx = (float)x / w;
	const float fy = (float)y / h;

	D2TK_BASE_TABLE(rect, N, M, D2TK_FLAG_TABLE_REL, tab)
	{
		const unsigned k = d2tk_table_get_index(tab);
		const unsigned a = d2tk_table_get_index_x(tab);
		const unsigned b = d2tk_table_get_index_y(tab);
		const d2tk_rect_t *bnd = d2tk_table_get_rect(tab);
		const d2tk_id_t id = D2TK_ID_IDX(k);

		const float fa = (float)a / N;
		const float fb = (float)b / M;

		const float dx = fa - fx;
		const float dy = fb - fy;

		const float dz = 1.f - sqrtf(dx*dx + dy*dy)*M_SQRT1_2;

		int32_t val = -54 + 60*dz;

		if(d2tk_base_meter_is_changed(base, id, bnd, &val))
		{
			// nothing to do, yet
		}
	}
#undef M
#undef N
}

static inline void
_render_c_frame(d2tk_base_t *base, const d2tk_rect_t *rect)
{
#define N 4
	static bool val [N];

	D2TK_BASE_TABLE(rect, N, N, D2TK_FLAG_TABLE_REL, tab)
	{
		const d2tk_rect_t *bnd_outer = d2tk_table_get_rect(tab);
		const unsigned k = d2tk_table_get_index(tab);

		char lbl [32];
		const ssize_t lbl_len = (k % 2)
			? snprintf(lbl, sizeof(lbl), "This is frame #%u", k)
			: 0;

		D2TK_BASE_FRAME(base, bnd_outer, lbl_len, lbl, frm)
		{
			const d2tk_rect_t *bnd_inner = d2tk_frame_get_rect(frm);
			const d2tk_id_t id = D2TK_ID_IDX(k);

			if(d2tk_base_dial_bool_is_changed(base, id, bnd_inner, &val[k]))
			{
				fprintf(stdout, "dial %016"PRIx64" %s\n", id, val[k] ? "ON" : "OFF");
			}
		}
	}
#undef N
}

static inline void
_render_c_utf8(d2tk_base_t *base, const d2tk_rect_t *rect)
{
#define N 3
	static const char *lbls [N] = {
		"I can eat glass and it doesn't hurt me", // English
		"Я могу есть стекло, оно мне не вредит", // Russian
		"Μπορῶ νὰ φάω σπασμένα γυαλιὰ χωρὶς νὰ πάθω τίποτα", // Greek
	};

	D2TK_BASE_TABLE(rect, 1, 20, D2TK_FLAG_TABLE_REL, tab)
	{
		const d2tk_rect_t *trect = d2tk_table_get_rect(tab);
		const unsigned k = d2tk_table_get_index(tab);

		if(k >= N)
		{
			break;
		}

		d2tk_base_label(base, -1, lbls[k], 0.5f, trect,
			D2TK_ALIGN_MIDDLE | D2TK_ALIGN_LEFT);
	}
#undef N
}

#if D2TK_PTY
#include <linenoise/linenoise.h>

static void
_completion(const char *buf, linenoiseCompletions *lc)
{
	switch(buf[0])
	{
		case 'b':
		{
			linenoiseAddCompletion(lc, "bar");
		} break;
		case 'c':
		{
			linenoiseAddCompletion(lc, "clear");
		} break;
		case 'f':
		{
			linenoiseAddCompletion(lc, "foo");
		} break;
	}
}

static char *
_hints(const char *buf, int *color, int *bold)
{
	static const char *hint = "<cmd>";

	switch(buf[0])
	{
		case '\0':
		{
			*color = 36;
			*bold = 0;
			return (char *)hint;
		} break;
	}

	return NULL;
}

static void
_curses(FILE *err __attribute__((unused)), void *data __attribute__((unused)))
{
	linenoiseSetMultiLine(0);
	linenoiseSetCompletionCallback(_completion);
	linenoiseSetHintsCallback(_hints);
	linenoiseHistorySetMaxLen(128);

	while(true)
	{
    char *line = linenoise("> ");

		if(!line)
		{
			break;
		}

		if(strlen(line))
		{
			//FIXME
			linenoiseHistoryAdd(line);
		}

		if(!strcmp(line, "clear"))
		{
			linenoiseClearScreen();
		}

		linenoiseFree(line);
	}

	_exit(EXIT_SUCCESS);
}

static inline void
_render_c_pty(d2tk_base_t *base, const d2tk_rect_t *rect)
{
#define HEIGHT 16
	static char *argv [] = {
		"bash",
		NULL
	};

	static d2tk_coord_t hfrac [2] = { 1, 1 };

	D2TK_BASE_LAYOUT(rect, 2, hfrac, D2TK_FLAG_LAYOUT_X_REL, hlay)
	{
		const d2tk_rect_t *hrect = d2tk_layout_get_rect(hlay);
		const d2tk_coord_t x = d2tk_layout_get_index(hlay);

		switch(x)
		{
			case 0:
			{
				d2tk_base_pty(base, D2TK_ID, NULL, argv, HEIGHT, hrect, false);
			} break;
			case 1:
			{
				d2tk_base_pty(base, D2TK_ID, _curses, NULL, HEIGHT, hrect, false);
			} break;
		}
	}

#undef HEIGHT
}
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
static int
strcasenumcmp(const char *s1, const char *s2)
{
	static const char *digits = "1234567890";
	const char *d1 = strpbrk(s1, digits);
	const char *d2 = strpbrk(s2, digits);

	// do both s1 and s2 contain digits?
	if(d1 && d2)
	{
		const size_t l1 = d1 - s1;
		const size_t l2 = d2 - s2;

		// do both s1 and s2 match up to the first digit?
		if( (l1 == l2) && (strncmp(s1, s2, l1) == 0) )
		{
			char *e1 = NULL;
			char *e2 = NULL;

			const int n1 = strtol(d1, &e1, 10);
			const int n2 = strtol(d2, &e2, 10);

			// do both d1 and d2 contain a valid number?
			if(e1 && e2)
			{
				// are the numbers equal? do the same for the substring
				if(n1 == n2)
				{
					return strcasenumcmp(e1, e2);
				}

				// the numbers differ, e.g. return their ordering
				return (n1 < n2) ? -1 : 1;
			}
		}
	}

	// no digits in either s1 or s2, do normal comparison
	return strcasecmp(s1, s2);
}

static int
_file_list_sort(const void *a, const void *b)
{
	const struct dirent *A = (const struct dirent *)a;
	const struct dirent *B = (const struct dirent *)b;

	const bool a_is_dir = (A->d_type == DT_DIR);
	const bool b_is_dir = (B->d_type == DT_DIR);

	if(a_is_dir && !b_is_dir)
	{
		return -1;
	}
	else if(!a_is_dir && b_is_dir)
	{
		return 1;
	}

	return strcasenumcmp(A->d_name, B->d_name);
}

static struct dirent *
_file_list_new(const char *path, size_t *num, bool return_hidden)
{
	struct dirent *list = NULL;
	*num = 0;

	DIR *dir = opendir(path);
	if(dir)
	{
		struct dirent *itm;

		while( (itm = readdir(dir)) )
		{
			if(itm->d_name[0] == '.')
			{
				if(  !return_hidden
					|| ( (itm->d_name[1] == '\0') || (itm->d_name[1] == '.') ) )
				{
					continue;
				}
			}

			list = realloc(list, (*num+1) * sizeof(struct dirent));
			memcpy(&list[*num], itm, sizeof(struct dirent));
			*num += 1;
		}

		closedir(dir);
	}

	if(list)
	{
		qsort(list, *num, sizeof(struct dirent), _file_list_sort);
		return list;
	}

	return NULL;
}

static void
_file_list_free(struct dirent *list)
{
	free(list);
}

static inline void
_render_c_browser(d2tk_base_t *base, const d2tk_rect_t *rect)
{
	static char root [PATH_MAX] = "";

	if(strlen(root) == 0)
	{
		snprintf(root, sizeof(root), "%s/", getenv("HOME"));
	}

#define M 30
	size_t nlist;
	struct dirent *list = _file_list_new(root, &nlist, false);
	d2tk_style_t style = *d2tk_base_get_default_style();

	D2TK_BASE_TABLE(rect, 2, 1, D2TK_FLAG_TABLE_REL, brow)
	{
		const unsigned r = d2tk_table_get_index(brow);
		const d2tk_rect_t *col = d2tk_table_get_rect(brow);

		switch(r)
		{
			case 0:
			{
				size_t ndir = 0;
				const char *ptr = root;

				for(const char *nxt = strchr(root, '/');
					nxt;
					nxt = strchr(nxt + 1, '/'))
				{
					ndir++;
				}

				const uint32_t max [2] = { 0, ndir };
				const uint32_t num [2] = { 0, M };
				D2TK_BASE_SCROLLBAR(base, col, D2TK_ID, D2TK_FLAG_SCROLL_Y,
					max, num, vscroll)
				{
					const float voffset = d2tk_scrollbar_get_offset_y(vscroll);
					const d2tk_rect_t *row = d2tk_scrollbar_get_rect(vscroll);

					d2tk_base_set_style(base, &style);

					D2TK_BASE_TABLE(row, 1, M, D2TK_FLAG_TABLE_REL, tab)
					{
						const unsigned k = d2tk_table_get_index(tab) + voffset;
						const d2tk_rect_t *bnd = d2tk_table_get_rect(tab);

						if(k >= ndir)
						{
							break;
						}

						char *nxt = strchr(ptr, '/');
						if(!nxt)
						{
							break;
						}

						nxt++;

						style.fill_color[D2TK_TRIPLE_NONE] = k % 2
							? 0x4f4f4fff
							: 0x3f3f3fff;

						const char *icon = "libre-gui-folder.png";

						if(d2tk_base_button_label_image_is_changed(base,
							D2TK_ID_IDX(k), nxt - ptr, ptr,
							D2TK_ALIGN_MIDDLE | D2TK_ALIGN_LEFT, -1, icon, bnd))
						{
							*nxt = '\0';
						}

						ptr = nxt;
					}

					d2tk_base_set_style(base, NULL);
				}
			} break;

			case 1:
			{
				const uint32_t max [2] = { 0, nlist };
				const uint32_t num [2] = { 0, M };
				D2TK_BASE_SCROLLBAR(base, col, D2TK_ID, D2TK_FLAG_SCROLL_Y,
					max, num, vscroll)
				{
					const float voffset = d2tk_scrollbar_get_offset_y(vscroll);
					const d2tk_rect_t *row = d2tk_scrollbar_get_rect(vscroll);

					d2tk_base_set_style(base, &style);

					D2TK_BASE_TABLE(row, 1, M, D2TK_FLAG_TABLE_REL, tab)
					{
						const unsigned k = d2tk_table_get_index(tab) + voffset;
						const d2tk_rect_t *bnd = d2tk_table_get_rect(tab);

						if(k >= nlist)
						{
							break;
						}

						style.fill_color[D2TK_TRIPLE_NONE] = k % 2
							? 0x4f4f4fff
							: 0x3f3f3fff;

						struct dirent *itm = &list[k];
						const bool is_dir = (itm->d_type == DT_DIR);

						const char *icon = is_dir
							? "libre-gui-folder.png"
							: "libre-gui-file.png";

						if(d2tk_base_button_label_image_is_changed(base,
							D2TK_ID_IDX(k), -1, itm->d_name,
							D2TK_ALIGN_MIDDLE | D2TK_ALIGN_LEFT, -1, icon, bnd))
						{
							if(is_dir)
							{
								strncat(root, itm->d_name, sizeof(root) - strlen(root) - 1);
								strncat(root, "/", sizeof(root) - strlen(root) - 1);

								d2tk_base_clear_focus(base);
							}
						}
					}

					d2tk_base_set_style(base, NULL);
				}
			} break;
		}
	}

	_file_list_free(list);
#undef M
}
#endif

#if D2TK_EVDEV
static void
_fake_event(unsigned type, unsigned code, int value)
{
	if(fake.uidev)
	{
		libevdev_uinput_write_event(fake.uidev, type, code, value);
	}
}

static void
_fake_key_down(unsigned keycode)
{
	_fake_event(EV_KEY, keycode, 1);
	_fake_event(EV_SYN, SYN_REPORT, 0);
}

static void
_fake_key_up(unsigned keycode)
{
	_fake_event(EV_KEY, keycode, 0);
	_fake_event(EV_SYN, SYN_REPORT, 0);
}

typedef struct _keybtn_t keybtn_t;

struct _keybtn_t {
	const char *name;
	const char *altn;
	unsigned code;
	float rect [4];
};

#define W (1.f / 15.f)
#define W_2 (W / 2)
#define H (1.f / 6.f)

static const keybtn_t keybtns [] = {
	// row 1
	{
		.name = "Esc",
		.code = KEY_ESC,
		.rect = { 0*W, 0*H, W + W_2, H }
	},
	{
		.name = "F1",
		.code = KEY_F1,
		.rect = { 1*W + W_2, 0*H, W, H }
	},
	{
		.name = "F2",
		.code = KEY_F2,
		.rect = { 2*W + W_2, 0*H, W, H }
	},
	{
		.name = "F3",
		.code = KEY_F3,
		.rect = { 3*W + W_2, 0*H, W, H }
	},
	{
		.name = "F4",
		.code = KEY_F4,
		.rect = { 4*W + W_2, 0*H, W, H }
	},
	{
		.name = "F5",
		.code = KEY_F5,
		.rect = { 5*W + W_2, 0*H, W, H }
	},
	{
		.name = "F6",
		.code = KEY_F6,
		.rect = { 6*W + W_2, 0*H, W, H }
	},
	{
		.name = "F7",
		.code = KEY_F7,
		.rect = { 7*W + W_2, 0*H, W, H }
	},
	{
		.name = "F8",
		.code = KEY_F8,
		.rect = { 8*W + W_2, 0*H, W, H }
	},
	{
		.name = "F9",
		.code = KEY_F9,
		.rect = { 9*W + W_2, 0*H, W, H }
	},
	{
		.name = "F10",
		.code = KEY_F10,
		.rect = { 10*W + W_2, 0*H, W, H }
	},
	{
		.name = "F11",
		.code = KEY_F11,
		.rect = { 11*W + W_2, 0*H, W, H }
	},
	{
		.name = "F12",
		.code = KEY_F12,
		.rect = { 12*W + W_2, 0*H, W, H }
	},
	{
		.name = "Delete",
		.code = KEY_DELETE,
		.rect = { 13*W + W_2, 0*H, W + W_2, H }
	},

	// row 2
	{
		.name = "`",
		.altn = "~",
		.code = KEY_GRAVE,
		.rect = { 0*W, 1*H, W, H }
	},
	{
		.name = "1",
		.altn = "!",
		.code = KEY_1,
		.rect = { 1*W, 1*H, W, H }
	},
	{
		.name = "2",
		.altn = "@",
		.code = KEY_2,
		.rect = { 2*W, 1*H, W, H }
	},
	{
		.name = "3",
		.altn = "#",
		.code = KEY_3,
		.rect = { 3*W, 1*H, W, H }
	},
	{
		.name = "4",
		.altn = "$",
		.code = KEY_4,
		.rect = { 4*W, 1*H, W, H }
	},
	{
		.name = "5",
		.altn = "%",
		.code = KEY_5,
		.rect = { 5*W, 1*H, W, H }
	},
	{
		.name = "6",
		.altn = "^",
		.code = KEY_6,
		.rect = { 6*W, 1*H, W, H }
	},
	{
		.name = "7",
		.altn = "&",
		.code = KEY_7,
		.rect = { 7*W, 1*H, W, H }
	},
	{
		.name = "8",
		.altn = "*",
		.code = KEY_8,
		.rect = { 8*W, 1*H, W, H }
	},
	{
		.name = "9",
		.altn = "(",
		.code = KEY_9,
		.rect = { 9*W, 1*H, W, H }
	},
	{
		.name = "0",
		.altn = ")",
		.code = KEY_0,
		.rect = { 10*W, 1*H, W, H }
	},
	{
		.name = "-",
		.altn = "_",
		.code = KEY_MINUS,
		.rect = { 11*W, 1*H, W, H }
	},
	{
		.name = "=",
		.altn = "+",
		.code = KEY_EQUAL,
		.rect = { 12*W, 1*H, W, H }
	},
	{
		.name = "Back",
		.code = KEY_BACKSPACE,
		.rect = { 13*W, 1*H, 2*W, H }
	},

	// row 3
	{
		.name = "Tab",
		.code = KEY_TAB,
		.rect = { 0*W, 2*H, W + W_2, H }
	},
	{
		.name = "Q",
		.code = KEY_Q,
		.rect = { 1*W + W_2, 2*H, W, H }
	},
	{
		.name = "W",
		.code = KEY_W,
		.rect = { 2*W + W_2, 2*H, W, H }
	},
	{
		.name = "E",
		.code = KEY_E,
		.rect = { 3*W + W_2, 2*H, W, H }
	},
	{
		.name = "R",
		.code = KEY_R,
		.rect = { 4*W + W_2, 2*H, W, H }
	},
	{
		.name = "T",
		.code = KEY_T,
		.rect = { 5*W + W_2, 2*H, W, H }
	},
	{
		.name = "Y",
		.code = KEY_Y,
		.rect = { 6*W + W_2, 2*H, W, H }
	},
	{
		.name = "U",
		.code = KEY_U,
		.rect = { 7*W + W_2, 2*H, W, H }
	},
	{
		.name = "I",
		.code = KEY_I,
		.rect = { 8*W + W_2, 2*H, W, H }
	},
	{
		.name = "O",
		.code = KEY_O,
		.rect = { 9*W + W_2, 2*H, W, H }
	},
	{
		.name = "P",
		.code = KEY_P,
		.rect = { 10*W + W_2, 2*H, W, H }
	},
	{
		.name = "[",
		.altn = "{",
		.code = KEY_LEFTBRACE,
		.rect = { 11*W + W_2, 2*H, W, H }
	},
	{
		.name = "]",
		.altn = "}",
		.code = KEY_RIGHTBRACE,
		.rect = { 12*W + W_2, 2*H, W, H }
	},
	{
		.name = "\\",
		.altn = "|",
		.code = KEY_BACKSLASH,
		.rect = { 13*W + W_2, 2*H, 2*W - W_2, H }
	},

	// row 4
	{
		.name = "Caps",
		.code = KEY_CAPSLOCK,
		.rect = { 0*W, 3*H, W*2, H }
	},
	{
		.name = "A",
		.code = KEY_A,
		.rect = { 2*W, 3*H, W, H }
	},
	{
		.name = "S",
		.code = KEY_S,
		.rect = { 3*W, 3*H, W, H }
	},
	{
		.name = "D",
		.code = KEY_D,
		.rect = { 4*W, 3*H, W, H }
	},
	{
		.name = "F",
		.code = KEY_F,
		.rect = { 5*W, 3*H, W, H }
	},
	{
		.name = "G",
		.code = KEY_G,
		.rect = { 6*W, 3*H, W, H }
	},
	{
		.name = "H",
		.code = KEY_H,
		.rect = { 7*W, 3*H, W, H }
	},
	{
		.name = "J",
		.code = KEY_J,
		.rect = { 8*W, 3*H, W, H }
	},
	{
		.name = "K",
		.code = KEY_K,
		.rect = { 9*W, 3*H, W, H }
	},
	{
		.name = "L",
		.code = KEY_L,
		.rect = { 10*W, 3*H, W, H }
	},
	{
		.name = ";",
		.altn = ":",
		.code = KEY_SEMICOLON,
		.rect = { 11*W, 3*H, W, H }
	},
	{
		.name = "'",
		.altn = "\"",
		.code = KEY_APOSTROPHE,
		.rect = { 12*W, 3*H, W, H }
	},
	{
		.name = "Enter",
		.code = KEY_ENTER,
		.rect = { 13*W, 3*H, W*2, H }
	},

	// row 5
	{
		.name = "Shift",
		.code = KEY_LEFTSHIFT,
		.rect = { 0*W, 4*H, W*2 + W_2, H }
	},
	{
		.name = "Z",
		.code = KEY_Z,
		.rect = { 2*W + W_2, 4*H, W, H }
	},
	{
		.name = "X",
		.code = KEY_X,
		.rect = { 3*W + W_2, 4*H, W, H }
	},
	{
		.name = "C",
		.code = KEY_C,
		.rect = { 4*W + W_2, 4*H, W, H }
	},
	{
		.name = "V",
		.code = KEY_V,
		.rect = { 5*W + W_2, 4*H, W, H }
	},
	{
		.name = "B",
		.code = KEY_B,
		.rect = { 6*W + W_2, 4*H, W, H }
	},
	{
		.name = "N",
		.code = KEY_N,
		.rect = { 7*W + W_2, 4*H, W, H }
	},
	{
		.name = "M",
		.code = KEY_M,
		.rect = { 8*W + W_2, 4*H, W, H }
	},
	{
		.name = ",",
		.altn = "<",
		.code = KEY_COMMA,
		.rect = { 9*W + W_2, 4*H, W, H }
	},
	{
		.name = ".",
		.altn = ">",
		.code = KEY_DOT,
		.rect = { 10*W + W_2, 4*H, W, H }
	},
	{
		.name = "/",
		.altn = "?",
		.code = KEY_SLASH,
		.rect = { 11*W + W_2, 4*H, W, H }
	},
	{
		.name = "Shift",
		.code = KEY_RIGHTSHIFT,
		.rect = { 12*W + W_2, 4*H, 3*W - W_2, H }
	},

	// row 6
	{
		.name = "Fn",
		.code = KEY_FN,
		.rect = { 0*W, 5*H, W, H }
	},
	{
		.name = "Ctrl",
		.code = KEY_LEFTCTRL,
		.rect = { 1*W, 5*H, W, H }
	},
	{
		.name = "Mta",
		.code = KEY_LEFTMETA,
		.rect = { 2*W, 5*H, W, H }
	},
	{
		.name = "Alt",
		.code = KEY_LEFTALT,
		.rect = { 3*W, 5*H, W, H }
	},
	{
		.name = "Space",
		.code = KEY_SPACE,
		.rect = { 4*W, 5*H, 5*W, H }
	},
	{
		.name = "Alt",
		.code = KEY_RIGHTALT,
		.rect = { 9*W, 5*H, W, H }
	},
	{
		.name = "Mta",
		.code = KEY_RIGHTMETA,
		.rect = { 10*W, 5*H, W, H }
	},
	{
		.name = "Ctrl",
		.code = KEY_RIGHTCTRL,
		.rect = { 11*W, 5*H, W, H }
	},
	{
		.name = "Hom",
		.code = KEY_HOME,
		.rect = { 12*W, 5*H, W, H }
	},
	{
		.name = "End",
		.code = KEY_END,
		.rect = { 13*W, 5*H, W, H }
	},
	{
		.name = "Ins",
		.code = KEY_INSERT,
		.rect = { 14*W, 5*H, W, H }
	},

	{ // sentinel
		.name = NULL
	}
};

static inline void
_render_c_keyboard(d2tk_base_t *base, const d2tk_rect_t *rect)
{
	for(const keybtn_t *keybtn = keybtns; keybtn->name; keybtn++)
	{
		const d2tk_rect_t bnd = {
			.x = rect->x + keybtn->rect[0]*rect->w,
			.y = rect->y + keybtn->rect[1]*rect->h,
			.w = keybtn->rect[2]*rect->w,
			.h = keybtn->rect[3]*rect->h
		};

		const char *lbl = d2tk_base_get_modmask(base, D2TK_MODMASK_SHIFT, false)
				&& keybtn->altn
			? keybtn->altn
			: keybtn->name;

		const d2tk_state_t state = d2tk_base_button_label(base,
			D2TK_ID_IDX(keybtn-keybtns), -1, lbl, D2TK_ALIGN_CENTERED, &bnd);

		if(d2tk_state_is_down(state))
		{
			_fake_key_down(keybtn->code);
		}
		else if(d2tk_state_is_up(state))
		{
			_fake_key_up(keybtn->code);
		}
	}
}
#endif

D2TK_API int
d2tk_example_init(void)
{
#if D2TK_EVDEV
	fake.dev = libevdev_new();
	if(!fake.dev)
	{
		fprintf(stderr, "Error: libevdev_new\n");
		return EXIT_FAILURE;;
	}
	libevdev_set_name(fake.dev, "Fake keyboard");
	libevdev_enable_event_type(fake.dev, EV_SYN);
	libevdev_enable_event_code(fake.dev, EV_SYN, SYN_REPORT, NULL);
	libevdev_enable_event_type(fake.dev, EV_KEY);
	for(const keybtn_t *keybtn = keybtns; keybtn->name; keybtn++)
	{
		libevdev_enable_event_code(fake.dev, EV_KEY, keybtn->code, NULL);
	}

	fake.uidev = NULL;
	libevdev_uinput_create_from_device(fake.dev, LIBEVDEV_UINPUT_OPEN_MANAGED,
		&fake.uidev);
	if(!fake.uidev)
	{
		fprintf(stderr, "Warning: libevdev_uinput_create_from_device\n");
		return EXIT_FAILURE;
	}
#endif
	
	return EXIT_SUCCESS;
}

D2TK_API void
d2tk_example_deinit(void)
{
#if D2TK_EVDEV
	if(fake.uidev)
	{
		libevdev_uinput_destroy(fake.uidev);
	}
	if(fake.dev)
	{
		libevdev_free(fake.dev);
	}
#endif
}

D2TK_API void
d2tk_example_run(d2tk_base_t *base, d2tk_coord_t w, d2tk_coord_t h)
{
	static d2tk_coord_t vfrac [2] = { 1, 19 };

	d2tk_base_set_ttls(base, 0x10, 0x100);

	D2TK_BASE_LAYOUT(&D2TK_RECT(0, 0, w, h), 2, vfrac, D2TK_FLAG_LAYOUT_Y_REL, vlay)
	{
		const d2tk_rect_t *vrect = d2tk_layout_get_rect(vlay);
		const unsigned y = d2tk_layout_get_index(vlay);

		switch(y)
		{
			case 0:
			{
				D2TK_BASE_TABLE(vrect, BAR_MAX, 1, D2TK_FLAG_TABLE_REL, tab)
				{
					const d2tk_rect_t *hrect = d2tk_table_get_rect(tab);
					const unsigned b = d2tk_table_get_index(tab);

					bool val = (b == bar);
					d2tk_base_toggle_label(base, D2TK_ID_IDX(b), -1, bar_lbl[b],
						D2TK_ALIGN_CENTERED, hrect, &val);
					if(val)
					{
						bar = b;
					}
				}
			} break;
			case 1:
			{
				switch(bar)
				{
					case BAR_MIX:
					{
						_render_c_mix(base, vrect);
					} break;
					case BAR_SEQ:
					{
						_render_c_seq(base, vrect);
					} break;
					case BAR_TABLE_ABS:
					{
						_render_c_table_abs(base, vrect);
					} break;
					case BAR_SCROLL:
					{
						_render_c_scroll(base, vrect);
					} break;
					case BAR_PANE:
					{
						_render_c_pane(base, vrect);
					} break;
					case BAR_LAYOUT:
					{
						_render_c_layout(base, vrect);
					} break;
					case BAR_FLOWMATRIX:
					{
						_render_c_flowmatrix(base, vrect);
					} break;
					case BAR_METER:
					{
						_render_c_meter(base, vrect);
					} break;
					case BAR_FRAME:
					{
						_render_c_frame(base, vrect);
					} break;
					case BAR_UTF8:
					{
						_render_c_utf8(base, vrect);
					} break;
#if D2TK_PTY
					case BAR_PTY:
					{
						_render_c_pty(base, vrect);
					} break;
#endif
#if !defined(_WIN32) && !defined(__APPLE__)
					case BAR_BROWSER:
					{
						_render_c_browser(base, vrect);
					} break;
#endif
#if D2TK_EVDEV
					case BAR_KEYBOARD:
					{
						_render_c_keyboard(base, vrect);
					} break;
#endif

					case BAR_MAX:
						// fall-through
					default:
					{
						// do nothing
					} break;
				}
			}
		}
	}
}
