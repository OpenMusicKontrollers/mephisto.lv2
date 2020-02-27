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

#include "base_internal.h"

D2TK_API d2tk_state_t
d2tk_base_spinner_int32(d2tk_base_t *base, d2tk_id_t id, const d2tk_rect_t *rect,
	int32_t min, int32_t *value, int32_t max)
{
	d2tk_state_t state = D2TK_STATE_NONE;
	const d2tk_coord_t h2 = rect->h/2;
	const d2tk_coord_t h8 = rect->h/8;
	const d2tk_coord_t fract [3] = { h2, 0, h2 };
	D2TK_BASE_LAYOUT(rect, 3, fract, D2TK_FLAG_LAYOUT_X_ABS, lay)
	{
		const unsigned k = d2tk_layout_get_index(lay);
		const d2tk_rect_t *lrect= d2tk_layout_get_rect(lay);
		const d2tk_id_t itrid = D2TK_ID_IDX(k);
		const d2tk_id_t subid = (itrid << 32) | id;

		switch(k)
		{
			case 0:
			{
				d2tk_rect_t bnd;
				d2tk_rect_shrink_y(&bnd, lrect, h8);

				static const char arrow [] = "◀";
				const d2tk_state_t substate = d2tk_base_button_label(base, subid,
					sizeof(arrow), arrow, D2TK_ALIGN_CENTERED, &bnd);

				if(d2tk_state_is_changed(substate))
				{
					const int32_t old_value = *value;

					*value -= 1;
					d2tk_clip_int32(min, value, max);

					if(old_value != *value)
					{
						state |= D2TK_STATE_CHANGED;
					}
				}
			} break;
			case 1:
			{
				const d2tk_state_t substate = d2tk_base_bar_int32(base, subid, lrect,
					min, value, max);

				state |= substate;

				d2tk_rect_t bnd;
				d2tk_rect_shrink(&bnd, lrect, 5); //FIXME relative to style->border

				char lbl [16];
				const ssize_t lbl_len = snprintf(lbl, sizeof(lbl), "%+"PRIi32, *value);
				d2tk_base_label(base, lbl_len, lbl, 0.75f, &bnd,
					D2TK_ALIGN_MIDDLE | D2TK_ALIGN_RIGHT);
			} break;
			case 2:
			{
				d2tk_rect_t bnd;
				d2tk_rect_shrink_y(&bnd, lrect, h8);

				static const char arrow [] = "▶";
				const d2tk_state_t substate = d2tk_base_button_label(base, subid,
					sizeof(arrow), arrow, D2TK_ALIGN_CENTERED, &bnd);

				if(d2tk_state_is_changed(substate))
				{
					const int32_t old_value = *value;

					*value += 1;
					d2tk_clip_int32(min, value, max);

					if(old_value != *value)
					{
						state |= D2TK_STATE_CHANGED;
					}
				}
			} break;
		}
	}

	return state;
}
