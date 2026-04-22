#include "system_rtt.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "delay.h"
#include "core_cm4.h"

#include "../../rep/lib/SEGGER/SEGGER_RTT.h"

#define SYSTEM_RTT_UP_BUFFER_INDEX     0u
#define SYSTEM_RTT_DOWN_BUFFER_INDEX   0u
#define SYSTEM_RTT_LINE_BUFFER_SIZE    64u
#define SYSTEM_RTT_READ_CHUNK_SIZE     16u

static bool gSystemRttInitialized = false;
static char gSystemRttLineBuffer[SYSTEM_RTT_LINE_BUFFER_SIZE];
static uint16_t gSystemRttLineLength = 0u;
static bool gSystemRttLineOverflow = false;

static void systemRttWriteBuffer(const char *buffer, uint16_t length);
static void systemRttWriteString(const char *text);
static void systemRttWritePrompt(void);
static void systemRttResetLine(void);
static char *systemRttTrimLeft(char *text);
static void systemRttTrimRight(char *text);
static void systemRttHandleCommand(void);
static void systemRttProcessByte(uint8_t data);

static void systemRttWriteBuffer(const char *buffer, uint16_t length)
{
	if ((buffer == NULL) || (length == 0u)) {
		return;
	}

	(void)SEGGER_RTT_Write(SYSTEM_RTT_UP_BUFFER_INDEX, buffer, (unsigned)length);
}

static void systemRttWriteString(const char *text)
{
	if (text == NULL) {
		return;
	}

	systemRttWriteBuffer(text, (uint16_t)strlen(text));
}

static void systemRttWritePrompt(void)
{
	systemRttWriteString("> ");
}

static void systemRttResetLine(void)
{
	gSystemRttLineLength = 0u;
	gSystemRttLineOverflow = false;
	gSystemRttLineBuffer[0] = '\0';
}

static char *systemRttTrimLeft(char *text)
{
	while ((text != NULL) && ((*text == ' ') || (*text == '\t'))) {
		text++;
	}

	return text;
}

static void systemRttTrimRight(char *text)
{
	uint16_t length = 0u;

	if (text == NULL) {
		return;
	}

	length = (uint16_t)strlen(text);
	while (length > 0u) {
		char value = text[length - 1u];

		if ((value != ' ') && (value != '\t')) {
			break;
		}

		text[length - 1u] = '\0';
		length--;
	}
}

static void systemRttHandleCommand(void)
{
	char *command = systemRttTrimLeft(gSystemRttLineBuffer);

	systemRttTrimRight(command);
	if ((command == NULL) || (command[0] == '\0')) {
		return;
	}

	if (strcmp(command, "help") == 0) {
		systemRttWriteString("commands:\r\n");
		systemRttWriteString("  help   - show this help\r\n");
		systemRttWriteString("  ping   - reply pong\r\n");
		systemRttWriteString("  status - show RTT background status\r\n");
		systemRttWriteString("  log <text> - emit one RTT log line\r\n");
		systemRttWriteString("  reboot - software reset\r\n");
		return;
	}

	if (strcmp(command, "ping") == 0) {
		systemRttWriteString("pong\r\n");
		return;
	}

	if (strcmp(command, "status") == 0) {
		systemRttWriteString("rtt background ready\r\n");
		return;
	}

	if (strncmp(command, "log ", 4u) == 0) {
		systemRttLog("cmd", &command[4]);
		systemRttWriteString("ok\r\n");
		return;
	}

	if (strcmp(command, "reboot") == 0) {
		systemRttWriteString("rebooting...\r\n");
		delay_ms(10);
		__DSB();
		NVIC_SystemReset();
		return;
	}

	systemRttWriteString("unknown command\r\n");
}

static void systemRttProcessByte(uint8_t data)
{
	if ((data == '\r') || (data == '\n')) {
		if ((gSystemRttLineLength == 0u) && !gSystemRttLineOverflow) {
			return;
		}

		systemRttWriteString("\r\n");
		if (gSystemRttLineOverflow) {
			systemRttWriteString("error: command too long\r\n");
		} else {
			systemRttHandleCommand();
		}

		systemRttResetLine();
		systemRttWritePrompt();
		return;
	}

	if ((data == '\b') || (data == 0x7fu)) {
		if (gSystemRttLineLength == 0u) {
			return;
		}

		gSystemRttLineLength--;
		gSystemRttLineBuffer[gSystemRttLineLength] = '\0';
		systemRttWriteString("\b \b");
		return;
	}

	if (gSystemRttLineOverflow) {
		return;
	}

	if (gSystemRttLineLength >= (SYSTEM_RTT_LINE_BUFFER_SIZE - 1u)) {
		gSystemRttLineOverflow = true;
		return;
	}

	gSystemRttLineBuffer[gSystemRttLineLength] = (char)data;
	gSystemRttLineLength++;
	gSystemRttLineBuffer[gSystemRttLineLength] = '\0';
	systemRttWriteBuffer((const char *)&gSystemRttLineBuffer[gSystemRttLineLength - 1u], 1u);
}

void systemRttInit(void)
{
	if (gSystemRttInitialized) {
		return;
	}

	SEGGER_RTT_ConfigUpBuffer(SYSTEM_RTT_UP_BUFFER_INDEX,
					  "RTTUP",
					  NULL,
					  0u,
					  SEGGER_RTT_MODE_NO_BLOCK_SKIP);
	SEGGER_RTT_ConfigDownBuffer(SYSTEM_RTT_DOWN_BUFFER_INDEX,
					    "RTTDOWN",
					    NULL,
					    0u,
					    SEGGER_RTT_MODE_NO_BLOCK_SKIP);

	systemRttResetLine();
	gSystemRttInitialized = true;
	systemRttWriteString("\r\nRTT background console ready\r\n");
	systemRttWriteString("type 'help' for commands\r\n");
	systemRttWritePrompt();
}

void systemRttProcess(void)
{
	uint8_t read_buffer[SYSTEM_RTT_READ_CHUNK_SIZE];
	unsigned received = 0u;
	unsigned index = 0u;

	if (!gSystemRttInitialized) {
		systemRttInit();
	}

	for (;;) {
		received = SEGGER_RTT_Read(SYSTEM_RTT_DOWN_BUFFER_INDEX,
					   read_buffer,
					   (unsigned)sizeof(read_buffer));
		if (received == 0u) {
			break;
		}

		for (index = 0u; index < received; index++) {
			systemRttProcessByte(read_buffer[index]);
		}
	}
}

void systemRttLog(const char *tag, const char *format, ...)
{
	char message_buffer[96];
	char line_buffer[128];
	int line_length = 0;
	va_list args;

	if (format == NULL) {
		return;
	}

	if (!gSystemRttInitialized) {
		systemRttInit();
	}

	va_start(args, format);
	(void)vsnprintf(message_buffer, sizeof(message_buffer), format, args);
	va_end(args);

	line_length = snprintf(line_buffer,
				       sizeof(line_buffer),
				       "[%s] %s\r\n",
				       (tag != NULL) ? tag : "system",
				       message_buffer);
	if (line_length <= 0) {
		return;
	}

	if (line_length > (int)sizeof(line_buffer)) {
		line_length = (int)sizeof(line_buffer);
	}

	systemRttWriteBuffer(line_buffer, (uint16_t)line_length);
}
