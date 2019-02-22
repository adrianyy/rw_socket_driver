#pragma once
#include <ntddk.h>

extern "C"
{
	NTKERNELAPI NTSTATUS MmCopyVirtualMemory(
		IN PEPROCESS		SourceProcess,
		IN PVOID			SourceAddress,
		IN PEPROCESS		TargetProcess,
		IN PVOID			TargetAddress,
		IN SIZE_T			BufferSize,
		IN KPROCESSOR_MODE  PreviousMode,
		OUT PSIZE_T			ReturnSize
	);

	NTKERNELAPI NTSTATUS PsLookupProcessByProcessId(
		IN HANDLE			ProcessId,
		OUT PEPROCESS*		Process
	);

	NTKERNELAPI PVOID PsGetProcessSectionBaseAddress(
		IN PEPROCESS		Process
	);
}