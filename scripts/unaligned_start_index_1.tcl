source scripts/run_hls.tcl

set top "unaligned_start_index_1"
set design_files {"test/unaligned_start_index_1_test.cpp"}
set tb_files {"test/unaligned_start_index_1_test.cpp"}

run_hls $top $design_files $tb_files xcvu9p-flga2104-2-i

