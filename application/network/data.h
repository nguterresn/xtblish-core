#include <stdbool.h>
#include <stdint.h>

enum network_interface {
	NET_INTF_ETH,
	NET_INTF_WIFI,
};

struct network_data {
	bool                   connected;
	enum network_interface intf;
	uint32_t               ip;
};

int network_data_get(struct network_data* output);
int network_data_set(struct network_data* input);
