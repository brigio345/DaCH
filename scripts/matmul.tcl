source scripts/run_hls.tcl

set top "matmul"
set design_files {"src/matmul_test.cpp"}
set tb_files {"src/matmul_test.cpp"}

run_hls $top $design_files $tb_files xcvu9p-flga2104-2-i

