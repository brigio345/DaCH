source scripts/run_hls.tcl

set top "vecsum"
set design_files {"test/vecsum_test.cpp"}
set tb_files {"test/vecsum_test.cpp"}

run_hls $top $design_files $tb_files

