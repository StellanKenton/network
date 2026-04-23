# update 升级方案设计

本文档用于在 `User/manager/update` 目录下实现当前工程的双镜像升级流程。目标是把 App 侧蓝牙收包、外置 Flash 暂存、Boot 侧备份与搬运、CRC 校验、失败回滚这几段逻辑收敛成一套可直接编码的方案。

## 1. 目标

当前升级链路定义如下：

1. App 层通过蓝牙接收升级文件。
2. App 层将升级文件写入外置 Flash 的 `app2` 区。
3. App 层将 `app2` 镜像的 CRC 元数据写在 `app2` 起始位置。
4. App 层写入升级标志位，然后软件复位跳转到 Boot。
5. Boot 检测到升级标志后，先把当前 MCU 内部 Flash 的 App 区完整备份到外置 Flash 的 `app1` 区。
6. Boot 对 `app1` 备份结果做 CRC 校验，校验通过后，再把 `app2` 的新镜像搬运到 MCU 内部 Flash 的 App 区。
7. Boot 对写入后的 MCU App 区再次做 CRC 校验。
8. 校验通过则清除升级标志并跳转到 App；失败则进入错误处理或回滚分支。

这个流程的核心目标不是“能升级一次”，而是满足下面三个约束：

1. 断电后不把设备刷成不可启动状态。
2. Boot 和 App 对镜像头、标志位、CRC 算法的理解必须完全一致。
3. 升级链路必须允许定位失败点，而不是只返回一个模糊的失败结果。

## 2. 目录职责建议

建议把 `update` 目录作为升级编排层，自己不直接耦合过多底层细节，而是只负责状态推进与跨模块协调。

推荐职责拆分如下：

| 文件 | 职责 |
| --- | --- |
| `update.h` | 宏、状态枚举、对外接口、镜像头与标志位结构体定义 |
| `update.c` | App/Boot 共用的升级状态机与调度入口 |
| `flash` manager | 负责外置 GD25Q32 就绪检查 |
| `system` manager | 负责模式切换、复位、跳转 App |
| MCU Flash 驱动 | 负责内部 Flash 擦写读 |
| GD25Q32 驱动 | 负责外置 Flash 擦写读 |
| `lib_comm` / 蓝牙收包层 | 负责 OTA 指令收发和分包校验 |

如果后面升级流程继续扩展，建议在 `update` 目录内再补两个私有文件：

1. `update_storage.c`：负责外置 Flash / MCU Flash 的读写封装。
2. `update_crc.c`：负责 CRC32 增量计算和镜像校验辅助。

这样 `update.c` 只保留状态机，不会很快失控。

## 3. 外置 Flash 布局

当前 `update.h` 已经给出了外置 Flash 的关键布局宏：

| 宏 | 含义 |
| --- | --- |
| `APP_FLASH_BOOT_FLAG_ADDR` | 升级标志位存放地址 |
| `APP_FLASH_APP1_BASE_ADDR` | 备份镜像区起始地址 |
| `APP_FLASH_APP1_DATA_ADDR` | `app1` 实际镜像数据起始地址 |
| `APP_FLASH_APP2_BASE_ADDR` | 待升级镜像区起始地址 |
| `APP_FLASH_APP_CRC_RESERVED_SIZE` | 每个镜像区保留的头部空间，当前为 4 KB |

基于这些宏，建议把两个镜像区统一定义为“头部 + 数据区”的布局。

### 3.1 app1 布局

| 偏移 | 内容 |
| --- | --- |
| `0x0000` | `stUpdateImageHeader` |
| `0x0000` ~ `0x0FFF` | 保留区，至少包含镜像头、状态、预留字段 |
| `0x1000` 起 | 当前 MCU App 备份数据 |

### 3.2 app2 布局

| 偏移 | 内容 |
| --- | --- |
| `0x0000` | `stUpdateImageHeader` |
| `0x0000` ~ `0x0FFF` | 保留区，至少包含镜像头、状态、预留字段 |
| `0x1000` 起 | 通过 BLE 接收的新固件数据 |

### 3.3 为什么不要只存一个 CRC 值

只在 `app2` 起始地址放一个 CRC 值是不够的，至少还要带上：

1. `magic`，用于判断该区域是不是有效镜像头。
2. `imageSize`，用于知道实际有效镜像长度。
3. `imageCrc32`，用于整镜像校验。
4. `headerVersion`，用于以后升级头格式扩展。
5. `imageVersion`，用于决定是否允许降级。
6. `writeDone` 或状态字段，避免“数据没收完但标志位已切换”的半成品场景。

因此建议统一定义镜像头，而不是只存裸 CRC。

## 4. 推荐数据结构

