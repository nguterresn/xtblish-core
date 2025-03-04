#include "data.h"
#include <string.h>
#include <errno.h>
#include <zephyr/kernel.h>

K_MUTEX_DEFINE(mutex);

static struct network_data data;

int network_data_get(struct network_data* output)
{
	if (!k_mutex_lock(&mutex, K_SECONDS(1))) {
		return -ENOLCK;
	}

	memcpy(output, &data, sizeof(struct network_data));
	k_mutex_unlock(&mutex);

	return 0;
}

int network_data_set(struct network_data* input)
{
	if (!k_mutex_lock(&mutex, K_SECONDS(1))) {
		return -ENOLCK;
	}

	memcpy(&data, input, sizeof(struct network_data));
	k_mutex_unlock(&mutex);

	return 0;
}
