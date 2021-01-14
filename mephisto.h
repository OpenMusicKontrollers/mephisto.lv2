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

#ifndef _MEPHISTO_LV2_H
#define _MEPHISTO_LV2_H

#include <stdint.h>
#if !defined(_WIN32)
#	include <sys/mman.h>
#else
#	define mlock(...)
#	define munlock(...)
#endif

#include <lv2/lv2plug.in/ns/ext/atom/atom.h>
#include <lv2/lv2plug.in/ns/ext/atom/forge.h>
#include <lv2/lv2plug.in/ns/ext/urid/urid.h>
#include <lv2/lv2plug.in/ns/ext/time/time.h>
#include <lv2/lv2plug.in/ns/ext/midi/midi.h>
#include <lv2/lv2plug.in/ns/ext/state/state.h>
#include <lv2/lv2plug.in/ns/ext/worker/worker.h>
#include <lv2/lv2plug.in/ns/ext/log/log.h>
#include <lv2/lv2plug.in/ns/ext/log/logger.h>
#include <lv2/lv2plug.in/ns/ext/patch/patch.h>
#include <lv2/lv2plug.in/ns/ext/options/options.h>
#include <lv2/lv2plug.in/ns/ext/buf-size/buf-size.h>
#include <lv2/lv2plug.in/ns/ext/parameters/parameters.h>
#include <lv2/lv2plug.in/ns/extensions/ui/ui.h>
#include <lv2/lv2plug.in/ns/lv2core/lv2.h>

#define MEPHISTO_URI    "http://open-music-kontrollers.ch/lv2/mephisto"
#define MEPHISTO_PREFIX MEPHISTO_URI "#"

// plugin uris
#define MEPHISTO__audio_1x1     MEPHISTO_PREFIX "audio_1x1"
#define MEPHISTO__audio_2x2     MEPHISTO_PREFIX "audio_2x2"
#define MEPHISTO__audio_4x4     MEPHISTO_PREFIX "audio_4x4"
#define MEPHISTO__audio_8x8     MEPHISTO_PREFIX "audio_8x8"
#define MEPHISTO__cv_1x1        MEPHISTO_PREFIX "cv_1x1"
#define MEPHISTO__cv_2x2        MEPHISTO_PREFIX "cv_2x2"
#define MEPHISTO__cv_4x4        MEPHISTO_PREFIX "cv_4x4"
#define MEPHISTO__cv_8x8        MEPHISTO_PREFIX "cv_8x8"

// plugin UI uris
#define MEPHISTO__ui            MEPHISTO_PREFIX "ui"

// param uris
#define MEPHISTO__code          MEPHISTO_PREFIX "code"
#define MEPHISTO__error         MEPHISTO_PREFIX "error"
#define MEPHISTO__xfadeDuration MEPHISTO_PREFIX "xfadeDuration"
#define MEPHISTO__fontHeight    MEPHISTO_PREFIX "fontHeight"

#define MEPHISTO__timestamp     MEPHISTO_PREFIX "timestamp"

#define MEPHISTO__control_1     MEPHISTO_PREFIX "control_1"
#define MEPHISTO__control_2     MEPHISTO_PREFIX "control_2"
#define MEPHISTO__control_3     MEPHISTO_PREFIX "control_3"
#define MEPHISTO__control_4     MEPHISTO_PREFIX "control_4"
#define MEPHISTO__control_5     MEPHISTO_PREFIX "control_5"
#define MEPHISTO__control_6     MEPHISTO_PREFIX "control_6"
#define MEPHISTO__control_7     MEPHISTO_PREFIX "control_7"
#define MEPHISTO__control_8     MEPHISTO_PREFIX "control_8"
#define MEPHISTO__control_9     MEPHISTO_PREFIX "control_9"
#define MEPHISTO__control_10    MEPHISTO_PREFIX "control_10"
#define MEPHISTO__control_11    MEPHISTO_PREFIX "control_11"
#define MEPHISTO__control_12    MEPHISTO_PREFIX "control_12"
#define MEPHISTO__control_13    MEPHISTO_PREFIX "control_13"
#define MEPHISTO__control_14    MEPHISTO_PREFIX "control_14"
#define MEPHISTO__control_15    MEPHISTO_PREFIX "control_15"
#define MEPHISTO__control_16    MEPHISTO_PREFIX "control_16"

#define MEPHISTO__controlMin_1     MEPHISTO_PREFIX "controlMin_1"
#define MEPHISTO__controlMin_2     MEPHISTO_PREFIX "controlMin_2"
#define MEPHISTO__controlMin_3     MEPHISTO_PREFIX "controlMin_3"
#define MEPHISTO__controlMin_4     MEPHISTO_PREFIX "controlMin_4"
#define MEPHISTO__controlMin_5     MEPHISTO_PREFIX "controlMin_5"
#define MEPHISTO__controlMin_6     MEPHISTO_PREFIX "controlMin_6"
#define MEPHISTO__controlMin_7     MEPHISTO_PREFIX "controlMin_7"
#define MEPHISTO__controlMin_8     MEPHISTO_PREFIX "controlMin_8"
#define MEPHISTO__controlMin_9     MEPHISTO_PREFIX "controlMin_9"
#define MEPHISTO__controlMin_10    MEPHISTO_PREFIX "controlMin_10"
#define MEPHISTO__controlMin_11    MEPHISTO_PREFIX "controlMin_11"
#define MEPHISTO__controlMin_12    MEPHISTO_PREFIX "controlMin_12"
#define MEPHISTO__controlMin_13    MEPHISTO_PREFIX "controlMin_13"
#define MEPHISTO__controlMin_14    MEPHISTO_PREFIX "controlMin_14"
#define MEPHISTO__controlMin_15    MEPHISTO_PREFIX "controlMin_15"
#define MEPHISTO__controlMin_16    MEPHISTO_PREFIX "controlMin_16"

#define MEPHISTO__controlMax_1     MEPHISTO_PREFIX "controlMax_1"
#define MEPHISTO__controlMax_2     MEPHISTO_PREFIX "controlMax_2"
#define MEPHISTO__controlMax_3     MEPHISTO_PREFIX "controlMax_3"
#define MEPHISTO__controlMax_4     MEPHISTO_PREFIX "controlMax_4"
#define MEPHISTO__controlMax_5     MEPHISTO_PREFIX "controlMax_5"
#define MEPHISTO__controlMax_6     MEPHISTO_PREFIX "controlMax_6"
#define MEPHISTO__controlMax_7     MEPHISTO_PREFIX "controlMax_7"
#define MEPHISTO__controlMax_8     MEPHISTO_PREFIX "controlMax_8"
#define MEPHISTO__controlMax_9     MEPHISTO_PREFIX "controlMax_9"
#define MEPHISTO__controlMax_10    MEPHISTO_PREFIX "controlMax_10"
#define MEPHISTO__controlMax_11    MEPHISTO_PREFIX "controlMax_11"
#define MEPHISTO__controlMax_12    MEPHISTO_PREFIX "controlMax_12"
#define MEPHISTO__controlMax_13    MEPHISTO_PREFIX "controlMax_13"
#define MEPHISTO__controlMax_14    MEPHISTO_PREFIX "controlMax_14"
#define MEPHISTO__controlMax_15    MEPHISTO_PREFIX "controlMax_15"
#define MEPHISTO__controlMax_16    MEPHISTO_PREFIX "controlMax_16"

#define MEPHISTO__controlStep_1     MEPHISTO_PREFIX "controlStep_1"
#define MEPHISTO__controlStep_2     MEPHISTO_PREFIX "controlStep_2"
#define MEPHISTO__controlStep_3     MEPHISTO_PREFIX "controlStep_3"
#define MEPHISTO__controlStep_4     MEPHISTO_PREFIX "controlStep_4"
#define MEPHISTO__controlStep_5     MEPHISTO_PREFIX "controlStep_5"
#define MEPHISTO__controlStep_6     MEPHISTO_PREFIX "controlStep_6"
#define MEPHISTO__controlStep_7     MEPHISTO_PREFIX "controlStep_7"
#define MEPHISTO__controlStep_8     MEPHISTO_PREFIX "controlStep_8"
#define MEPHISTO__controlStep_9     MEPHISTO_PREFIX "controlStep_9"
#define MEPHISTO__controlStep_10    MEPHISTO_PREFIX "controlStep_10"
#define MEPHISTO__controlStep_11    MEPHISTO_PREFIX "controlStep_11"
#define MEPHISTO__controlStep_12    MEPHISTO_PREFIX "controlStep_12"
#define MEPHISTO__controlStep_13    MEPHISTO_PREFIX "controlStep_13"
#define MEPHISTO__controlStep_14    MEPHISTO_PREFIX "controlStep_14"
#define MEPHISTO__controlStep_15    MEPHISTO_PREFIX "controlStep_15"
#define MEPHISTO__controlStep_16    MEPHISTO_PREFIX "controlStep_16"

#define MEPHISTO__controlType_1     MEPHISTO_PREFIX "controlType_1"
#define MEPHISTO__controlType_2     MEPHISTO_PREFIX "controlType_2"
#define MEPHISTO__controlType_3     MEPHISTO_PREFIX "controlType_3"
#define MEPHISTO__controlType_4     MEPHISTO_PREFIX "controlType_4"
#define MEPHISTO__controlType_5     MEPHISTO_PREFIX "controlType_5"
#define MEPHISTO__controlType_6     MEPHISTO_PREFIX "controlType_6"
#define MEPHISTO__controlType_7     MEPHISTO_PREFIX "controlType_7"
#define MEPHISTO__controlType_8     MEPHISTO_PREFIX "controlType_8"
#define MEPHISTO__controlType_9     MEPHISTO_PREFIX "controlType_9"
#define MEPHISTO__controlType_10    MEPHISTO_PREFIX "controlType_10"
#define MEPHISTO__controlType_11    MEPHISTO_PREFIX "controlType_11"
#define MEPHISTO__controlType_12    MEPHISTO_PREFIX "controlType_12"
#define MEPHISTO__controlType_13    MEPHISTO_PREFIX "controlType_13"
#define MEPHISTO__controlType_14    MEPHISTO_PREFIX "controlType_14"
#define MEPHISTO__controlType_15    MEPHISTO_PREFIX "controlType_15"
#define MEPHISTO__controlType_16    MEPHISTO_PREFIX "controlType_16"

#define MEPHISTO__controlLabel_1     MEPHISTO_PREFIX "controlLabel_1"
#define MEPHISTO__controlLabel_2     MEPHISTO_PREFIX "controlLabel_2"
#define MEPHISTO__controlLabel_3     MEPHISTO_PREFIX "controlLabel_3"
#define MEPHISTO__controlLabel_4     MEPHISTO_PREFIX "controlLabel_4"
#define MEPHISTO__controlLabel_5     MEPHISTO_PREFIX "controlLabel_5"
#define MEPHISTO__controlLabel_6     MEPHISTO_PREFIX "controlLabel_6"
#define MEPHISTO__controlLabel_7     MEPHISTO_PREFIX "controlLabel_7"
#define MEPHISTO__controlLabel_8     MEPHISTO_PREFIX "controlLabel_8"
#define MEPHISTO__controlLabel_9     MEPHISTO_PREFIX "controlLabel_9"
#define MEPHISTO__controlLabel_10    MEPHISTO_PREFIX "controlLabel_10"
#define MEPHISTO__controlLabel_11    MEPHISTO_PREFIX "controlLabel_11"
#define MEPHISTO__controlLabel_12    MEPHISTO_PREFIX "controlLabel_12"
#define MEPHISTO__controlLabel_13    MEPHISTO_PREFIX "controlLabel_13"
#define MEPHISTO__controlLabel_14    MEPHISTO_PREFIX "controlLabel_14"
#define MEPHISTO__controlLabel_15    MEPHISTO_PREFIX "controlLabel_15"
#define MEPHISTO__controlLabel_16    MEPHISTO_PREFIX "controlLabel_16"

#define NCONTROLS 16
#define MAX_NPROPS (5 + 6*NCONTROLS)
#define CODE_SIZE 0x10000 // 64 K
#define ERROR_SIZE 0x2000 // 8 K
#define BUF_SIZE (CODE_SIZE * 4)
#define LABEL_SIZE 0x80 // 128

#define CONTROL(NUM) \
{ \
	.property = MEPHISTO_PREFIX"control_"#NUM, \
	.offset = offsetof(plugstate_t, control) + (NUM-1)*sizeof(float), \
	.type = LV2_ATOM__Float, \
	.event_cb = _intercept_control \
}, \
{ \
	.access = LV2_PATCH__readable, \
	.property = MEPHISTO_PREFIX"controlMin_"#NUM, \
	.offset = offsetof(plugstate_t, control_min) + (NUM-1)*sizeof(float), \
	.type = LV2_ATOM__Float \
}, \
{ \
	.access = LV2_PATCH__readable, \
	.property = MEPHISTO_PREFIX"controlMax_"#NUM, \
	.offset = offsetof(plugstate_t, control_max) + (NUM-1)*sizeof(float), \
	.type = LV2_ATOM__Float \
}, \
{ \
	.access = LV2_PATCH__readable, \
	.property = MEPHISTO_PREFIX"controlStep_"#NUM, \
	.offset = offsetof(plugstate_t, control_step) + (NUM-1)*sizeof(float), \
	.type = LV2_ATOM__Float \
}, \
{ \
	.access = LV2_PATCH__readable, \
	.property = MEPHISTO_PREFIX"controlType_"#NUM, \
	.offset = offsetof(plugstate_t, control_type) + (NUM-1)*sizeof(int32_t), \
	.type = LV2_ATOM__Int \
}, \
{ \
	.access = LV2_PATCH__readable, \
	.property = MEPHISTO_PREFIX"controlLabel_"#NUM, \
	.offset = offsetof(plugstate_t, control_label) + (NUM-1)*LABEL_SIZE, \
	.type = LV2_ATOM__String, \
	.max_size = LABEL_SIZE \
}

typedef enum _cntrl_type_t {
	CNTRL_NONE = 0,
	CNTRL_BUTTON,
	CNTRL_CHECK_BUTTON,
	CNTRL_VERTICAL_SLIDER,
	CNTRL_HORIZONTAL_SLIDER,
	CNTRL_NUM_ENTRY,
	CNTRL_HORIZONTAL_BARGRAPH,
	CNTRL_VERTICAL_BARGRAPH,
	CNTRL_SOUND_FILE
} cntrl_type_t;

typedef struct _plugstate_t plugstate_t;

struct _plugstate_t {
	char code [CODE_SIZE];
	char error [ERROR_SIZE];
	float control [NCONTROLS];
	float control_min [NCONTROLS];
	float control_max [NCONTROLS];
	float control_step [NCONTROLS];
	int32_t control_type [NCONTROLS];
	char control_label [NCONTROLS][LABEL_SIZE];
	int32_t xfade_dur;
	int32_t font_height;
	int64_t timestamp;
};

#endif // _MEPHISTO_LV2_H
