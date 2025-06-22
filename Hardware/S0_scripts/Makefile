# Parameters to run this Makefile:
# 0. PROJ_PATH: the path to the project, for example: ICARUS/VRU
# 1. HLS_BUILD_NAME: the name of the build for HLS, for example: build_hls
# 2. FC_BUILD_NAME: the name of the build for Fusion Compiler, for example: build_fc	
# 3. CLK_PERIOD_HLS: the clock period in ns for HLS, for example: 2.0
# 4. CLK_PERIOD_FC: the clock period in ns for FC, for example: 2.0
# 5. TECH_NODE: the technology node to use, for example: cat

# This makefile has the following targets to run by users:
# 1. hls, 
# -- this target will create a build folder in TOT/A2_hls/$(PROJ_PATH)
# -- the output files will be in TOT/A2_hls/$(PROJ_PATH)/$(HLS_BUILD_NAME)
# -- to run this target, HLS_BUILD_NAME must be defined
# -- Usage: make hls PROJ_PATH=ICARUS/VRU HLS_BUILD_NAME=build_hls (GUI=1)
# -- Usage: make hlsclean PROJ_PATH=ICARUS/VRU HLS_BUILD_NAME=build_hls to clean the build folder
# 2. fc, 
# -- this target will create a build folder in TOT/A4_fusion/$(PROJ_PATH)
# -- the output files will be in TOT/A4_fusion/$(PROJ_PATH)/$(FC_BUILD_NAME)
# -- to run this target, HLS_BUILD_NAME and FC_BUILD_NAME must be defined
# -- Usage: make fc PROJ_PATH=ICARUS/VRU HLS_BUILD_NAME=build_hls FC_BUILD_NAME=build_fc (GUI=1)
# -- Usage: make fclean PROJ_PATH=ICARUS/VRU HLS_BUILD_NAME=build_hls FC_BUILD_NAME=build_fc to clean the build folder
# 3. pwr,
# -- this target will create a build folder in TOT/A5_pwr/$(PROJ_PATH)
# -- the output files will be in TOT/A5_pwr/$(PROJ_PATH)/$(FC_BUILD_NAME)
# -- to run this target, HLS_BUILD_NAME and FC_BUILD_NAME must be defined
# -- Usage: make pwr PROJ_PATH=ICARUS/VRU HLS_BUILD_NAME=build_hls FC_BUILD_NAME=build_fc (GUI=1)
# -- Usage: make pwrclean PROJ_PATH=ICARUS/VRU HLS_BUILD_NAME=build_hls FC_BUILD_NAME=build_fc to clean the build folder



# -------------------- Start of the script, users should not change the following --------------------
# Check if PROJ_PATH is defined
ifeq ($(PROJ_PATH),)
    $(error "PROJ_PATH variable is not defined. Please use 'make all PROJ_PATH=XXX' to specify it.")
endif

# if TECH_NODE is not defined, set it to cat
ifeq ($(TECH_NODE), tn28rvt9t)
	TECHTCLP:=tn28rvt9t.tcl
else ifeq ($(TECH_NODE), saedn14rvt)
	TECHTCLP:=saedn14rvt.tcl
else
    TECHTCLP:=catapult.tcl
endif

# phrase project path and set the design name
# PROJ_PATH must be defined by users for all targets
DESIGN_NAME:=$(shell basename $(PROJ_PATH))

ifeq ($(CLK_PERIOD),)
	CLK_PERIOD:=1.0
endif

TARGET_FILE = TOT_EICHW
CURRENT_DIR := $(CURDIR)
TOT_DIR := $(CURRENT_DIR:/S0_scripts=)

PROJ_HLS_SRC_PATH = $(TOT_DIR)/A1_cmod/$(PROJ_PATH)/
PROJ_HLS_BUILD_PATH = $(TOT_DIR)/A2_hls/$(PROJ_PATH)/$(HLS_BUILD_NAME)
PROJ_FC_BUILD_PATH = $(TOT_DIR)/A4_fusion/$(PROJ_PATH)/$(FC_BUILD_NAME)
PROJ_PWR_BUILD_PATH = $(TOT_DIR)/A5_pwr/$(PROJ_PATH)/$(FC_BUILD_NAME)

