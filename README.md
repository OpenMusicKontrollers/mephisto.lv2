## Mephisto

### a Just-in-Time FAUST compiler embedded in an LV2 plugin

Write LV2 audio/cv instruments/filters directly in your host in FAUST
DSP language without any need to restart/reload host or plugin upon code changes.

Use it for one-off instruments/filters, prototyping, experimenting or
glueing stuff together.

*Note: This is an early release, it may thus have rough edges and will need a
fairly recent libFAUST and/or bleeding edge GNU/Linux distribution.*

#### Build status

[![build status](https://gitlab.com/OpenMusicKontrollers/mephisto.lv2/badges/master/build.svg)](https://gitlab.com/OpenMusicKontrollers/mephisto.lv2/commits/master)

### Binaries

For GNU/Linux (64-bit, 32-bit).

To install the plugin bundle on your system, simply copy the __mephisto.lv2__
folder out of the platform folder of the downloaded package into your
[LV2 path](http://lv2plug.in/pages/filesystem-hierarchy-standard.html).

#### Stable release

* [mephisto.lv2-0.2.2.zip](https://dl.open-music-kontrollers.ch/mephisto.lv2/stable/mephisto.lv2-0.2.2.zip) ([sig](https://dl.open-music-kontrollers.ch/mephisto.lv2/stable/mephisto.lv2-0.2.2.zip.sig))

#### Unstable (nightly) release

* [mephisto.lv2-latest-unstable.zip](https://dl.open-music-kontrollers.ch/mephisto.lv2/unstable/mephisto.lv2-latest-unstable.zip) ([sig](https://dl.open-music-kontrollers.ch/mephisto.lv2/unstable/mephisto.lv2-latest-unstable.zip.sig))

### Sources

#### Stable release

* [mephisto.lv2-0.2.2.tar.xz](https://git.open-music-kontrollers.ch/lv2/mephisto.lv2/snapshot/mephisto.lv2-0.2.2.tar.xz)

#### Git repository

* <https://git.open-music-kontrollers.ch/lv2/mephisto.lv2>

<!--
### Packages

* [ArchLinux](https://www.archlinux.org/packages/community/x86_64/mephisto.lv2/)
-->

### Bugs and feature requests

* [Gitlab](https://gitlab.com/OpenMusicKontrollers/mephisto.lv2)
* [Github](https://github.com/OpenMusicKontrollers/mephisto.lv2)

#### Plugins

##### Audio 1x1

1x1 Audio version of the plugin.

Prototype new audio filters and instruments in [FAUST](https://faust.grame.fr)
directly in your favorite running host, without the need to restart the latter
after code changes.

##### Audio 2x2

2x2 Audio version of the plugin.

Prototype new audio filters and instruments in [FAUST](https://faust.grame.fr)
directly in your favorite running host, without the need to restart the latter
after code changes.

##### Audio 4x4

4x4 Audio version of the plugin.

Prototype new audio filters and instruments in [FAUST](https://faust.grame.fr)
directly in your favorite running host, without the need to restart the latter
after code changes.

##### Audio 8x8

8x8 Audio version of the plugin.

Prototype new audio filters and instruments in [FAUST](https://faust.grame.fr)
directly in your favorite running host, without the need to restart the latter
after code changes.

##### CV 1x1

1x1 CV version of the plugin.

Prototype new CV filters and instruments in [FAUST](https://faust.grame.fr)
directly in your favorite running host, without the need to restart the latter
after code changes.

##### CV 2x2

2x2 CV version of the plugin.

Prototype new CV filters and instruments in [FAUST](https://faust.grame.fr)
directly in your favorite running host, without the need to restart the latter
after code changes.

##### CV 4x4

4x4 CV version of the plugin.

Prototype new CV filters and instruments in [FAUST](https://faust.grame.fr)
directly in your favorite running host, without the need to restart the latter
after code changes.

##### CV 8x8

8x8 CV version of the plugin.

Prototype new CV filters and instruments in [FAUST](https://faust.grame.fr)
directly in your favorite running host, without the need to restart the latter
after code changes.

#### Dependencies

* [LV2](http://lv2plug.in) (LV2 Plugin Standard)
* [FAUST](https://faust.grame.fr/) (Faust Programming Language >=2.14.4)

#### Build / install

	git clone https://git.open-music-kontrollers.ch/lv2/mephisto.lv2
	cd mephisto.lv2
	meson build
	cd build
	ninja -j4
	sudo ninja install
	ninja test

#### GUI

This plugin features an external LV2 plugin GUI, which does nothing else than
just opening the plugin's FAUST source in your favorite editor and monitor its
modification state. Additionally it opens a log file to write compile errors to.

Currently, the editor has to be defined via the environment variable
*MEPHISTO_EDITOR*.

    export MEPHISTO_EDITOR='gedit'
    export MEPHISTO_EDITOR='xterm -e emacs'
    export MEPHISTO_EDITOR='urxvt -e vim -o2'

If no environment variable is defined, the default fallback invocation commands
are defined as follows:

* 'xterm -e vi' (Unix)
<!--
* 'open -nW' (MacOS)
* 'cmd /c start /wait' (Windows)
-->

Whenever you save the FAUST source, the plugin will try to just-in-time compile and
inject it. Potential warnings and errors are reported in the plugin host's log
and the log file.

#### Controls

The plugin supports up to 16 controls implemented as LV2
[Parameters](http://lv2plug.in/ns/lv2core/lv2core.html#Parameter). To have
access to them, simply use one of FAUST's active control structures with
[ordering indexes](https://faust.grame.fr/doc/manual/index.html#ordering-ui-elements)
in their labels in your DSP code:

    cntrl1 = hslider("[1]Control 1", 500.0, 10.0, 1000.0, 1.0);
    cntrl2 = hslider("[2]Control 2", 5.0, 1.0, 10.0, 1.0);
    cntrl3 = hslider("[3]Control 3", 0.5, 0.0, 1.0, 0.1);

#### MIDI and polyphony

The plugin supports building instruments with
[MIDI polyphony](https://faust.grame.fr/doc/manual/index.html#midi-polyphony-support).
For this to work you have to enable the MIDI option and declare amount of polyphony
(maximum polyphony is 64).

The plugin automatically derives the 3 control signals:

* gate (NoteOn vs NoteOff)
* freq (NoteOn-note + PitchBend), honouring PitchBend range RPN 0/0
* gain (NoteOn-velocity)
* pressure (NotePressure aka polyphonic aftertouch)

Additionally, the following MIDI ControlChanges are supported:

* SustainPedal
* AllNotesOff
* AllSoundsOff

Other MIDI events are not supported as of today and thus should be
automated via the plugin host to one of the 16 control slots.

    declare options("[midi:on][nvoices:32]");

    freq = hslider("freq", 0, 0, 1, 0.1);
    gain = hslider("gain", 0, 0, 1, 0.1);
    gate = button("gate");
    pressure = button("gate");
    pressure = hslider("pressure", 0.0, 0.0, 1.0, 0.1);

    cntrl1 = hslider("[1]Control 1", 500.0, 10.0, 1000.0, 1.0);
    cntrl2 = hslider("[2]Control 2", 5.0, 1.0, 10.0, 1.0);
    cntrl3 = hslider("[3]Control 3", 0.5, 0.0, 1.0, 0.1);

#### OSC

OSC events are not supported as of today and thus should be automated via
the plugin host to one of the 16 control slots.

#### Time

The plugin supports LV2 [time position](http://lv2plug.in/ns/ext/time/time.html#Position)
events. To have access to them, simply use one of FAUST's active control
structures with the corresponding time metadata in their labels in your DPS code
and additionally enable the time option:

    declare options("[time:on]");

    barBeat = hslider("barBeat[time:barBeat]", 0.0, 0.0, 32.0, 1.0);
    bar = hslider("bar[time:bar]", 0.0, 0.0, 400.0, 1.0);
    beatUnit = hslider("beatUnit[time:beatUnit]", 1.0, 1.0, 32.0, 1.0);
    beatsPerBar = hslider("beatsPerBar[time:beatsPerBar]", 1.0, 1.0, 32.0, 1.0);
    beatsPerMinute = hslider("beatsPerMinute[time:beatsPerMinute]", 1.0, 1.0, 400.0, 1.0);
    frame = hslider("frame[time:frame]", 1.0, 1.0, 400.0, 1.0);
    framesPerSecond = hslider("framesPerSecond[time:framesPerSecond]", 1.0, 1.0, 96000.0, 1.0);
    speed = button("speed[time:speed]");

#### License

Copyright (c) 2019 Hanspeter Portner (dev@open-music-kontrollers.ch)

This is free software: you can redistribute it and/or modify
it under the terms of the Artistic License 2.0 as published by
The Perl Foundation.

This source is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
Artistic License 2.0 for more details.

You should have received a copy of the Artistic License 2.0
along the source as a COPYING file. If not, obtain it from
<http://www.perlfoundation.org/artistic_license_2_0>.
