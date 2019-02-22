#include "server_shared.h"
#include "sockets.h"
#include "log.h"

extern uint64_t handle_incoming_packet(const Packet& packet);
extern bool		complete_request(SOCKET client_connection, uint64_t result);

static SOCKET create_listen_socket()
{
	SOCKADDR_IN address{ };

	address.sin_family	= AF_INET;
	address.sin_port	= htons(server_port);

	const auto listen_socket = socket_listen(AF_INET, SOCK_STREAM, 0);
	if (listen_socket == INVALID_SOCKET)
	{
		log("Failed to create listen socket.");
		return INVALID_SOCKET;
	}

	if (bind(listen_socket, (SOCKADDR*)&address, sizeof(address)) == SOCKET_ERROR)
	{
		log("Failed to bind socket.");

		closesocket(listen_socket);
		return INVALID_SOCKET;
	}

	if (listen(listen_socket, 10) == SOCKET_ERROR)
	{
		log("Failed to set socket mode to listening.");

		closesocket(listen_socket);
		return INVALID_SOCKET;
	}

	return listen_socket;
}

// Connection handling thread.
static void NTAPI connection_thread(void* connection_socket)
{
	const auto client_connection = SOCKET(ULONG_PTR(connection_socket));
	log("New connection.");

	Packet packet{ };
	while (true)
	{
		const auto result = recv(client_connection, (void*)&packet, sizeof(packet), 0);
		if (result <= 0)
			break;

		if (result < sizeof(PacketHeader))
			continue;

		if (packet.header.magic != packet_magic)
			continue;

		const auto packet_result = handle_incoming_packet(packet);
		if (!complete_request(client_connection, packet_result))
			break;
	}

	log("Connection closed.");
	closesocket(client_connection);
}

// Main server thread.
void NTAPI server_thread(void*)
{
	auto status = KsInitialize();
	if (!NT_SUCCESS(status))
	{
		log("Failed to initialize KSOCKET. Status code: %X.", status);
		return;
	}

	const auto listen_socket = create_listen_socket();
	if (listen_socket == INVALID_SOCKET)
	{
		log("Failed to initialize listening socket.");

		KsDestroy();
		return;
	}

	log("Listening on port %d.", server_port);

	while (true)
	{
		sockaddr  socket_addr{ };
		socklen_t socket_length{ };

		const auto client_connection = accept(listen_socket, &socket_addr, &socket_length);
		if (client_connection == INVALID_SOCKET)
		{
			log("Failed to accept client connection.");
			break;
		}

		HANDLE thread_handle = nullptr;

		// Create a thread that will handle connection with client.
		// TODO: Limit number of threads.
		status = PsCreateSystemThread(
			&thread_handle,
			GENERIC_ALL,
			nullptr,
			nullptr,
			nullptr,
			connection_thread,
			(void*)client_connection
		);

		if (!NT_SUCCESS(status))
		{
			log("Failed to create thread for handling client connection.");

			closesocket(client_connection);
			break;
		}

		ZwClose(thread_handle);
	}

	closesocket(listen_socket);

	// Better not destroy, maybe threads handling client connection are still running.
	// TODO: Fix it
	// KsDestroy();
}