# STM32F103RET6 Pin Map

Current project MCU: `STM32F103RET6`

## Full Pin Table

| No. | Pin | Label | Function |
| --- | --- | --- | --- |
| 2 | PC13 | Free | GPIO / TAMPER-RTC |
| 16 | PA4 | SPI-CS | GPIO OUT |
| 17 | PA5 | SPI1 clock | SPI1_SCK |
| 18 | PA6 | SPI1 MISO | SPI1_MISO |
| 19 | PA7 | SPI1 MOSI | SPI1_MOSI | GD25QXXX
| 32 | PB15 | RESET-WIFI | GPIO OUT |
| 37 | PA8 | USB_Select | GPIO OUT | 拉低则启用USB的cdc通信，拉高则启用另外一个u盘通信
| 40 | PA11 | USB device D- | USB_DM |
| 41 | PA12 | USB device D+ | USB_DP |
| 47 | PC10 | UART4 TX | UART4_TX |
| 48 | PC11 | UART4 RX | UART4_RX | WIFI UART
| 49 | PC12 | MCU-LED-SDA | GPIO OUT |
| 50 | PD2 | MCU-LED-CLK | GPIO OUT | TM1651 ANLOG I2C
| 53 | PB5 | Motor_PWM | GPIO OUT |
| 54 | PB6 | Buzzer_PWM | GPIO OUT |
| 64 | PC3 | Power-ON_Ctrl | GPIO Out |
