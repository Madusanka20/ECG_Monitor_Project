################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/ads1293.c \
../Core/Src/ble_ecg.c \
../Core/Src/ble_probe.c \
../Core/Src/custom_bus.c \
../Core/Src/hci_tl_interface.c \
../Core/Src/main.c \
../Core/Src/stm32u5xx_hal_msp.c \
../Core/Src/stm32u5xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32u5xx.c 

C_DEPS += \
./Core/Src/ads1293.d \
./Core/Src/ble_ecg.d \
./Core/Src/ble_probe.d \
./Core/Src/custom_bus.d \
./Core/Src/hci_tl_interface.d \
./Core/Src/main.d \
./Core/Src/stm32u5xx_hal_msp.d \
./Core/Src/stm32u5xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32u5xx.d 

OBJS += \
./Core/Src/ads1293.o \
./Core/Src/ble_ecg.o \
./Core/Src/ble_probe.o \
./Core/Src/custom_bus.o \
./Core/Src/hci_tl_interface.o \
./Core/Src/main.o \
./Core/Src/stm32u5xx_hal_msp.o \
./Core/Src/stm32u5xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32u5xx.o 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m33 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32U575xx -c -I../Core/Inc -I../Drivers/STM32U5xx_HAL_Driver/Inc -I../Drivers/STM32U5xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32U5xx/Include -I../Drivers/CMSIS/Include -I../BlueNRG-2/Target -I../Middlewares/ST/BlueNRG-2/hci/hci_tl_patterns/Basic -I../Middlewares/ST/BlueNRG-2/utils -I../Middlewares/ST/BlueNRG-2/includes -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/ads1293.cyclo ./Core/Src/ads1293.d ./Core/Src/ads1293.o ./Core/Src/ads1293.su ./Core/Src/ble_ecg.cyclo ./Core/Src/ble_ecg.d ./Core/Src/ble_ecg.o ./Core/Src/ble_ecg.su ./Core/Src/ble_probe.cyclo ./Core/Src/ble_probe.d ./Core/Src/ble_probe.o ./Core/Src/ble_probe.su ./Core/Src/custom_bus.cyclo ./Core/Src/custom_bus.d ./Core/Src/custom_bus.o ./Core/Src/custom_bus.su ./Core/Src/hci_tl_interface.cyclo ./Core/Src/hci_tl_interface.d ./Core/Src/hci_tl_interface.o ./Core/Src/hci_tl_interface.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/stm32u5xx_hal_msp.cyclo ./Core/Src/stm32u5xx_hal_msp.d ./Core/Src/stm32u5xx_hal_msp.o ./Core/Src/stm32u5xx_hal_msp.su ./Core/Src/stm32u5xx_it.cyclo ./Core/Src/stm32u5xx_it.d ./Core/Src/stm32u5xx_it.o ./Core/Src/stm32u5xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32u5xx.cyclo ./Core/Src/system_stm32u5xx.d ./Core/Src/system_stm32u5xx.o ./Core/Src/system_stm32u5xx.su

.PHONY: clean-Core-2f-Src

