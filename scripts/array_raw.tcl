source scripts/run_hls.tcl

set top "array_raw"
set design_files {"test/array_raw_test.cpp"}
set tb_files {"test/array_raw_test.cpp"}

run_hls $top $design_files $tb_files xcvu9p-flga2104-2-i

