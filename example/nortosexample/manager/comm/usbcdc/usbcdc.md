---
doc_role: user-manager
layer: project
module: usbcdc
status: active
portability: project-bound
public_headers:
  - usbcdc.h
depends_on:
  - User/bsp/bspusb.h
  - User/port/drvusb_port.h
  - User/system/system.h
  - User/system/systask.h
forbidden_depends_on:
  - rep/ 之外的随机业务目录
required_hooks: []
optional_hooks: []
common_utils: []
copy_minimal_set:
  - User/manager/usbcdc/usbcdc.h
  - User/manager/usbcdc/usbcdc.c
read_next:
  - usbcdc.h
  - usbcdc.c
---

# usbcdc 管理器说明

这是当前目录的权威入口文档。

## 1. 作用

`usbcdc` 目录承载当前 bootloader 工程的 USB CDC 原始收发管理。

它的职责只有三件事：

- 维护 USB CDC 链路状态。
- 从 USB 虚拟串口收原始数据。
- 向 USB 虚拟串口发送原始数据。

它不是命令解析层，也不负责升级协议状态机。

## 2. 接口契约

| 接口 | 作用 | 调用时机 | 失败语义 |
| --- | --- | --- | --- |
| `usbCdcManagerInit()` | 初始化 USB CDC 通信状态 | `drvUsbConnect()` 之后 | 返回 `false` 表示管理器未就绪 |
| `usbCdcManagerProcess()` | 轮询 USB CDC 状态并缓存最新一帧接收数据 | 周期任务或后台任务 | 静默返回，不阻塞系统主循环 |
| `usbCdcManagerWrite()` | 发送原始数据到 USB CDC | 上层需要发送数据时 | 返回 `false` 表示当前不可发送 |
| `usbCdcManagerRead()` | 读取已缓存的最新一帧接收数据 | 上层检测到有接收数据时 | 返回 `false` 表示参数错误 |
| `usbCdcManagerGetRxLength()` | 查询当前缓存的接收长度 | 上层轮询时 | 返回 `0` 表示当前无数据 |
| `usbCdcManagerReset()` | 清空内部状态 | 初始化前或异常恢复 | 不返回错误 |

## 3. 当前数据语义

当前模块只负责 USB CDC 原始字节流收发，不解释命令、不拼装协议、不生成自动回包。

## 4. 使用方式

- 在系统启动时先完成 USB CDC 初始化与连接。
- 若通过上层统一通信编排，优先由 `comm_mgr` 调用 `usbCdcManagerInit()`。
- 在主循环的周期处理里持续调用 `usbCdcManagerProcess()` 或让 `comm_mgr` 统一调度。
- 上层通过 `usbCdcManagerGetRxLength()` 和 `usbCdcManagerRead()` 读取收到的原始数据。
- 上层通过 `usbCdcManagerWrite()` 发送原始数据。

## 5. 改动边界

- 若只改当前工程的 USB 调试命令，改本目录。
- 若要统一管理 USB CDC 和 BLE 等多条通信链路，改 `User/manager/comm_mgr/`。
- 若要把命令输入复用到公共 console/log transport，优先改 `User/port` 绑定层，不要直接改本目录为公共组件。
- 若要上升为可复用协议栈，再考虑迁移到 `rep/comm/`。