################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
WebFiles/www/%.obj: ../WebFiles/www/%.c $(GEN_OPTS) | $(GEN_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: ARM Compiler'
	"D:/ti/ccsv8/tools/compiler/ti-cgt-arm_18.1.4.LTS/bin/armcl" -mv7M4 --code_state=16 --float_support=vfplib -me --include_path="D:/CCS8Workspace/DinoBox_CC3220S_LAUNCHXL_freertos_ccs" --include_path="D:/ti/simplelink_cc32xx_sdk_2_40_01_01/source" --include_path="D:/ti/simplelink_cc32xx_sdk_2_40_01_01/source/ti/posix/ccs" --include_path="D:/FreeRTOS/FreeRTOSv10.1.1/FreeRTOS/Source/include" --include_path="D:/FreeRTOS/FreeRTOSv10.1.1/FreeRTOS/Source/portable/CCS/ARM_CM3" --include_path="D:/CCS8Workspace/freertos_builds_CC3220S_LAUNCHXL_release_ccs" --include_path="D:/ti/ccsv8/tools/compiler/ti-cgt-arm_18.1.4.LTS/include" -g --diag_warning=225 --diag_warning=255 --diag_wrap=off --display_error_number --gen_func_subsections=on --preproc_with_compile --preproc_dependency="WebFiles/www/$(basename $(<F)).d_raw" --obj_directory="WebFiles/www" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


