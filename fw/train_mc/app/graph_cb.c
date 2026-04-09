#include "graph.h"
#include "platform.h"
#include <string.h>

#define MIN(a, b) ((a) <= (b) ? (a) : (b))

static uint8_t read_data[240] __attribute__((section(".ccmram")));
static uint32_t read_data_sz = 0;

void gr_wr_cb(const void *buf, size_t count)
{
	read_data_sz = sizeof(read_data);
	gr_cb(buf, count, read_data, &read_data_sz);
}

int gr_rd_cb(void *buf, size_t *sz)
{
	if(*sz < read_data_sz)
	{
		*sz = 0;
		return -1;
	}
	memcpy(buf, read_data, read_data_sz);
	*sz = read_data_sz;
	return 0;
}