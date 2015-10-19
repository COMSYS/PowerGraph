#ifndef _NETWORK_
#define _NETWORK_

#include "common.h"

#define SERVER_PORT (9657)

extern bool server_is_connected;
extern bool server_is_listening;


// stop receiving TCP connections
bool server_close();

// waits for an incoming TCP connection
bool server_accept_connection();


// wait for this many bytes to arrive on the currect TCP conenction
void server_receive_bytes(int byte_count, void* buffer);

// send data on the currently open TCP connection
void server_send_bytes(int byte_count, const void* buffer);


// initialize the broadcast sockets, must be called before calling broadcast()
void broadcast_init();

// emits a UDP broadcast packet into the network
int broadcast(void *data, int data_length);

#endif