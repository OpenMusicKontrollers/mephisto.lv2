import("stdfaust.lib");

process = _ <: attach(_,abs : ba.linear2db : hbargraph("Level [0]", -60, 0));

// vim: set syntax=faust:
