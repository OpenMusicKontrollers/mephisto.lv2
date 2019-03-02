# mephisto.lv2

## a Just-in-Time FAUST compiler embedded in an LV2 plugin

### Webpage 

Get more information at: [http://open-music-kontrollers.ch/lv2/mephisto](http://open-music-kontrollers.ch/lv2/mephisto)

### Build status

[![build status](https://gitlab.com/OpenMusicKontrollers/mephisto.lv2/badges/master/build.svg)](https://gitlab.com/OpenMusicKontrollers/mephisto.lv2/commits/master)

### Dependencies

* [LV2](http://lv2plug.in) (LV2 Plugin Standard)
* [FAUST](https://faust.grame.fr/) (Faust Programming Language)

### Build / install

	git clone https://git.open-music-kontrollers.ch/lv2/mephisto.lv2
	cd mephisto.lv2
	meson build
	cd build
	ninja -j4
	sudo ninja install

### GUI

This plugin features an external LV2 plugin GUI, which does nothing else than
just opening the plugin's FAUST source in your favorite editor and monitor its
modification state.

Currently, the editor has to be defined via an environment variable. You can
use either the environment varialbe *EDITOR* or *MEPHISTO_EDITOR*, whereby the
latter will take precedence over the former.

    export JIT_EDITOR='urxvt -e nvim'

If no environment variable is defined, the default fallback invocation commands
are defined as follows:

* 'xterm -e vi' (Unix)
* 'open -nW' (MacOS)
* 'cmd /c start /wait' (Windows)

Whenever you save the FAUST source, the plugin will try to just-in-time compile and
inject it. Potential warnings and errors are reported in the plugin host's log.

### License

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
