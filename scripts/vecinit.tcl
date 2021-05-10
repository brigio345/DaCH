source scripts/run_hls.tcl

set top "vecinit"
set design_files {"src/vecinit_test.cpp"}
set tb_files {"src/vecinit_test.cpp"}

run_hls $top $design_files $tb_files xcvu9p-flga2104-2-i

