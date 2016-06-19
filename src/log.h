#ifndef _LOG_H_
#define _LOG_H_

#include <stdarg.h>
#include <stdbool.h>
#include <syslog.h>
#include <stdio.h>

#define SYSLOG_ID "iwraw"

extern int log_level;
extern bool log_stderr;
extern bool log_initialized;

#define LOG_ERR_(fmt, ...) \
	do { \
		if ((log_level) >= (LOG_ERR)) \
			log_((log_level), (fmt), ##__VA_ARGS__); \
	} while (0)
#define LOG_WARN_(fmt, ...) \
	do { \
		if (log_level >= LOG_WARNING) \
			log_((log_level), (fmt), ##__VA_ARGS__); \
	} while (0)
#define LOG_NOTICE_(fmt, ...) \
	do { \
		if (log_level >= LOG_NOTICE) \
			log_((log_level), (fmt), ##__VA_ARGS__); \
	} while (0)
#define LOG_INFO_(fmt, ...) \
	do { \
		if (log_level >= LOG_INFO) \
			log_((log_level), (fmt), ##__VA_ARGS__); \
	} while (0)
#define LOG_DBG_(fmt, ...) \
	do { \
		if (log_level >= LOG_DEBUG) \
			log_((log_level), (fmt), ##__VA_ARGS__); \
	} while (0)

static void log_(uint32_t level, const char *fmt, ...)
{
	va_list arg_list;

	va_start(arg_list, fmt);
	if (log_stderr) {
		vfprintf(stderr, fmt, arg_list);
	} else {
		if (!log_initialized) {
			openlog(SYSLOG_ID, 0, LOG_USER);
			log_initialized = true;
		}
		vsyslog(level, fmt, arg_list);
	}
	va_end(arg_list);
}

#endif /*_LOG_H_*/
