source scripts/run_hls.tcl

set top "bitonic_sort"
set design_files {"test/bitonic_sort_test.cpp"}
set tb_files {"test/bitonic_sort_test.cpp"}

run_hls $top $design_files $tb_files

