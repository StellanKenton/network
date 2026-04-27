---
doc_role: layer-guide
layer: User/bsp
module: bsp
status: active
---

# User BSP

`User/bsp/` 保存当前工程的板级外设适配代码，负责把 MCU 寄存器、官方中间件和板级引脚细节收敛在项目边界内。

## 子目录

- `usbhost/`：基于 STM32 USB Host-Device Library V2.1.0 的 USB OTG FS Host 适配层，用于 EC800M-CN USB AT 通信。

## 约束

- 上层业务和 `rep/driver` 不直接包含官方 USB Host 头文件。
- USB Host 轮询、枚举、端点选择和 VBUS/IRQ 细节保留在 `bspusb.*` 与 `usbhost/` 内。
- 日志使用 `LOG_I`、`LOG_W`、`LOG_E`。