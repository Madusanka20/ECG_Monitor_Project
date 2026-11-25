# STM32U575 ECG bench stream

The firmware streams the ADS1293's three simultaneous channels over the BlueNRG-M2SP. The browser shows I, II and V1, with an option to calculate III, aVR, aVL and aVF. Begin with an ECG simulator. Keep people disconnected during USB, ST-LINK and bench-instrument testing.

## 1. Install the complete files in CubeIDE

1. Back up your existing working project. Keep its .ioc, .project, .cproject, startup assembly, linker script, CMSIS and STM32U5 HAL drivers. This archive is a source overlay for your project.
2. Replace each matching application file with the entire file from Core/Src or Core/Inc. In CubeIDE you can open the supplied file, select all, copy, then select all and paste in the same-named project file. If custom_bus.c or another generated file already lives under a different folder, replace it there. Project search (Ctrl+H) can locate other copies.
3. Four files were absent from the upload. Add Core/Src/hci_tl_interface.c, Core/Inc/hci_tl_interface.h, Core/Inc/bluenrg_conf.h and Core/Inc/ble_list_utils.h if missing. Use the supplied contents for any existing board-specific copies as well. A CubeMX-generated transport under App/ may take precedence over an additional copy under Core/Inc.
4. Use STMicroelectronics X-CUBE-BLE2 **v3.3.0** with the BlueNRG-2 **Basic HCI transport** middleware. Your uploaded RTE_Components.h identifies this version. Keep exactly one compiled SPI2 handle, one application transport and one Basic hci_tl.c. Exclude any older demo application that initializes a second BLE stack. CubeIDE offers Resource Configurations → Exclude from Build for duplicate source files.
5. Select Project → Clean, then Build Project. Flash the STM32 through its own SWD connector. Start the firmware running after programming. Manual edits outside CubeMX user sections can be overwritten by regeneration.

### If your existing project lacks the BLE middleware

Use the official package link below and open Middlewares/ST/BlueNRG-2. Add its includes, utils, and hci/hci_tl_patterns/Basic directories to the GNU C compiler include paths together with your application's Core/Inc. Compile utils/ble_list.c, hci/hci_tl_patterns/Basic/hci_tl.c, hci/bluenrg1_hci_le.c and the .c files in hci/controller. The application handles its three HCI event types directly. The Template transport and Nucleo example main, bus and interface sources should be excluded. Keep the normal STM32U5 project drivers generated for your MCU. If you already have the middleware, use that installation to avoid duplicate symbols.

