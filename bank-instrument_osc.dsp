declare options "[nvoices:48][midi:on]";

import("stdfaust.lib");

freq = hslider("freq", 20, 20, 20000, 1);
gain = hslider("gain", 0, 0, 1, 0.01);
gate = button("gate");

lfo_f = hslider("LFO frequency[0]", 0, 0, 1000, 1);
lfo_a = hslider("LFO amplitude[1]", 0, 0, 1, 0.01);

env = en.adsr(0.01, 1.0, 0.8, 0.1, gate) * gain;
lfo = 1 + os.lf_triangle(lfo_f) * lfo_a;

inst = os.triangle(freq * lfo) * env / 4;

process = inst, inst;

// vim: set syntax=faust:
