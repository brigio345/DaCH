source scripts/run_hls.tcl

set top "vecinit"
set design_files {"test/vecinit_test.cpp"}
set tb_files {"test/vecinit_test.cpp"}

run_hls $top $design_files $tb_files