[ST X-CUBE-BLE2 v3.3.0](https://github.com/STMicroelectronics/x-cube-ble2/tree/v3.3.0)

## 2. Verify the separately programmed Bluetooth module

The source overlay targets the STM32. A bare BlueNRG-M2SP needs a compatible DTM SPI network-coprocessor application loaded on the BlueNRG itself. Your PCB has its own BlueNRG SWD header J2. On the schematic J2-1 is target VDD reference, J2-2 SWDIO, J2-3 SWCLK, J2-4 BLE_RESET and J2-6 GND. Power the board as designed and use the probe's target voltage reference according to the probe instructions. Release BOOT1 for normal power-up. R8 is the external 10 kΩ DIO7 pull-down.

| STM32 | BlueNRG IC2 | Function |
|---|---|---|
| PA8 | DIO7 / pin 7 | IRQ after boot |
| PA9 | DIO1 / pin 18 | Active-low chip select |
| PB12 | RESETN / pin 19 | Active-low module reset |
| PB13 | DIO0 / pin 15 | SPI2 SCK |
| PB14 | DIO2 / pin 16 | SPI2 MISO |
| PB15 | DIO3 / pin 17 | SPI2 MOSI |

Check the **BlueNRG side** of chip select. ST's DT0121 default uses DIO11. Your board routes PA9 to DIO1. If the module firmware expects DIO11, use the M2SP DTM SPI project from STSW-BLUENRG1-DK, configure its chip-select GPIO/pin mux and any CS edge/wakeup selection for DIO1 in the board configuration, rebuild for the M2SP and program the module. Retain DIO0/2/3/7 as shown. The name of a prebuilt image alone does not verify its mapping. A successful HCI version read in step 4 verifies the actual connection. This archive contains no radio image.

[ST network-coprocessor configuration, DT0121](https://www.st.com/resource/en/design_tip/dt0121-how-to-configure-the-bluenrg1-and-bluenrg2-devices-in-network-coprocessor-mode-stmicroelectronics.pdf) · [ST BlueNRG SDK](https://www.st.com/en/embedded-software/stsw-bluenrg1-dk.html)

## 3. Connect the simulator and open the web app

The supplied ADS1293 sheet routes J1 pins 1–5 to IN1–IN5. Verify cable pin numbering before connecting the simulator.

| J1 | Simulator lead | Measurement role |
|---|---|---|
| 1 | RA | Reference arm |
| 2 | LA | I = LA − RA |
| 3 | LL | II = LL − RA |
| 4 | RL | Right-leg drive |
| 5 | V1 | V1 measured against the Wilson central terminal |

1. Select a known ECG simulator output, for example 60 bpm. At 60 bpm the spacing between successive QRS complexes should be about one second. Verify the amplitude against the simulator setting for the specific lead.
2. On the Windows PC, enable Bluetooth. Open the extracted web folder in File Explorer. Type powershell in the address bar and press Enter.
3. Run the command below. Keep PowerShell open. On a system without the Windows py launcher, use python in its place.

   ```powershell
   py -m http.server 8000 --bind 127.0.0.1
   ```

4. Open [the local viewer](http://localhost:8000) in desktop Chrome or Edge. Click **Connect ECG**, choose **STM32-ECG**, and approve the connection. If another BLE inspector is connected to the board, disconnect it first.
5. Start with the measured leads. The viewer shows nominal input mV, received samples/s, and missing sample indices. The additional four limb leads are calculated from I and II. The page processes notifications locally.

[Chrome Web Bluetooth guide](https://developer.chrome.com/docs/capabilities/bluetooth) · [TI ADS1293 datasheet, Tables 8–11, Eq. 13 and §9.2.2](https://www.ti.com/lit/ds/symlink/ads1293.pdf)

## 4. Locate a problem in CubeIDE Live Expressions

| Watch expression | What to expect and what to check |
|---|---|
| ble_irq_in_reset | 0 while the module is held in reset. If 1, check BOOT1, R8 and a possible VDD short on PA8. A high after reset can be a pending event. |
| ble_probe_ok | 1 after a successful HCI Read Local Version command. ble_hci_revision and ble_manufacturer contain the reply. If this fails, check module firmware, CS mapping, common ground and SPI2 signals. |
| ble_init_step / ble_init_err | Step 8 and error 0 indicate successful advertising setup. Stages 1 to 7 cover reset/version, GATT init, GAP init, name, service, characteristic and advertising. On a failure at 1 inspect ble_valid_frames, ble_io_errors and ble_last_header. |
| adc_ready / adc_failed_reg | 1 / 0xFF after register readback. If adc_ready is 0, adc_failed_reg, adc_expected and adc_actual identify the failed register. Check SPI1, reset and power. |
| debug_00 / debug_27 / debug_2F | 0x01 / 0x08 / 0x70 are the expected configuration, DRDY source and three-channel loop-read mask. |
| samples_ok / drdy_count | Normally increase by about 400 per second, even before the browser subscribes. A scope on PB1 should show about 2.50 ms between falling edges. If configuration passes but both counts stall, check PB1/EXTI1 and the 4.096 MHz crystal. adc_restarts counts recoveries. |
| ble_connection / ble_subscribed | 0xFFFF means disconnected. ble_subscribed becomes 1 when the browser enables notifications. |
| ble_pkts_sent / ble_queue_drops | Sent notification count should rise after subscription. Queue drops should ordinarily stay at zero. Sustained drops mean the negotiated link cannot carry the incoming samples. ble_tx_retries counts temporary controller backpressure. |
| ECG_Lead_I / ECG_Lead_II / ECG_Lead_V1 | Centered ADC counts. If samples arrive but the trace looks wrong, verify the simulator cable map, right-leg drive, Wilson reference and physical analog circuitry. |

The debugger can change timing when it halts the CPU. Observe running counters and use a logic analyzer for transaction-level checks. SPI1 is mode 0 at 5 MHz. SPI2 is mode 1 at 625 kHz. CS stays low across the five-byte Bluetooth header and the payload of each transaction.

## Acquisition and wire format

This build deliberately uses **400 samples/s per measured channel**. With the board's 4.096 MHz crystal the settings are SDM 102.4 kHz, R1=4, R2=4, R3=16, nominal 80 Hz ECG bandwidth and ADCMAX=0x800000. For comparison, the uploaded settings used R2=5, R3=6, about 853 samples/s with the default SDM. Changes to these settings require updating the firmware midpoint and the browser sample rate and voltage scale together.

The ADS1293 sends unsigned, offset-coded samples. The firmware subtracts 0x400000. The browser converts centered counts to nominal input mV with a factor of (2 × 2400) / (3.5 × 0x800000), assuming the internal 2.4 V reference. Confirm gain and offset with a calibrated simulator.

| Item | Value |
|---|---|
| Service | 12340000-1234-5678-9abc-def012345678 |
| Notify characteristic | 12340001-1234-5678-9abc-def012345678 |
| Bytes 0–1 | First sample index, little-endian, modulo 65536 |
| Bytes 2–10 | I, II, V1, each a signed little-endian 24-bit centered count |
| Bytes 11–19 | Optional second consecutive sample in the same format |
| Length | 20 bytes normally, 11 bytes to end a run before a sample-index gap |

The sequence identifies serviced DRDY events. It detects queue and transport losses between observed samples, and the firmware inserts a discontinuity marker after an ADC restart. It cannot determine every missed hardware conversion or the duration of a long outage. Two notifications per four samples means about 200 notifications/s at full rate. The host controls the actual BLE connection interval.

## Verification

The source was cross-compiled and linked for Cortex-M33/STM32U575 with GNU Arm Embedded GCC 12.2.1, ST's U5 HAL and CMSIS, and the X-CUBE-BLE2 v3.3.0 Basic stack. Simulated event and transport tests exercised subscription, retry, disconnect, reconnect and sample gaps. The web decoder read a packet produced by the actual C packetizer, including negative samples. The viewer logic and canvas drawing were exercised with simulated notifications. Board operation, BLE throughput, signal accuracy, and your specific CubeIDE project settings require bench verification. The viewer and firmware are for simulator-based engineering evaluation.
