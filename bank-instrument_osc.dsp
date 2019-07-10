declare options "[nvoices:48][midi:on]";

import("stdfaust.lib");

freq = hslider("synth/freq", 20, 20, 20000, 1);
gain = hslider("synth/gain", 0, 0, 1, 0.01);
gate = button("synth/gate");

env = en.adsr(0.01, 1.0, 0.8, 0.1, gate) * gain;

inst = os.triangle(freq) * env;

process = inst, inst;

// vim: set syntax=faust:
