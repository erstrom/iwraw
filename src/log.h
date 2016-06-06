/*
 * Copyright (C) 2016  Erik Stromdahl
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

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
