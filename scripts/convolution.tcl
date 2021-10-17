source scripts/run_hls.tcl

set top "convolution"
set design_files {"test/convolution_test.cpp"}
set tb_files {"test/convolution_test.cpp"}

run_hls $top $design_files $tb_files virtexu

