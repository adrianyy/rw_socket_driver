#include "driver.h"
#include <iostream>

void example()
{
	const auto connection = driver::connect();
	if (connection == INVALID_SOCKET)
	{
		std::cout << "Connection failed.\n";
		return;
	}

	const auto pid = 5540;

	const auto base_address = driver::get_process_base_address(connection, pid);
	std::printf("Process base address: %p.\n", (void*)base_address);

	const auto dos_magic = driver::read<uint16_t>(connection, pid, base_address);
	std::printf("DOS signature: %X.\n", dos_magic);

	driver::disconnect(connection);
}

int main()
{
	driver::initialize();

	example();

	driver::deinitialize();
	std::cin.get();
}
