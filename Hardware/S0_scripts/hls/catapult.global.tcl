# Set Catapult cache
options set Cache/UserCacheHome "${TOT_PATH}Tmps/catapult_cache"
options set Cache/DefaultCacheHomeEnabled false

set TECH_NODE $env(TECH_NODE_CAT)
set PDK_PATH $env(PDK_PATH)
set BASELIB $env(BASELIB)   
set TECHNOLOGY $env(TECHNOLOGY)
set HLS_BUILD_NAME $env(HLS_BUILD_NAME_CAT)

set DESIGN_NAME $env(DESIGN_NAME_CAT)
set CLK_PERIOD $env(CLK_PERIOD_CAT)
set INCLUDE_PATH $env(INCLUDE_PATH_CAT)
set HEADER_FILES $env(HEADER_FILES_CAT)
set TESTBENCH_FILES $env(TESTBENCH_FILES_CAT)
set FLAGS_PATH $env(FLAGS_PATH_CAT)
set FC_BUILD_NAME $env(FC_BUILD_NAME)

puts "DESIGN_NAME: $DESIGN_NAME"
puts "HLS_BUILD_NAME: $HLS_BUILD_NAME"
puts "TECH_NODE: $TECH_NODE"
puts "CLK_PERIOD: $CLK_PERIOD"
puts "INCLUDE_PATH: $INCLUDE_PATH"
puts "HEADER_FILES: $HEADER_FILES"
puts "TESTBENCH_FILES: $TESTBENCH_FILES"
puts "FLAGS_PATH: $FLAGS_PATH"
# Set project directory name (Default: Catapult)
logfile move ${DESIGN_NAME}_${HLS_BUILD_NAME}.log
if { ![file exists ${DESIGN_NAME}_${HLS_BUILD_NAME}.ccs] } {
    project new -directory ${DESIGN_NAME}_${HLS_BUILD_NAME}
}


flow package require /PowerAnalysis
flow package require /SCVerify
flow package require /VCS
flow package require /CDesignChecker
solution options set /Output/PackageOutput false

# This is needed for catapult 2022, but can be skipped for 2024
solution options set Input/CppStandard c++11

# check whether NOVAS_INST_DIR has been set before your launch Catapult.
solution options set /Flows/LowPower/SWITCHING_ACTIVITY_TYPE fsdb

solution options set /Flows/SCVerify/USE_MSIM true
solution options set /Flows/SCVerify/USE_OSCI false
solution options set /Flows/SCVerify/USE_VCS true

options set Flows/QuestaSIM/Path {$MODEL_TECH/bin}
options set Flows/QuestaSIM/DEF_MODELSIM_INI {$MODEL_TECH/modelsim.ini}

solution options set /Flows/VCS/VCS_HOME $env(VCS_HOME)
solution options set /Flows/VCS/VG_GNU_PACKAGE $env(VG_GNU_PACKAGE)
solution options set /Flows/VCS/VG_ENV64_SCRIPT source_me.csh
solution options set /Flows/VCS/SYSC_VERSION 2.3.3
# solution options set /Flows/VCS/SYSC_VERSION 2.3.1

set SKIP_TECH_LIBS false

if { $TECH_NODE eq "SAEDN14RVT" } {
    set TECH_PATH xxx
    set CELL_NAME xxx
    set RAMC_PATH xxx
    set VENDOR SAED
    set TECHNOLOGY N14RVT
} elseif { $TECH_NODE eq "tn28rvt9t" } {
    # add base library search path
    solution options set /ComponentLibs/SearchPath $PDK_PATH/CAT -append
    # add memory library search path
    solution options set /ComponentLibs/SearchPath $PDK_PATH/CAT/Memory -append

    # add base library and design ware library
    solution library add $BASELIB -- -rtlsyntool DesignCompiler -vendor TSMC -technology $TECHNOLOGY
    solution library add $BASELIB

} else {
    echo "WARNING: No HLS libs available, use catapult default libraries."
    set SKIP_TECH_LIBS true
}

if {$SKIP_TECH_LIBS} {
    puts "Skip tech libs"
	solution library add nangate-45nm_beh -- -rtlsyntool OasysRTL -vendor Nangate -technology 045nm
    solution library add ram_nangate-45nm_pipe_beh
    solution library add ram_nangate-45nm-dualport_beh
    solution library add ram_nangate-45nm-separate_beh
    solution library add ram_nangate-45nm-singleport_beh
    solution library add ram_nangate-45nm-register-file_beh
    solution library add rom_nangate-45nm_beh
    solution library add rom_nangate-45nm-sync_regin_beh
    solution library add rom_nangate-45nm-sync_regout_beh
} else {
    puts "Use assigned tech libs"
}

# set ram_cells { xx1 xx2 xx3 }

# foreach ram $ram_cells {
#     solution library add $ram -file $RAMC_PATH/$ram.lib -rtlsyntools DesignCompiler -vendor $VENDOR -technology $TECHNOLOGY
# }


namespace eval hls {

    proc do_catapult {} {
        global DESIGN_NAME CLK_PERIOD INCLUDE_PATH HEADER_FILES TESTBENCH_FILES TOT_PATH FLAGS_PATH

        puts "CLK_PERIOD: $CLK_PERIOD"

        options set Input/SearchPath $INCLUDE_PATH

        solution file add [list $HEADER_FILES] -type C++
        solution file add [list $TESTBENCH_FILES] -type C++ -exclude true

        # DHLS_CATAPULT is a macro that is used to enable Catapult-specific code in matchlib, must be set
        # according to https://github.com/NVlabs/matchlib/blob/main/README.md
        # we skipped random stall here, please test them in c cim
        options set Input/CompilerFlags "$FLAGS_PATH"

        user_global_directives
        go analyze

        solution design set $DESIGN_NAME -top
        setup_clocks $CLK_PERIOD

        user_pre_compile
        go compile

        go libraries

        user_pre_assem
        go assembly

        user_pre_arch
        go architect

        user_pre_extract
        go extract


        
        options set Flows/VCS/VCS_DOFILE "${TOT_PATH}S0_scripts/sim/vcs_fsdb.tcl"
        flow run /SCVerify/launch_make ./scverify/Verify_concat_sim_rtl_v_vcs.mk {} SIMTOOL=vcs build
        flow run /SCVerify/launch_make ./scverify/Verify_concat_sim_rtl_v_vcs.mk {} SIMTOOL=vcs sim

        go switch
        flow run /PowerAnalysis/report_pre_pwropt_Verilog

        project save

    }

    proc user_global_directives {} {}
    proc user_pre_compile {} {}
    proc setup_clocks {} {}
    proc user_pre_arch {} {}
    proc user_pre_extract {} {}

}
