/*
**	Command & Conquer Renegade(tm)
**	Copyright 2026 The OpenW3D Team.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "coopdebuglog.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace
{
	static bool _did_reset = false;

	static unsigned long Get_Process_Id(void)
	{
#if defined(_WIN32)
		return (unsigned long)GetCurrentProcessId();
#else
		return (unsigned long)getpid();
#endif
	}

	static unsigned long Get_Thread_Id(void)
	{
#if defined(_WIN32)
		return (unsigned long)GetCurrentThreadId();
#else
		return 0;
#endif
	}

	static const char *Get_Temp_Directory(void)
	{
		const char *temp_path = getenv("TEMP");
		if (temp_path == NULL || temp_path[0] == 0) {
			temp_path = getenv("TMP");
		}
		if (temp_path == NULL || temp_path[0] == 0) {
			temp_path = "/tmp";
		}
		return temp_path;
	}

	static void Build_Temp_Log_Path(char *buffer, size_t buffer_size, bool per_process)
	{
		const char *temp_path = Get_Temp_Directory();
		const char *separator = "";
		size_t length = strlen(temp_path);
		if (length > 0 && temp_path[length - 1] != '\\' && temp_path[length - 1] != '/') {
			separator = "\\";
		}

		if (per_process) {
			snprintf(buffer, buffer_size, "%s%sopenw3d_coop_runtime_%lu.log", temp_path, separator, Get_Process_Id());
		} else {
			snprintf(buffer, buffer_size, "%s%sopenw3d_coop_runtime.log", temp_path, separator);
		}
		buffer[buffer_size - 1] = 0;
	}

	static void Write_To_File(const char *filename, const char *mode, const char *line)
	{
		FILE *file = fopen(filename, mode);
		if (file != NULL) {
			fputs(line, file);
			fflush(file);
			fclose(file);
		}
	}

	static void Write_All(const char *mode, const char *line)
	{
		Write_To_File("coop_runtime.log", mode, line);

		char temp_log_path[512];
		Build_Temp_Log_Path(temp_log_path, sizeof(temp_log_path), false);
		Write_To_File(temp_log_path, mode, line);

		Build_Temp_Log_Path(temp_log_path, sizeof(temp_log_path), true);
		Write_To_File(temp_log_path, mode, line);
	}

	static void Build_Time_Prefix(char *buffer, size_t buffer_size)
	{
		time_t now = time(NULL);
		struct tm local_time;
#if defined(_WIN32)
		localtime_s(&local_time, &now);
#else
		localtime_r(&now, &local_time);
#endif

		char time_text[32];
		strftime(time_text, sizeof(time_text), "%Y-%m-%d %H:%M:%S", &local_time);
		snprintf(buffer, buffer_size, "[%s pid=%lu tid=%lu] ", time_text, Get_Process_Id(), Get_Thread_Id());
		buffer[buffer_size - 1] = 0;
	}

	static void Ensure_Reset(void)
	{
		if (!_did_reset) {
			CoopDebugLog::Reset();
		}
	}
}

void CoopDebugLog::Reset(void)
{
	_did_reset = true;

	char line[256];
	char prefix[96];
	Build_Time_Prefix(prefix, sizeof(prefix));
	snprintf(line, sizeof(line), "\n===== %sco-op runtime log start =====\n", prefix);
	line[sizeof(line) - 1] = 0;

	Write_All("a", line);
}

void CoopDebugLog::Log(const char *format, ...)
{
	Ensure_Reset();

	char message[2048];
	va_list args;
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);
	message[sizeof(message) - 1] = 0;

	char prefix[96];
	Build_Time_Prefix(prefix, sizeof(prefix));

	char line[2304];
	snprintf(line, sizeof(line), "%s%s\n", prefix, message);
	line[sizeof(line) - 1] = 0;

	Write_All("a", line);
}