建议在 `update.h` 中补齐以下结构体，Boot 和 App 共用同一份定义。

```c
typedef enum eUpdateBootFlag {
    E_UPDATE_BOOT_FLAG_IDLE = 0,
    E_UPDATE_BOOT_FLAG_APP_REQUEST,
    E_UPDATE_BOOT_FLAG_BACKUP_DONE,
    E_UPDATE_BOOT_FLAG_PROGRAM_DONE,
    E_UPDATE_BOOT_FLAG_SUCCESS,
    E_UPDATE_BOOT_FLAG_FAILED,
} eUpdateBootFlag;

typedef enum eUpdateImageState {
    E_UPDATE_IMAGE_STATE_EMPTY = 0,
    E_UPDATE_IMAGE_STATE_RECEIVING,
    E_UPDATE_IMAGE_STATE_READY,
    E_UPDATE_IMAGE_STATE_INVALID,
} eUpdateImageState;

typedef struct stUpdateImageHeader {
    uint32_t magic;
    uint32_t headerVersion;
    uint32_t imageVersion;
    uint32_t imageSize;
    uint32_t imageCrc32;
    uint32_t writeOffset;
    uint32_t imageState;
    uint32_t reserved[9];
} stUpdateImageHeader;

typedef struct stUpdateBootRecord {
    uint32_t magic;
    uint32_t requestFlag;
    uint32_t lastError;
    uint32_t app1Crc32;
    uint32_t app2Crc32;
    uint32_t appSize;
    uint32_t sequence;
    uint32_t reserved[9];
} stUpdateBootRecord;
```

建议常量：

```c
#define UPDATE_IMAGE_MAGIC              0x55504454UL
#define UPDATE_BOOT_RECORD_MAGIC        0x4254464CUL
```

这里的重点不是字段多少，而是两个原则：

1. Boot 和 App 共用同一份头定义，避免协议漂移。
2. 外置 Flash 中所有关键元数据都要可恢复、可诊断、可重复校验。

## 5. App 侧流程

App 侧升级逻辑建议只负责“接收并准备镜像”，不要在 App 侧执行真正的 MCU 内部 Flash 自刷写。这样分工更稳定。

### 5.1 App 侧状态机建议

建议细化成下面几个状态：

| 状态 | 说明 |
| --- | --- |
| `IDLE` | 空闲，未进入升级 |
| `PREPARE_APP2` | 擦除 `app2` 区和镜像头保留区 |
| `RECEIVE_INFO` | 接收 `0xEB` 文件信息 |
| `RECEIVE_OFFSET` | 接收 `0xEC` 断点偏移 |
| `RECEIVE_DATA` | 接收 `0xED` 分包数据并写入外置 Flash |
| `VERIFY_APP2` | 对 `app2` 数据做整包 CRC32 校验 |
| `SET_UPDATE_FLAG` | 写升级标志位 |
| `RESET_TO_BOOT` | 软件复位进入 Boot |
| `ERROR` | 当前升级失败 |

### 5.2 OTA 命令映射

当前工程已定义 OTA 命令：

| 命令 | 值 | 用途 |
| --- | --- | --- |
| `E_CMD_HANDSHAKE` | `0xE0` | 升级层握手命令，用于升级协议握手与能力确认 |
| `E_CMD_OTA_REQUEST` | `0xEA` | 请求 OTA 能力和协商包长 |
| `E_CMD_OTA_FILE_INFO` | `0xEB` | 下发文件大小、版本、CRC32 |
| `E_CMD_OTA_OFFSET` | `0xEC` | 查询或设置续传偏移 |
| `E_CMD_OTA_DATA` | `0xED` | 分包发送升级数据 |
| `E_CMD_OTA_RESULT` | `0xEE` | 返回升级结果 |

建议 App 侧行为：

1. 蓝牙链路建立后，先由 `wirelessMgr` 处理上位机 `0x01` 会话握手；该握手只负责 BLE 会话建立，不替代升级层握手。
2. 收到 `0xE0` 后，由升级层返回升级握手应答和当前能力信息。
3. 收到 `0xEA` 后返回当前支持的最大单包长度。
4. 收到 `0xEB` 后校验文件大小是否小于 `APP_FLASH_APP1_DATA_MAX_SIZE`，再写 `app2` 头部的固定字段。
5. 收到 `0xEC` 后根据 `stUpdateImageHeader.writeOffset` 返回可续传位置。
6. 收到 `0xED` 后先做分包 CRC16 校验，再把 payload 写入 `app2` 数据区。
7. 全部收完后做整镜像 CRC32 校验；成功则把 `imageState` 改成 `READY`。
8. 最后写 `stUpdateBootRecord.requestFlag = E_UPDATE_BOOT_FLAG_APP_REQUEST`，然后触发软件复位。

