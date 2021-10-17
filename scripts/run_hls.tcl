proc run_hls {top_name design_files tb_files {parts {xcvu9p-flgb2104-2-e}}
		{inc_dir "src"} {sol_name "solution1"} {t_clk 4} {axi_latency 3}} {
	open_project -reset "proj_$top_name"
	add_files -cflags "-I $inc_dir" $design_files
	add_files -tb -cflags "-I $inc_dir" $tb_files
	set_top "${top_name}_top"
	open_solution -flow_target vitis -reset $sol_name
	set_part $parts
	create_clock -period $t_clk
	config_interface -m_axi_latency $axi_latency

	csim_design
	csynth_design
	cosim_design -trace_level all
	exit
}

