import("stdfaust.lib");
declare options "[nvoices:48]";

freq = hslider("freq", 20, 20, 20000, 1);
gain = hslider("gain", 0, 0, 1, 0.01);
gate = button("gate");

inst = os.osc(freq) * gain;

process = inst, inst;

// vim: set syntax=faust:
