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
#include <lv2/lv2plug.in/ns/lv2core/lv2.h>

#define MEPHISTO_URI    "http://open-music-kontrollers.ch/lv2/mephisto"
#define MEPHISTO_PREFIX MEPHISTO_URI "#"

// plugin uris
#define MEPHISTO__mono          MEPHISTO_PREFIX "mono"
#define MEPHISTO__stereo        MEPHISTO_PREFIX "stereo"
#define MEPHISTO__cv            MEPHISTO_PREFIX "cv"

// plugin UI uris
#define MEPHISTO__showUI        MEPHISTO_PREFIX "showUI"
#define MEPHISTO__kxUI          MEPHISTO_PREFIX "kxUI"

// param uris
#define MEPHISTO__code          MEPHISTO_PREFIX "code"
#define MEPHISTO__error         MEPHISTO_PREFIX "error"
#define MEPHISTO__xfadeDuration MEPHISTO_PREFIX "xfadeDuration"
#define MEPHISTO__releaseDuration MEPHISTO_PREFIX "releaseDuration"
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

#define NCONTROLS 16
#define MAX_NPROPS (4 + NCONTROLS)
#define CODE_SIZE 0x10000 // 64 K
#define ERROR_SIZE 0x400 // 1 K
#define BUF_SIZE (CODE_SIZE * 4)

#define CONTROL(NUM) \
{ \
	.property = MEPHISTO_PREFIX"control_"#NUM, \
	.offset = offsetof(plugstate_t, control) + (NUM-1)*sizeof(float), \
	.type = LV2_ATOM__Float, \
	.event_cb = _intercept_control \
}

typedef struct _plugstate_t plugstate_t;

struct _plugstate_t {
	char code [CODE_SIZE];
	char error [ERROR_SIZE];
	float control [NCONTROLS];
	int32_t xfade_dur;
	int32_t release_dur;
};

#endif // _MEPHISTO_LV2_H
