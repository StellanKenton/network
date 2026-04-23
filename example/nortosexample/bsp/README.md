# BSP 绑定层

该目录存放当前板级专用的外设适配代码，只负责把板级资源包装成可被 `User/port` 使用的薄接口，不放 `rep/` 里的可复用逻辑。

当前文件：

- `bspgpio.*`：把逻辑 GPIO 绑定到 STM32F103 的实际引脚。
- `bspmcuflash.*`：把 STM32F103 片内 Flash 控制器封装成 `drvmcuflash` 所需的板级接口，并给项目更新绑定提供薄存储包装。
- `bspuart.*`：把 USART1 + DMA1 Ch4/Ch5 封装成 `drvuart` 所需的板级 UART 接口。
- `bsprtt.*`：把 SEGGER RTT 封装成日志传输接口，供 `User/port/log_port.c` 绑定到 `rep/service/log/log`。
- `bspusb.h`、`hw_config.*` 与 `usb/*.c`、`usb/*.h`：把 STM32F103 USB FS 绑定成 CDC 设备，供 `User/port/drvusb_port.c` 和 `rep/driver/drvusb` 使用。

约束：

- 新增板级 transport、总线或芯片专用外设时，优先先补这里，再由 `User/port` 接到 `rep` 的 port 接口。
- 目录内代码只描述当前硬件资源，不直接承载业务流程。