### 5.3 App 侧关键约束

1. 只有 `app2` 整镜像 CRC32 通过后，才允许写 Boot 标志。
2. 标志位必须最后写，不能先写标志再补数据。
3. 每写入一个分包后要更新 `writeOffset`，以支持断点续传。
4. 外置 Flash 写入必须按页写、按扇区擦，不能跨页乱写。
5. 进入复位前至少刷一次日志输出，方便 RTT 看到切换原因。

## 6. Boot 侧流程

Boot 侧是实际执行升级的唯一主体。建议 Boot 在 `E_SYSTEM_UPDATE_MODE` 里调用 `updateProcess()`，由 `updateProcess()` 自己推进分阶段动作。

### 6.1 Boot 侧状态机建议

| 状态 | 说明 |
| --- | --- |
| `CHECK_REQUEST` | 读取 Boot 标志和 `app2` 头部 |
| `VALIDATE_APP2` | 校验 `app2` 镜像头和整包 CRC32 |
| `BACKUP_APP_TO_APP1` | 将 MCU App 区完整备份到 `app1` |
| `VERIFY_APP1` | 校验 `app1` 备份 CRC32 |
| `ERASE_MCU_APP` | 擦除内部 Flash 的 App 区 |
| `PROGRAM_APP2_TO_MCU` | 将 `app2` 写入 MCU App 区 |
| `VERIFY_MCU_APP` | 对 MCU App 区重新计算 CRC32 |
| `CLEAR_FLAG` | 清除 Boot 标志，必要时更新结果 |
| `JUMP_TO_APP` | 跳转到主应用 |
| `ROLLBACK` | 可选，恢复 `app1` 备份 |
| `ERROR` | 停留在 Boot，等待人工处理 |

### 6.2 Boot 主流程

推荐执行顺序：

1. 读取 `stUpdateBootRecord`，判断 `magic` 和 `requestFlag` 是否有效。
2. 如果没有升级请求，则直接跳转 App。
3. 读取 `app2` 的 `stUpdateImageHeader`。
4. 校验 `app2` 头部合法性：`magic`、`imageSize`、`imageState == READY`。
5. 对 `app2` 数据区做整镜像 CRC32 校验。
6. 将当前 MCU App 区备份到 `app1` 数据区，并写入 `app1` 头部。
7. 再次对 `app1` 备份数据做 CRC32 校验。
8. 擦除 MCU App 区。
9. 将 `app2` 数据区写入 MCU App 区。
10. 对 MCU App 区做整镜像 CRC32 校验。
11. 成功则清除升级标志并跳转 App。
12. 失败则记录 `lastError`，必要时执行回滚。

## 7. CRC 方案建议

当前链路至少有两类 CRC，职责不要混淆。

| 类型 | 用途 |
| --- | --- |
| `CRC16` | BLE 单包校验，快速拦截通信错误 |
| `CRC32` | 整镜像校验，保证镜像内容一致 |

建议规则：

1. OTA 分包阶段继续使用现有 `Crc16Compute()` 处理单包。
2. 镜像级校验统一使用 CRC32。
3. App、Boot、上位机三端必须使用同一 CRC32 算法、初值和累加方式。
4. CRC32 必须只覆盖实际镜像数据，不覆盖镜像头保留区。

从测试工具现状看，PC 侧已经按“与设备端 `Bootloader_CRC32Update` 一致”的方式计算 CRC32，因此设备端应补一份统一的 CRC32 实现，避免 App、Boot 分别写两套算法。

## 8. 失败处理和回滚策略

当前流程中，真正危险的阶段是“已经擦掉 MCU App，但新镜像还没完全写成功”。因此建议把失败处理分成两级。

### 8.1 一级失败处理

适用于下面场景：

1. `app2` 头无效。
2. `app2` CRC 不匹配。
3. `app1` 备份失败。
4. `app1` CRC 不匹配。

处理建议：

1. 不改写 MCU App 区。
2. 标记 `lastError`。
3. 清除或保留升级请求标志，避免无穷重试。
4. 直接跳旧 App，或者停留在 Boot 等待调试。

### 8.2 二级失败处理

适用于下面场景：

1. MCU App 擦除成功，但新镜像写入失败。
2. MCU App 写入后 CRC 校验失败。

处理建议：

1. 如果 `app1` 备份有效，则立刻执行回滚，把 `app1` 重新写回 MCU App 区。
2. 回滚成功后，把 `requestFlag` 改成失败态并跳旧 App。
3. 回滚也失败时，Boot 不再尝试跳转，而是打印错误并停留等待人工恢复。

