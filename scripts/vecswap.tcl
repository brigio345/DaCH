source scripts/run_hls.tcl

set top "vecswap"
set design_files {"test/vecswap_test.cpp"}
set tb_files {"test/vecswap_test.cpp"}

run_hls $top $design_files $tb_files xcvu9p-flga2104-2-i

