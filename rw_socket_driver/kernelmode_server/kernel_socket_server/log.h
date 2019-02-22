#pragma once
#include <ntddk.h>

template <typename... Args>
void log(const char* format, Args... args)
{
	DbgPrintEx(0, 0, "[socksrv] ");
	DbgPrintEx(0, 0, format, args...);
	DbgPrintEx(0, 0, "\n");
}