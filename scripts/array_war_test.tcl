source scripts/run_hls.tcl

set top "array_war"
set design_files {"test/array_war_test.cpp"}
set tb_files {"test/array_war_test.cpp"}

run_hls $top $design_files $tb_files xcvu9p-flga2104-2-i

