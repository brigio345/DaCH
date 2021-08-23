source scripts/run_hls.tcl

set top "unaligned_start_index_0"
set design_files {"test/unaligned_start_index_0_test.cpp"}
set tb_files {"test/unaligned_start_index_0_test.cpp"}

run_hls $top $design_files $tb_files xcvu9p-flga2104-2-i

