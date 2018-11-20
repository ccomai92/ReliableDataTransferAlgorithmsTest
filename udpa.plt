set terminal postscript landscape
set nolabel
set xlabel "DropRate"
set xrange [0:12]
set ylabel "usec"
set yrange [0:15000000]
set output "udpa.ps"
plot "1gbpsa-1.dat" title "1gbps slinding window size 1" with linespoints, "1gbpsa-30.dat" title "1gbps sliding window size 30" with linespoints, 5795783 title "1gbps stopNwait" with line
