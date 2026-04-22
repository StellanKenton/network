#include "log.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#if LOG_USE_RTT
#include "SEGGER_RTT.h"
#endif

#if LOG_USE_UART
#include "usart.h"
#endif
#if LOG_USE_BACK_FIRED
#include "cm_backtrace.h"
#endif

// 日志缓冲区大小，根据最大单条日志长度调整
#define LOG_BUF_SIZE 256

static char log_buf[LOG_BUF_SIZE];
static uint32_t Log_TimeStamp=0;
static LogFun_Def LogFunList[10];

void Log_LoopFun(char *data);

// 串口发送函数
int Log_UART_Transmit(uint8_t *data, uint16_t len) {
#if LOG_USE_UART
    // 使用阻塞方式发送，确保日志完整性
    // 超时时间设为 100ms，避免卡死
    if (HAL_UART_Transmit(&huart3, data, len, 100) != HAL_OK) {
        return -1;
    }
#endif
    return 0; 
}

void Log_Init(void) {
#if LOG_USE_RTT
    SEGGER_RTT_ConfigUpBuffer(0, "RTTUP", NULL, 0, SEGGER_RTT_MODE_NO_BLOCK_SKIP);
    SEGGER_RTT_ConfigDownBuffer(0,"RTTDOWN",NULL, 0, SEGGER_RTT_MODE_NO_BLOCK_SKIP);
#endif
    LOG_I("=================System Power Up ===================");
#if LOG_USE_BACK_FIRED
    LOG_I("cm_backtrace initialized");
    cm_backtrace_init(FIRMWARE_NAME, FIRMWARE_VERSION, HARDWARE_VERSION);
#endif
    LOG_I("Log System Initialized");
    Log_RegisterFunction("loop", Log_LoopFun);
}

static void Log_Output(const char *str, uint16_t len) {
#if LOG_USE_RTT
    SEGGER_RTT_Write(0, str, len);
#endif

#if LOG_USE_UART
    Log_UART_Transmit((uint8_t *)str, len);
#endif
}

void Log_Printf(uint8_t level, const char *file, int line, const char *fmt, ...) {
    if (level > LOG_GLOBAL_LEVEL) return;

    va_list args;
    int offset = 0;

    const char *tag = "UNK";
#if LOG_ENABLE_COLOR
    const char *color_start = LOG_CLR_NORMAL;
    // 1. 确定标签和颜色
    switch (level) {
        case LOG_LEVEL_ERROR: tag = "ERR"; color_start = LOG_CLR_RED; break;
        case LOG_LEVEL_WARN:  tag = "WRN"; color_start = LOG_CLR_YELLOW; break;
        case LOG_LEVEL_INFO:  tag = "INF"; color_start = LOG_CLR_GREEN; break;
        case LOG_LEVEL_DEBUG: tag = "DBG"; color_start = LOG_CLR_CYAN; break;
        default: break;
    }
    // 2. 格式化前缀
    // 颜色开始
    offset += snprintf(log_buf + offset, LOG_BUF_SIZE - offset, "%s", color_start);
#else
    switch (level) {
        case LOG_LEVEL_ERROR: tag = "ERR"; break;
        case LOG_LEVEL_WARN:  tag = "WRN"; break;
        case LOG_LEVEL_INFO:  tag = "INF"; break;
        case LOG_LEVEL_DEBUG: tag = "DBG"; break;
        default: break;
    }
#endif

    // 时间戳
#if LOG_ENABLE_TIMESTAMP
    //offset += snprintf(log_buf + offset, LOG_BUF_SIZE - offset, "[%08lu] ", (unsigned long)Log_TimeStamp);
    offset += snprintf(log_buf + offset, LOG_BUF_SIZE - offset, "[%08lu] ", (unsigned long)HAL_GetTick());
#endif

    // 标签
    offset += snprintf(log_buf + offset, LOG_BUF_SIZE - offset, "[%s] ", tag);

    // 文件信息 (可选)
#if LOG_ENABLE_FILE_INFO
    // 只显示文件名，去掉路径
    const char *filename = strrchr(file, '/');
    if (filename) filename++; else filename = strrchr(file, '\\');
    if (filename) filename++; else filename = file;
    
    offset += snprintf(log_buf + offset, LOG_BUF_SIZE - offset, "(%s:%d) ", filename, line);
#endif

    // 3. 格式化用户内容
    if (offset < LOG_BUF_SIZE) {
        va_start(args, fmt);
        offset += vsnprintf(log_buf + offset, LOG_BUF_SIZE - offset, fmt, args);
        va_end(args);
    }

    // 4. 结尾处理 (颜色复位 + 换行)
#if LOG_ENABLE_COLOR
    if (offset < LOG_BUF_SIZE - 6) { // 预留空间
        offset += snprintf(log_buf + offset, LOG_BUF_SIZE - offset, "%s\r\n", LOG_CLR_NORMAL);
    }
#else
    if (offset < LOG_BUF_SIZE - 2) {
        offset += snprintf(log_buf + offset, LOG_BUF_SIZE - offset, "\r\n");
    }
#endif

    // 5. 输出
    Log_Output(log_buf, offset);
}