HLS_USER_CMD_PATH = $(TOT_DIR)/S1_projconf/$(PROJ_PATH)/catapult.user.tcl
HLS_FLAG_PATH = $(TOT_DIR)/S1_projconf/$(PROJ_PATH)/hlsflags.mk
HLS_INCLUDE_PATH = $(TOT_DIR)/S1_projconf/$(PROJ_PATH)/hlsinclude.mk
POWER_CONF_PATH = $(TOT_DIR)/S1_projconf/$(PROJ_PATH)/pwrconf.mk


.PHONY: end all Welcome hls hlsclean fc fclean pwr pwrclean

end:
	@echo " "
	@echo "============EICHW - Build Hardware Design #End# ============"
	@echo " "

clean: 
	-rm .tot_path *.log *.ccs
	-rm -rf $(DESIGN_NAME)_$(HLS_BUILD_NAME)
	-rm -rf $(DESIGN_NAME)_$(HLS_BUILD_NAME)_*

Welcome:
	@echo " "
	@echo "============EICHW - Build Hardware Design #Start# ============"
	@echo " "
	@echo "> Initialization and env defination"
	@echo "    Current time: $(shell date)"
	@echo "    Current design: $(DESIGN_NAME)"
	@echo "    HLS build name: $(HLS_BUILD_NAME)"
	@echo "    FC build name: $(FC_BUILD_NAME)"
	@echo "    Clock period: $(CLK_PERIOD)"
	@echo "    Technology node: $(TECH_NODE)"
	@echo "    TOT path: $(TOT_DIR)"
	@echo "    HLS src path: $(PROJ_HLS_SRC_PATH)"
	@echo "    HLS build path: $(PROJ_HLS_BUILD_PATH)"
	@echo "    FC build path: $(PROJ_FC_BUILD_PATH)"
	@echo "    PWR build path: $(PROJ_PWR_BUILD_PATH)"
	@echo " "

hls: Welcome
	@echo " "
	@echo "> Run HLS Synthesis"
	@CURR_PATH=$(CURDIR); \
	INCLUDE_PATH="$(shell cat $(HLS_INCLUDE_PATH))"; \
	FLAGS_PATH="$(shell cat $(HLS_FLAG_PATH))"; \
	HEADER_FILES=$$(find $(PROJ_HLS_SRC_PATH) -name "*.h"); \
	TESTBENCH_FILES=$(PROJ_HLS_SRC_PATH)testbench.cpp; \
	echo "    HEADER_FILES: $$HEADER_FILES"; \
	echo "    TESTBENCH_FILES: $$TESTBENCH_FILES"; \
	echo "    INCLUDE_PATH: $$INCLUDE_PATH"; \
	echo "    FLAGS_PATH: $$FLAGS_PATH"; \
	export HEADER_FILES_CAT=$$HEADER_FILES; \
	export TOT_PATH_CAT=$(TOT_DIR)/; \
	export TESTBENCH_FILES_CAT=$$TESTBENCH_FILES; \
	export INCLUDE_PATH_CAT=$$INCLUDE_PATH; \
	export FLAGS_PATH_CAT=$$FLAGS_PATH; \
	export DESIGN_NAME_CAT=$(DESIGN_NAME); \
	export TECH_NODE_CAT=$(TECH_NODE); \
	export HLS_BUILD_NAME_CAT=$(HLS_BUILD_NAME); \
	export CLK_PERIOD_CAT=$(CLK_PERIOD); \
	mkdir -p $(PROJ_HLS_BUILD_PATH); \
	cd $(PROJ_HLS_BUILD_PATH); \
	if [ -z $$GUI ]; then \
		echo "    Run catapult in shell mode, you can rerun and add -GUI to open gui for debugging"; \
		catapult -product ultra -file $(HLS_USER_CMD_PATH) -shell; \
	else \
		echo "    Run catapult in GUI mode for debugging"; \
		catapult -product ultra -file $(HLS_USER_CMD_PATH); \
	fi

hlsclean: Welcome
	-rm -rf $(PROJ_HLS_BUILD_PATH)

