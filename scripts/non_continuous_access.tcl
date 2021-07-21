source scripts/run_hls.tcl

set top "non_continuous_access"
set design_files {"test/non_continuous_access_test.cpp"}
set tb_files {"test/non_continuous_access_test.cpp"}

run_hls $top $design_files $tb_files xcvu9p-flga2104-2-i

