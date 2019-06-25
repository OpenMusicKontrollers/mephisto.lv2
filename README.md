## Mephisto

### a Just-in-Time FAUST compiler embedded in an LV2 plugin

#### Build status

[![build status](https://gitlab.com/OpenMusicKontrollers/mephisto.lv2/badges/master/build.svg)](https://gitlab.com/OpenMusicKontrollers/mephisto.lv2/commits/master)

### Binaries

For GNU/Linux (64-bit, 32-bit, armv7), Windows (64-bit, 32-bit) and MacOS
(64/32-bit univeral).

To install the plugin bundle on your system, simply copy the __mephisto.lv2__
folder out of the platform folder of the downloaded package into your
[LV2 path](http://lv2plug.in/pages/filesystem-hierarchy-standard.html).

<!--
#### Stable release

* [mephisto.lv2-0.16.0.zip](https://dl.open-music-kontrollers.ch/mephisto.lv2/stable/mephisto.lv2-0.16.0.zip) ([sig](https://dl.open-music-kontrollers.ch/mephisto.lv2/stable/mephisto.lv2-0.16.0.zip.sig))
-->

#### Unstable (nightly) release

* [mephisto.lv2-latest-unstable.zip](https://dl.open-music-kontrollers.ch/mephisto.lv2/unstable/mephisto.lv2-latest-unstable.zip) ([sig](https://dl.open-music-kontrollers.ch/mephisto.lv2/unstable/mephisto.lv2-latest-unstable.zip.sig))

### Sources

<!--
#### Stable release

* [mephisto.lv2-0.16.0.tar.xz](https://git.open-music-kontrollers.ch/lv2/mephisto.lv2/snapshot/mephisto.lv2-0.16.0.tar.xz)
-->

#### Git repository

* <https://git.open-music-kontrollers.ch/lv2/mephisto.lv2>

<!--
### Packages

* [ArchLinux](https://www.archlinux.org/packages/community/x86_64/mephisto.lv2/)
-->

### Bugs and feature requests

* [Gitlab](https://gitlab.com/OpenMusicKontrollers/mephisto.lv2)
<!--
* [Github](https://github.com/OpenMusicKontrollers/mephisto.lv2)
-->

#### Plugins

##### Mono

Mono version of the plugin.

Prototype new audio filters and instruments in [FAUST](https://faust.grame.fr)
directly in your favorite running host, without the need to restart the latter
after code changes.

##### Stereo

Stereo version of plugin.

Prototype new audio filters and instruments in [FAUST](https://faust.grame.fr)
directly in your favorite running host, without the need to restart the latter
after code changes.

#### Dependencies

* [LV2](http://lv2plug.in) (LV2 Plugin Standard)
* [FAUST](https://faust.grame.fr/) (Faust Programming Language)

#### Build / install

	git clone https://git.open-music-kontrollers.ch/lv2/mephisto.lv2
	cd mephisto.lv2
	meson build
	cd build
	ninja -j4
	sudo ninja install

#### GUI

This plugin features an external LV2 plugin GUI, which does nothing else than
just opening the plugin's FAUST source in your favorite editor and monitor its
modification state. Additionally it opens a log file to write compile errors to.

Currently, the editor has to be defined via an environment variable. You can
use either the environment varialbe *EDITOR* or *MEPHISTO_EDITOR*, whereby the
latter will take precedence over the former.

    export MEPHISTO_EDITOR='urxvt -e nvim -o2'

If no environment variable is defined, the default fallback invocation commands
are defined as follows:

* 'xterm -e vim -o2' (Unix)
* 'open -nW' (MacOS)
* 'cmd /c start /wait' (Windows)

Whenever you save the FAUST source, the plugin will try to just-in-time compile and
inject it. Potential warnings and errors are reported in the plugin host's log.

#### Controls

The plugin supports up to 16 controls implemented as LV2
[Parameters](http://lv2plug.in/ns/lv2core/lv2core.html#Parameter). To have
access to them, simply use one of FAUST's active control structures with
[ordering indexes](https://faust.grame.fr/doc/manual/index.html#ordering-ui-elements)
in their labels in your DSP code:

    cntrl1 = hslider("[1]Control 1", 500.0, 10.0, 1000.0, 1.0);
    cntrl2 = hslider("[2]Control 2", 5.0, 1.0, 10.0, 1.0);
    cntrl3 = hslider("[3]Control 3", 0.5, 0.0, 1.0, 0.1);

#### Instruments

The plugin supports building instruments with
[MIDI polyphony](https://faust.grame.fr/doc/manual/index.html#midi-polyphony-support).

    declare options("[midi:on][nvoices:32]");

    freq = hslider("freq", 0, 0, 1, 0.1);
    gain = hslider("gain", 0, 0, 1, 0.1);
    gate = button("gate");

    cntrl1 = hslider("[1]Control 1", 500.0, 10.0, 1000.0, 1.0);
    cntrl2 = hslider("[2]Control 2", 5.0, 1.0, 10.0, 1.0);
    cntrl3 = hslider("[3]Control 3", 0.5, 0.0, 1.0, 0.1);

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
