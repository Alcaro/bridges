#!/bin/sh
rm callgrind.out.*
make OPT=1 DEBUG=1 -j8 && valgrind --tool=callgrind ./bridges --perf && kcachegrind callgrind.out.*
