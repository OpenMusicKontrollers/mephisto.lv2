declare options "[time:on]";

import("stdfaust.lib");

barBeat = hslider("bar beat[time:barBeat]", 0, 0, 32, 1);
beatsPerBar = hslider("beats per bar[time:beatsPerBar]", 1, 1, 32, 1);
gate = button("speed[time:speed]");

mul = hslider("mul[0]", 0, 0, 1000, 1);
add = hslider("add[1]", 0, 0, 1000, 1);

freq = sin(barBeat / beatsPerBar * ma.PI) * mul + add;

env = en.adsr(0.01, 1.0, 0.8, 0.1, gate);

instr = os.triangle(freq) * env;

process = instr, instr;

// vim: set syntax=faust:
