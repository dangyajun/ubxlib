# Instance Matrix
The table below defines the instances of test hardware available on the `ubxlib` test farm and how each one is currently configured.  It is parsed and used by the functions in `u_data.py` to know what to do so please always ensure that it is a properly formatted Markdown format table.

The first digit of the instance ID represents either a physical thing: a board connected via a COM port/debugger, or a distinct process (e.g. run Lint).  The remaining digits represent variants, e.g. using different SDKs on a board.

The platform field must match a unique platform name from under one of the `platform/<vendor>/<chipset>` directories (e.g `esp32`), the SDK must match an `sdk` of that platform, each API must be the name of one in the directory structure (e.g. `port` for the porting API), module names must be used consistently as must the second\[/third\] digit(s) of the instance ID (e.g. for NRF52 variant 0 is always the GCC build variant, variant 1 is always the SES build variant); case is ignored.

|  ID   | Description                                | Platform  |   SDK   | Module(s) (separate with spaces) |  APIs available (separate with spaces)      | #defines required (separate with spaces) |
| :---: | ------------------------------------------ | :-------: | :-----: | :------------------------------: | ------------------------------------------- | ---------------------------------------- |
| 0     | Run Lint                                   |           |         |                                  |                                             |                                          |
| 1     | Run Doxygen                                |           |         |                                  |                                             |                                          |
| 2     | Run AStyle style checker (for advice only) |           |         |                                  |                                             |                                          |
| 3     | Run Pylint                                 |           |         |                                  |                                             |                                          |
| 4     | Reserved                                   |           |         |                                  |                                             |                                          |
| 5     | WHRE board (NINA-W1)                       | ESP32     | ESP-IDF | SARA-R412M-03B                   | port                                        | U_CFG_TEST_PIN_A=-1 U_CFG_TEST_PIN_B=-1 U_CFG_TEST_PIN_C=-1 U_CFG_TEST_PIN_UART_TXD=-1 U_CFG_TEST_PIN_UART_RXD=-1 |
| 6     | ESP32-DevKitC + EVK                        | ESP32     | ESP-IDF | SARA-R412M-02B                   | port                                        | U_CFG_APP_PIN_CELLULAR_RXD=19 U_CFG_APP_PIN_CELLULAR_TXD=21 U_CFG_APP_PIN_CELLULAR_RTS=22 U_CFG_APP_PIN_CELLULAR_CTS=23 U_CFG_APP_PIN_CELLULAR_VINT=-1 U_CFG_APP_PIN_CELLULAR_ENABLE_POWER=-1 |
| 7     | ESP32-DevKitC + EVK                        | ESP32     | ESP-IDF | SARA-R5                          | port                                        | U_CFG_APP_PIN_CELLULAR_RXD=19 U_CFG_APP_PIN_CELLULAR_TXD=21 U_CFG_APP_PIN_CELLULAR_VINT=-1 U_CFG_APP_PIN_CELLULAR_ENABLE_POWER=-1 |
| 8.0   | Nordic DK board (NRF52840) + EVK           | NRF52     |   GCC   | SARA-R5                          | port                                        |                                          |
| 8.1   | Nordic DK board (NRF52840) + EVK           | NRF52     |   SES   | SARA-R5                          | port                                        |                                          |
| 9     | C030-R412M board (STM32F437), GPRS         | STM32F4   |  Cube   | SARA-R412M-02B                   | port                                        |                                          |
| 10    | C030-R412M board (STM32F437), Cat-M1       | STM32F4   |  Cube   | SARA-R412M-02B                   | port                                        |                                          |
| 11    | STM32F4 Discovery board (ST32F407) + EVK   | STM32F4   |  Cube   |                                  | port                                        | HSE_VALUE=((uint32_t)8000000U)           |
| 12.0  | Nordic DK board (NRF52840) + EVK           | NRF52     |   GCC   |                                  | port                                        |                                          |
| 12.1  | Nordic DK board (NRF52840) + EVK           | NRF52     |   SES   |                                  | port                                        |                                          |
| 13.0  | C208 board (NINA-B3), Cat-M1               | NRF52     |   GCC   | SARA-R412M-02B                   | port                                        | U_CFG_TEST_PIN_A=-1 U_CFG_TEST_PIN_B=-1 U_CFG_TEST_PIN_C=-1 U_CFG_TEST_PIN_UART_TXD=-1 U_CFG_TEST_PIN_UART_RXD=-1 |
| 13.1  | C208 board (NINA-B3), Cat-M1               | NRF52     |   SES   | SARA-R412M-02B                   | port                                        | U_CFG_TEST_PIN_A=-1 U_CFG_TEST_PIN_B=-1 U_CFG_TEST_PIN_C=-1 U_CFG_TEST_PIN_UART_TXD=-1 U_CFG_TEST_PIN_UART_RXD=-1 |
| 14    | C030-U201 board (STM32F437), 3G            | STM32F4   |  Cube   | SARA-U201                        | port                                        |                                          |

Note: don't change existing instance IDs without checking the rest of the automation system for scripts which "know" about their numbers (e.g. the `esp32` ones do).