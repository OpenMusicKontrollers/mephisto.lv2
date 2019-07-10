declare options "[time:on]";

import("stdfaust.lib");

barBeat = hslider("time/barBeat[time:barBeat]", 0, 0, 32, 1);
beatsPerBar = hslider("time/beatsPerBar[time:beatsPerBar]", 1, 1, 32, 1);
gate = button("time/speed[time:speed]");

mul = hslider("parameter/mul[0]", 0, 0, 1000, 1);
add = hslider("parameter/add[1]", 0, 0, 1000, 1);

freq = sin(barBeat / beatsPerBar * ma.PI) * mul + add;

env = en.adsr(0.01, 1.0, 0.8, 0.1, gate);

instr = os.triangle(freq) * env;

process = instr, instr;

// vim: set syntax=faust:
