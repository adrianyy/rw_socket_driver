#include "log.h"

extern void NTAPI server_thread(void*);

extern "C" NTSTATUS DriverEntry(
	PDRIVER_OBJECT  driver_object,
	PUNICODE_STRING registry_path
)
{
	// These are invalid for mapped drivers.
	UNREFERENCED_PARAMETER(driver_object);
	UNREFERENCED_PARAMETER(registry_path);

	HANDLE thread_handle = nullptr;

	// Create server thread that will wait for incoming connections.
	const auto status = PsCreateSystemThread(
		&thread_handle,
		GENERIC_ALL,
		nullptr,
		nullptr,
		nullptr,
		server_thread,
		nullptr
	);

	if (!NT_SUCCESS(status))
	{
		log("Failed to create server thread. Status code: %X.", status);
		return STATUS_UNSUCCESSFUL;
	}

	ZwClose(thread_handle);
	return STATUS_SUCCESS;
}