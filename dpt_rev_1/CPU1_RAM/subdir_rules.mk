################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Each subdirectory must supply rules for building sources it contributes
%.obj: ../%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'C2000 Compiler - building file: "$<"'
	"/Applications/ti/ccs2100/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS/bin/cl2000" -v28 -ml -mt --cla_support=cla1 --float_support=fpu32 --tmu_support=tmu0 --vcu_support=vcu2 -Ooff --include_path="/Users/josephinewilliams/Desktop/research-gits/dpt-f2837xd/dpt_rev_1" --include_path="/Users/josephinewilliams/Desktop/research-gits/dpt-f2837xd/dpt_rev_1/device" --include_path="/Applications/ti/c2000/C2000Ware_26_01_00_00/driverlib/f2837xd/driverlib/" --include_path="/Applications/ti/ccs2100/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS/include" --define=DEBUG --define=CPU1 --diag_suppress=10063 --diag_warning=225 --diag_wrap=off --display_error_number --abi=eabi --preproc_with_compile --preproc_dependency="$(basename $(<F)).d_raw" --include_path="/Users/josephinewilliams/Desktop/research-gits/dpt-f2837xd/dpt_rev_1/CPU1_RAM/syscfg" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

build-1224932784: ../epwm_ex2_updown_aq.syscfg
	@echo 'SysConfig - building file: "$<"'
	"/Applications/ti/ccs2100/ccs/utils/sysconfig_1.28.0/sysconfig_cli.sh" -s "/Applications/ti/c2000/C2000Ware_26_01_00_00/.metadata/sdk.json" -d "F2837xD" -p "F2837xD_176PTP" -r "F2837xD_176PTP" --script "/Users/josephinewilliams/Desktop/research-gits/dpt-f2837xd/dpt_rev_1/epwm_ex2_updown_aq.syscfg" -o "syscfg" --compiler ccs
	@echo 'Finished building: "$<"'
	@echo ' '

syscfg/board.c: build-1224932784 ../epwm_ex2_updown_aq.syscfg
syscfg/board.h: build-1224932784
syscfg/board.cmd.genlibs: build-1224932784
syscfg/board.opt: build-1224932784
syscfg/board.json: build-1224932784
syscfg/pinmux.csv: build-1224932784
syscfg/epwm.dot: build-1224932784
syscfg/c2000ware_libraries.cmd.genlibs: build-1224932784
syscfg/c2000ware_libraries.opt: build-1224932784
syscfg/c2000ware_libraries.c: build-1224932784
syscfg/c2000ware_libraries.h: build-1224932784
syscfg/clocktree.h: build-1224932784
syscfg: build-1224932784

syscfg/%.obj: ./syscfg/%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'C2000 Compiler - building file: "$<"'
	"/Applications/ti/ccs2100/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS/bin/cl2000" -v28 -ml -mt --cla_support=cla1 --float_support=fpu32 --tmu_support=tmu0 --vcu_support=vcu2 -Ooff --include_path="/Users/josephinewilliams/Desktop/research-gits/dpt-f2837xd/dpt_rev_1" --include_path="/Users/josephinewilliams/Desktop/research-gits/dpt-f2837xd/dpt_rev_1/device" --include_path="/Applications/ti/c2000/C2000Ware_26_01_00_00/driverlib/f2837xd/driverlib/" --include_path="/Applications/ti/ccs2100/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS/include" --define=DEBUG --define=CPU1 --diag_suppress=10063 --diag_warning=225 --diag_wrap=off --display_error_number --abi=eabi --preproc_with_compile --preproc_dependency="syscfg/$(basename $(<F)).d_raw" --include_path="/Users/josephinewilliams/Desktop/research-gits/dpt-f2837xd/dpt_rev_1/CPU1_RAM/syscfg" --obj_directory="syscfg" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


