open_project -reset proj_lk

add_files -cflags "-I src" test/lk_test.cpp 
add_files -tb -cflags "-I src" test/lk_test.cpp 
#add_files -blackbox $::env(XILINX_HLS)/common/technology/generic/print/display_int.json 
#add_files -blackbox $::env(XILINX_HLS)/common/technology/generic/print/display_none.json 

set_top knp_top

open_solution solution1

set_part virtexu

create_clock -period 10

csynth_design
cosim_design 