// 十六进制打印工具
void Log_Hex(uint8_t level, const char *tag, const void *data, uint16_t len) {
    if (level > LOG_GLOBAL_LEVEL) return;
    
    const uint8_t *p = (const uint8_t *)data;
    char hex_buf[64]; // 临时行缓冲
    
    // 打印头部
    Log_Printf(level, "", 0, "--- %s Hex Dump (%d bytes) ---", tag, len);
    
    for (uint16_t i = 0; i < len; i += 16) {
        int n = 0;
        // 打印偏移量
        n += snprintf(hex_buf + n, sizeof(hex_buf) - n, "%04X: ", i);
        
        // 打印16进制
        for (int j = 0; j < 16; j++) {
            if (i + j < len) {
                n += snprintf(hex_buf + n, sizeof(hex_buf) - n, "%02X ", p[i + j]);
            } else {
                n += snprintf(hex_buf + n, sizeof(hex_buf) - n, "   ");
            }
        }
        
        // 打印ASCII
        n += snprintf(hex_buf + n, sizeof(hex_buf) - n, " |");
        for (int j = 0; j < 16; j++) {
            if (i + j < len) {
                char c = p[i + j];
                n += snprintf(hex_buf + n, sizeof(hex_buf) - n, "%c", (c >= 32 && c <= 126) ? c : '.');
            }
        }
        n += snprintf(hex_buf + n, sizeof(hex_buf) - n, "|");
        
        // 直接输出这一行，不带前缀，保持整洁
        Log_Printf(level, "", 0, "%s", hex_buf);
    }
}

void Log_Process(uint8_t taskTick) {
    static char LogCmd[256];
    static char KeyBuf[16];
    int GetKey;
    Log_TimeStamp += taskTick;
    if (SEGGER_RTT_HasKey()) 
    {
        memset(LogCmd, 0, sizeof(LogCmd));
        uint32_t idx = 0;
        while (SEGGER_RTT_HasKey()) 
        {
            GetKey = SEGGER_RTT_GetKey();
            if (GetKey == '\r' || GetKey == '\n' || idx >= sizeof(LogCmd) - 1) 
            {
                break;
            }
            LogCmd[idx++] = (char)GetKey;
        }
        // 执行注册的函数
        uint8_t keyIndex = 0;
        bool found = true;
        while(LogCmd[keyIndex] != ' ') {
            KeyBuf[keyIndex] = LogCmd[keyIndex];
            keyIndex++;
            if (keyIndex >= sizeof(KeyBuf) - 1) {
                found = false;
                break;
            }
        }
        if(found) {
            KeyBuf[keyIndex] = '\0';
            for (uint8_t i = 0; i < 10; i++) {
                if (LogFunList[i].isUsed && (strcmp(LogFunList[i].Fun_Name, KeyBuf) == 0)) {
                    LogFunList[i].Fun_Def(&LogCmd[keyIndex+1]);
                    break;
                }
            }
        }

    }
}


bool Log_RegisterFunction(const char *name, void (*func)(char *)) {
    static uint8_t UsedInfex = 0;
    if (LogFunList[UsedInfex].isUsed == false && UsedInfex < 10
        && name != NULL && func != NULL){
        LogFunList[UsedInfex].isUsed = true;
        strncpy(LogFunList[UsedInfex].Fun_Name, name, sizeof(LogFunList[UsedInfex].Fun_Name) - 1);
        LogFunList[UsedInfex].Fun_Def = func;
        UsedInfex++;
        return true;
    }else {
        return false;
    }
}

void Log_LoopFun(char *data) {
    LOG_I("Log_LoopFun executed: %s\r\n", data);
}


/**************************End of file********************************/
