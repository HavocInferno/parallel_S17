set title "jacoi-relaxation"
set xlabel "resolution"
set ylabel "MFlop/s"
plot "perf_O0" title "-O0" with lines,\
     "perf_O1" title "-O1" with lines,\
     "perf_O2" title "-O2" with lines,\
     "perf_O3" title "-O3" with lines,\
     "perf_O3_ipo" title "-O3 -ipo" with lines,\
     "perf_O3_xhost" title "-O3 -xhost" with lines,\
     "perf_fast" title "-fast" with lines,\
     "perf_fast_xhost" title "-fast -xhost" with lines

