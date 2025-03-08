#pragma once
#include <zephyr/net/net_ip.h>

int socket_connect(int* sock, int type, int proto, struct sockaddr_in* addr,
                   int port);
