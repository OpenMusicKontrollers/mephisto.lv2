import("stdfaust.lib");

gain_l = hslider("gain left[0]", 0, 0, 1, 0.01);
gain_r = hslider("gain right[1]", 0, 0, 1, 0.01);

process = _ * gain_l, _ * gain_r;

// vim: set syntax=faust:
