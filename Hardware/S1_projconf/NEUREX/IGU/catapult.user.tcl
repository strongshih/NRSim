set TOT_PATH $env(TOT_PATH_CAT)
puts "sourcing global setting file ${TOT_PATH}S0_scripts/hls/catapult.global.tcl"
source ${TOT_PATH}S0_scripts/hls/catapult.global.tcl

proc hls::user_global_directives {} {
    # set MIO schduling to false to improve the performance
    # directive set -STRICT_MIO_SCHEDULING false
    # set the IO protocol to standard to avoid channel loop, switch to coupled if there is no loop
    # directive set -CHAN_IO_PROTOCOL standard
}

proc hls::user_pre_compile {} {
    directive set -CLOCK_OVERHEAD 0.000000
}

proc hls::user_pre_assem {} {

}

proc hls::setup_clocks {CLK_PERIOD} {
    puts "CLK_PERIOD in setup_clocks: $CLK_PERIOD"
    directive set -CLOCK_NAME clk
    set CLK_PERIODby2 [expr $CLK_PERIOD/2.0]
    puts "CLK_PERIODby2 in setup_clocks: $CLK_PERIODby2"
	directive set -CLOCKS "clk \"-CLOCK_PERIOD $CLK_PERIOD\""
    # directive set -CLOCKS clk \"-CLOCK_PERIOD $CLK_PERIOD -CLOCK_HIGH_TIME $CLK_PERIODby2 -CLOCK_OFFSET 0.000000 -CLOCK_UNCERTAINTY 0.0 -CLOCK_EDGE rising -RESET_ASYNC_NAME rst -RESET_SYNC_NAME rst -RESET_ASYNC_ACTIVE low -RESET_SYNC_ACTIVE low"
}

proc hls::user_pre_arch {} {
    global DESIGN_NAME
    # add your own directives here
    # directive set /$env(TOP_NAME)/ComputePipelineBlockOA/while -PIPELINE_STALL_MODE flush
}

proc hls::user_pre_extract {} {
    global DESIGN_NAME
    
}

hls::do_catapult
