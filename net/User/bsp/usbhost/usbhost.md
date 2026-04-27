---
doc_role: module-guide
layer: User/bsp
module: usbhost
status: active
---

# usbhost

本目录是 EC800M-CN USB Host 通信的项目适配层。

## 文件职责

- `official/`：从 STM32 USB Host-Device Library V2.1.0 拷贝的最小 Host Core/HCD 源码集合。
- `inc/usb_conf.h`：当前工程 USB OTG FS Host 配置。
- `inc/usbh_conf.h`：当前工程 USB Host Core 容量配置。
- `inc/usbh_cdc_host.h` / `src/usbh_cdc_host.c`：项目补充的 CDC/ACM Host class，负责选择 EC800M AT bulk in/out 端点。
- `inc/usbhost_ec800.h` / `src/usbhost_ec800.c`：把官方 Host Core、CDC class 和 `drvusb` BSP hook 连接起来。

## 依赖与边界

- 允许依赖 STM32F4 标准外设库、官方 USB OTG/Host Core、`repRtosGetTickMs` 和 `LOG_*`。
- 不向 `rep/driver/drvusb` 泄漏官方 USB 类型。
- EC800M 是 USB device，MCU 使用 OTG FS Host，PA11/PA12 为 DM/DP；当前 pinmap 未登记 VBUS sense 或 VBUS power switch 引脚，PA9/PA10 保留为 5G UART。

## 验证

修改后至少执行 Keil Build；影响硬件通信时继续执行 J-Link Flash 和 RTT `iot cell status` / `iot cell info` 验证。