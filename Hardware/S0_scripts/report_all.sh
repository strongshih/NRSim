#!/usr/bin/env bash

modules=(
  "CICERO/AG"
  "CICERO/NPU"
  "CICERO/NPU_PE"
  "CICERO/reducer"
  "GauRast/PE"
  "GauRast/PE_array"
  "GauRast/VRU"
  "GSCore/BSU"
  "GSCore/QSU"
  "GSCore/VRU"
  "GS_processor/IE"
  "GS_processor/UNIIE"
  "GS_processor/VRU"
  "ICARUS/fixedpoint_mul"
  "ICARUS/ICARUS"
  "ICARUS/MLP_block"
  "ICARUS/MLP_monb"
  "ICARUS/MLP_shared"
  "ICARUS/MLP_sonb"
  "ICARUS/MLP_ssa"
  "ICARUS/MLP_vanilla"
  "ICARUS/PEU"
  "ICARUS/VRU"
  "IRIS/HAMAT"
  "IRIS/RFM"
  "NEUREX/ICU"
  "NEUREX/IGU"
  "NEUREX/NPU"
  "NEUREX/NPU_PE"
  "SRender/CU"
  "SRender/DCU"
  "SRender/NPU"
  "SRender/NPU_PE"
  "SRender/PRU"
  "SRender/SPE"
)

for module in "${modules[@]}"; do
  echo -e "\n==================== $module ===================="

  # Run build flow (HLS, Fusion, Power) for the current module.
  make hls PROJ_PATH="$module" HLS_BUILD_NAME=build_hls FC_BUILD_NAME=build_fc CLK_PERIOD=1.0 TECH_NODE=tn28rvt9t
  make fc  PROJ_PATH="$module" HLS_BUILD_NAME=build_hls FC_BUILD_NAME=build_fc CLK_PERIOD=1.0 TECH_NODE=tn28rvt9t
  make pwr PROJ_PATH="$module" HLS_BUILD_NAME=build_hls FC_BUILD_NAME=build_fc CLK_PERIOD=1.0 TECH_NODE=tn28rvt9t

  # Summarise the generated reports.
  ./report_module.sh "$module"
done