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

#include <inttypes.h>
#include <math.h>
#include <stdio.h>

#include "base_internal.h"

static inline void
_d2tk_base_draw_wave(d2tk_core_t *core, const d2tk_rect_t *rect,
	d2tk_state_t state, const d2tk_style_t *style, float min,
	const float *value, int32_t nelem, float max)
{
	const d2tk_hash_dict_t dict [] = {
		{ rect, sizeof(d2tk_rect_t) },
		{ &state , sizeof(d2tk_state_t) },
		{ style, sizeof(d2tk_style_t) },
		{ &min, sizeof(float) },
		{ value, sizeof(float)*nelem },
		{ &max, sizeof(float) },
		{ NULL, 0 }
	};
	const uint64_t hash = d2tk_hash_dict(dict);

	D2TK_CORE_WIDGET(core, hash, widget)
	{
		const size_t ref = d2tk_core_bbox_push(core, true, rect);
		const float range_1 = 1.f / (max - min);

		d2tk_core_begin_path(core);

		if(nelem > rect->w)
		{
			for(int32_t i = 0; i < rect->w; i++)
			{
				const int32_t off = i * nelem / (rect->w - 1);
				const float rel = 1.f - (value[off] - min)*range_1;

				const int32_t x0 = rect->x + i;
				const int32_t y0 = rect->y + rel*rect->h;

				if(i == 0)
				{
					d2tk_core_move_to(core, x0, y0);
				}
				else
				{
					d2tk_core_line_to(core, x0, y0);
				}
			}
		}
		else
		{
			for(int32_t off = 0; off < nelem; off++)
			{
				const int32_t i = off * rect->w / (nelem - 1);
				const float rel = 1.f - (value[off] - min) * range_1;

				const int32_t x0 = rect->x + i;
				const int32_t y0 = rect->y + rel*rect->h;

				if(i == 0)
				{
					d2tk_core_move_to(core, x0, y0);
				}
				else
				{
					d2tk_core_line_to(core, x0, y0);
				}
			}
		}
		d2tk_core_color(core, style->stroke_color[D2TK_TRIPLE_ACTIVE_HOT_FOCUS]);
		d2tk_core_stroke_width(core, style->border_width);
		d2tk_core_stroke(core);

		d2tk_core_bbox_pop(core, ref);
	}
}

D2TK_API d2tk_state_t
d2tk_base_wave_float(d2tk_base_t *base, d2tk_id_t id, const d2tk_rect_t *rect,
	float min, const float *value, int32_t nelem, float max)
{
	d2tk_state_t state = d2tk_base_is_active_hot(base, id, rect,
		D2TK_FLAG_SCROLL);

	_d2tk_base_draw_wave(base->core, rect, state, d2tk_base_get_style(base),
		min, value, nelem, max);

	return state;
}