## 9. 标志位设计建议

不要把 Boot 标志位设计成单一字节开关，建议至少包含下面信息：

| 字段 | 用途 |
| --- | --- |
| `magic` | 防止把脏数据当有效请求 |
| `requestFlag` | 指示当前升级阶段 |
| `lastError` | 保留最近失败原因 |
| `appSize` | 记录目标镜像大小 |
| `app2Crc32` | 固定本次升级目标 CRC |
| `sequence` | 防止旧请求重复触发 |

推荐错误码：

| 值 | 含义 |
| --- | --- |
| `0` | 无错误 |
| `1` | `app2` 头无效 |
| `2` | `app2` CRC 错误 |
| `3` | `app1` 备份写入失败 |
| `4` | `app1` CRC 错误 |
| `5` | MCU App 擦除失败 |
| `6` | MCU App 写入失败 |
| `7` | MCU App CRC 错误 |
| `8` | 回滚失败 |

## 10. 实现落地建议

建议按下面顺序实现，避免一开始把状态机、通信、Flash、跳转全部混在一起。

### 10.1 第一步：补齐数据结构与布局常量

在 `update.h` 中完成：

1. `stUpdateImageHeader`
2. `stUpdateBootRecord`
3. `eUpdateBootFlag`
4. 错误码枚举
5. MCU App 起始地址、长度宏

### 10.2 第二步：先打通 App 侧收包和 `app2` 持久化

优先完成：

1. 擦 `app2` 区。
2. 写镜像头。
3. 分包写入 `app2` 数据区。
4. 断点续传偏移维护。
5. 收包完成后的整镜像 CRC32 校验。
6. 写 Boot 请求标志并软件复位。

### 10.3 第三步：实现 Boot 侧备份和搬运

优先完成：

1. 读取 Boot 标志。
2. 校验 `app2`。
3. 备份内部 App 到 `app1`。
4. 校验 `app1`。
5. 擦写内部 Flash。
6. 校验内部 Flash。
7. 清标志并跳 App。

### 10.4 第四步：最后补回滚和异常恢复

等主链路稳定后再补：

1. 断电恢复。
2. 半写入头处理。
3. 回滚。
4. 多次失败禁止重复刷写。

## 11. 推荐接口草案

如果后面要把这个方案真正写成代码，建议对外接口收敛成下面几类：

```c
bool updateInit(void);
void updateProcess(uint32_t nowTick);

bool updateStartReceive(uint32_t imageVersion, uint32_t imageSize, uint32_t imageCrc32);
bool updateWritePacket(uint32_t offset, const uint8_t *data, uint16_t length, uint16_t packetCrc16);
bool updateFinishReceive(void);

bool updateBootCheckRequest(void);
bool updateBootBackupCurrentApp(void);
bool updateBootProgramNewApp(void);
bool updateBootVerifyMcuApp(void);
bool updateBootRollback(void);
```

注意：`updateProcess()` 应只负责状态推进，底层读写动作尽量拆到私有函数里，否则后面很难测。

## 12. 与当前工程的衔接点

当前工程里已经存在几个关键事实，文档实现时要保持一致：

1. 系统模式里已经有 `E_SYSTEM_UPDATE_MODE`。
2. 当前 Boot 启动后会进入 `UPDATE_MODE` 路径。
3. 外置 Flash manager 已经用 `gd25qxxx` 驱动探测 GD25Q32。
4. OTA 命令字已经在 `lib_comm.h` 中定义为 `0xEA` 到 `0xEE`。
5. `update.h` 已经预留了 `app1`、`app2`、Boot 标志区的地址宏。

因此后续编码时，最稳妥的方式不是推翻现有结构，而是在 `update.h/update.c` 上补齐镜像头、标志位、状态机和读写封装。

## 13. 最小验收标准

实现完成后，至少满足下面这些验收点：

1. App 能把完整镜像写到 `app2`，并且 CRC32 校验通过。
2. App 仅在 `app2` 有效时才写升级标志。
3. Boot 检测到升级标志后，能先把旧 App 备份到 `app1`。
4. `app1` 备份和 MCU 新 App 都能独立做 CRC32 校验。
5. Boot 只有在最终校验成功后才清除升级标志并跳转 App。
6. 任一步失败都能记录错误原因，且不会把设备留在不可诊断状态。

## 14. 后续建议

如果你准备继续往下写代码，建议下一轮直接做下面三件事：

1. 先把 `update.h` 里的镜像头、标志位、错误码结构补齐。
2. 再把 `update.c` 拆成 App 接收流程和 Boot 执行流程两条状态机。
3. 最后补一份统一的 `CRC32` 实现，保证 PC/App/Boot 三端一致。