#include "network.h"

struct sockaddr_in server_address, connection_address;
int server_socket, server_connection;

#define SERVER_PORT (9657)

bool server_is_connected = false;
bool server_is_listening = false;


bool server_close()
{
	// do not shutdown the server if it's already shutdown
	if(!server_is_connected)return;

	shutdown(server_connection, SHUT_RDWR);
	server_is_connected = false;
}

bool server_accept_connection()
{
	// start the server if it's not already running
	if(!server_is_listening)
	{
		// boilerplate code for berkerley server sockets
		server_socket = socket(AF_INET, SOCK_STREAM, 0);
		if (server_socket < 0)
		{
			printf("ERROR opening socket\n");
			server_is_listening = false;
			return false;
		}

		int one = 1;
		setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

		bzero((char *) &server_address, sizeof(server_address));

		server_address.sin_family = AF_INET;
		server_address.sin_port = htons(SERVER_PORT);
		server_address.sin_addr.s_addr = INADDR_ANY;

		if(bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
		{
			printf("ERROR on binding\n");
			server_is_listening = false;
			return false;
		}
		listen(server_socket,5);

		// update status
		server_is_listening = true;
	}

	// wait for an incoming connection
	int sizeof_connection_address = sizeof(connection_address);
	server_connection = accept(server_socket, (struct sockaddr *) &connection_address, &sizeof_connection_address);
	if (server_connection < 0)
	{
		printf("ERROR on accept\n");
		server_is_listening = false;
		server_is_connected = false;
	}

	// once accept() successfully returns, the conection has been established
	server_is_connected = true;

	return true;
}



void server_receive_bytes(int byte_count, void* buffer)
{
	if(!server_is_connected)return;

	int bytes_read = recv(server_connection, (char*)buffer, byte_count, MSG_WAITALL);
	if (bytes_read < 0)
	{
		printf("ERROR reading from socket\n");
		server_is_connected = false;
		return;
	}
	if (bytes_read == 0)
	{
		server_is_connected = false;
		return;
	}
}

void server_send_bytes(int byte_count, const void* buffer)
{
	if(!server_is_connected)return;

	int bytes_written = send(server_connection, buffer, byte_count, 0);
	if (bytes_written != byte_count)
	{
		server_is_connected = false;
		printf("ERROR writing to socket\n");
		return;
	}
}





int broadcast_socket;
struct sockaddr_in broadcast_address;
bool broadcast_initialized = false;

#define broadcast_structure(structure) broadcast(structure, sizeof(structure))


// constructs a broadcast address based on current information obtained from network interfaces
void broadcast_find_adapter_address(void)
{
	// get information about LAN interfaces

	struct ifaddrs *all_info;
	struct ifaddrs *info;
	long new_address = 0;

	if (getifaddrs(&all_info) == -1) {
		printf("broadcast's getifaddrs() failed\n");
		return;
	}

	int adapter_count = 0;
	for(info = all_info; info; info = info->ifa_next, adapter_count++) // iterate over all interfaces
	{
		if((int)info->ifa_addr->sa_family == AF_INET && 0xFF == ((struct sockaddr_in*)info->ifa_broadaddr)->sin_addr.s_addr >> 24)
		{
			new_address = ((struct sockaddr_in*)info->ifa_broadaddr)->sin_addr.s_addr;
			break;
		}
	}
	if(!info) // if no fitting interfaces found
	{
		printf("broadcast did not find an adapter for broadcasting, from %d adapters\n", adapter_count);

		// print available interfaces for debugging
		for(info = all_info; info; info = info->ifa_next)
		{
			printf("network interface: %s  broadcast: %08x\n", info->ifa_name,
				((struct sockaddr_in*)info->ifa_broadaddr)->sin_addr.s_addr);
		}
	}
	else
	{
		//new_address = inet_addr("137.226.182.24");
		if(broadcast_address.sin_addr.s_addr != new_address)
		{
			broadcast_address.sin_addr.s_addr = new_address;
			printf("broadcast address update: %d.%d.%d.%d\n", new_address&0xFF, (new_address>>8)&0xFF, (new_address>>16)&0xFF, (new_address>>24)&0xFF);
		}
	}

	freeifaddrs(all_info);
}

void broadcast_init()
{
	if(broadcast_initialized == false)
	{
		broadcast_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

		if(broadcast_socket == -1)
		{
			printf("broadcast coudn't create new socket\n");
			return;
		}

		// without this, broadcast sendto() calls will fail
		int one = 1;
		if(setsockopt(broadcast_socket, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one)) != 0)
		{
			printf("broadcast's setsockopt error\n");
		}

		/* if sending only, binding is not necessary
		struct sockaddr_in broadcast_bind_address;
		memset(&broadcast_bind_address, 0, sizeof(broadcast_bind_address));
		broadcast_bind_address.sin_family = AF_INET;
		broadcast_bind_address.sin_addr.s_addr = htons(INADDR_ANY);
		broadcast_bind_address.sin_port = htons(SERVER_PORT);
		if (bind(broadcast_socket, (struct sockaddr *) &broadcast_bind_address, sizeof(broadcast_bind_address)) < 0)
			printf("broadcast's bind() failed\n");
		*/


		broadcast_address.sin_family = AF_INET;
		broadcast_address.sin_port = (in_port_t)htons(SERVER_PORT);
		broadcast_address.sin_addr.s_addr = htonl(INADDR_BROADCAST);

		printf("broadcast initialized\n");
		broadcast_initialized = true;
	}

	broadcast_find_adapter_address();
}

int broadcast(void *data, int data_length)
{
	broadcast_init();

	if(sendto(broadcast_socket, data, data_length, 0, (struct sockaddr *)&broadcast_address, sizeof(struct sockaddr_in)) < 0)
	{
		printf("broadcast's sendto fail\n");
		broadcast_initialized = false;
	}
}

int broadcast_receive_remote_address()
{
	broadcast_init();

	char buffer[18];
	int buffer_length = 18;
	
	struct sockaddr_storage from_addr;
	int sizeof_from_addr = sizeof(from_addr);
	while(true)
	{
		int bytes_received = recvfrom(broadcast_socket, buffer, buffer_length, 0, (struct sockaddr *)&from_addr, &sizeof_from_addr);
		if(bytes_received < 0)
		{
			printf("broadcast's recvfrom fail\n");
			return 0;
		}
		if(bytes_received == 18 & memcmp(buffer, "PowerGraph request", 18) == 0)
		{
			if(from_addr.ss_family == AF_INET)
			{
				break;
			}
		}
	}
	return ((struct sockaddr_in *)&from_addr)->sin_addr.s_addr;
}
