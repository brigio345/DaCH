set project_name "proj_matmul"
set solution_name "solution1"
set top "matmul_top"

set include_dir "inc"
set design_files {"src/matmul_test.cpp"}
set tb_files {"src/matmul_test.cpp"}

set parts {xczu3eg-sbva484-1-e}
set clock_period 10

# detect wheter script is run by Vivado HLS or Vitis HLS
if {[string compare [file tail $argv0] vivado_hls]} {
	set vivado false
} else {
	set vivado true
}

# create a project
open_project -reset $project_name

# add design files
add_files -cflags "-I $include_dir" $design_files
# add test bench & files
add_files -tb -cflags "-I $include_dir" $tb_files

# set the top-level function
set_top $top

# create a solution
open_solution -reset $solution_name
# define technology and clock rate
set_part $parts
create_clock -period $clock_period

if {$vivado} {
	csim_design -compiler gcc
} else {
	csim_design
}

# set any optimization directives
# end of directives

csynth_design
cosim_design -disable_deadlock_detect
export_design

