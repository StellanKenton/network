#ifndef __LOG_H__
#define __LOG_H__

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

/* ================= 配置区域 ================= */

// 1. 输出后端配置 (1: 启用, 0: 禁用)
#define LOG_USE_BACK_FIRED      1   // 1: 使用CMBackFire

#define LOG_USE_RTT             1   // 使用 SEGGER RTT 输出
#define LOG_USE_UART            0   // 使用 串口 输出

// 2. 功能配置
#define LOG_ENABLE_COLOR        0   // 启用彩色日志 (建议 RTT 开启，普通串口助手可能显示乱码)
#define LOG_ENABLE_TIMESTAMP    1   // 启用时间戳
#define LOG_ENABLE_FILE_INFO    0   // 启用文件名和行号显示 (调试时很有用，但会增加Flash占用)

// 3. 日志等级定义
#define LOG_LEVEL_OFF           0
#define LOG_LEVEL_ERROR         1
#define LOG_LEVEL_WARN          2
#define LOG_LEVEL_INFO          3
#define LOG_LEVEL_DEBUG         4

// 4. 当前全局日志等级
#ifndef LOG_GLOBAL_LEVEL
#define LOG_GLOBAL_LEVEL        LOG_LEVEL_DEBUG     // 当前日志等级
#endif

/* =========================================== */

// 颜色代码
#if LOG_ENABLE_COLOR
    #define LOG_CLR_NORMAL      "\033[0m"
    #define LOG_CLR_RED         "\033[31m"
    #define LOG_CLR_GREEN       "\033[32m"
    #define LOG_CLR_YELLOW      "\033[33m"
    #define LOG_CLR_BLUE        "\033[34m"
    #define LOG_CLR_MAGENTA     "\033[35m"
    #define LOG_CLR_CYAN        "\033[36m"
#else
    #define LOG_CLR_NORMAL      ""
    #define LOG_CLR_RED         ""
    #define LOG_CLR_GREEN       ""
    #define LOG_CLR_YELLOW      ""
    #define LOG_CLR_BLUE        ""
    #define LOG_CLR_MAGENTA     ""
    #define LOG_CLR_CYAN        ""
#endif

// 核心函数声明
void Log_Init(void);
void Log_Printf(uint8_t level, const char *file, int line, const char *fmt, ...);
void Log_Hex(uint8_t level, const char *tag, const void *data, uint16_t len);
void Log_Process(uint8_t taskTick);
// 串口发送接口 (需要在外部实现，例如在 main.c 或 usart.c 中)
// 返回值: 1 成功, 0 失败
int Log_UART_Transmit(uint8_t *data, uint16_t len);
bool Log_RegisterFunction(const char *name, void (*func)(char *));

typedef struct 
{
    bool    isUsed;
    char     Fun_Name[15];
    void   (*Fun_Def)(char *);
}LogFun_Def;


// 宏定义封装
#if (LOG_GLOBAL_LEVEL >= LOG_LEVEL_ERROR)
    #define LOG_E(fmt, ...) Log_Printf(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
    #define LOG_E(fmt, ...)
#endif

#if (LOG_GLOBAL_LEVEL >= LOG_LEVEL_WARN)
    #define LOG_W(fmt, ...) Log_Printf(LOG_LEVEL_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
    #define LOG_W(fmt, ...)
#endif

#if (LOG_GLOBAL_LEVEL >= LOG_LEVEL_INFO)
    #define LOG_I(fmt, ...) Log_Printf(LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
    #define LOG_I(fmt, ...)
#endif

#if (LOG_GLOBAL_LEVEL >= LOG_LEVEL_DEBUG)
    #define LOG_D(fmt, ...) Log_Printf(LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
    #define LOG_D(fmt, ...)
#endif

// Hex Dump 宏
#define LOG_HEX_D(tag, data, len) Log_Hex(LOG_LEVEL_DEBUG, tag, data, len)
#define LOG_HEX_I(tag, data, len) Log_Hex(LOG_LEVEL_INFO, tag, data, len)

#endif // __LOG_H__

/**************************End of file********************************/
