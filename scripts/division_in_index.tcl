source scripts/run_hls.tcl

set top "division_in_index"
set design_files {"test/division_in_index_test.cpp"}
set tb_files {"test/division_in_index_test.cpp"}

run_hls $top $design_files $tb_files xcvu9p-flga2104-2-i

