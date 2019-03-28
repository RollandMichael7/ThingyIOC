
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include <pthread.h>

#include <sys/mman.h>

#include <dbAccess.h>
#include <dbDefs.h>
#include <dbFldTypes.h>
#include <dbScan.h>

#include <registryFunction.h>
#include <aSubRecord.h>
#include <waveformRecord.h>
#include <epicsExport.h>
#include <epicsTime.h>
#include <callback.h>

#include <glib.h>
#include "gattlib.h"


gatt_connection_t *gatt_connection = 0;
//static EVENTPVT event;

#define TEMP_UUID "0201"
#define HUMIDITY_UUID "0203"
#define BUTTON_UUID "0302"
#define ORIENTATION_UUID "0403"

typedef struct {
	char uuid_str[35];
	aSubRecord *pv;
	uuid_t uuid;
} NotifyArgs;

static gatt_connection_t *get_connection() {
	if (gatt_connection != 0)
		return gatt_connection;
	gatt_connection = gattlib_connect(NULL, "D3:69:D6:BA:E3:31", BDADDR_LE_PUBLIC, BT_SEC_LOW, 0, 0);
	return gatt_connection;
}

static void writePV_callback(const uuid_t *uuid, const uint8_t *data, size_t len, void *user_data) {
	NotifyArgs *args = (NotifyArgs *) user_data;
	aSubRecord *pv = args->pv;
	if (strcmp(args->uuid_str, ORIENTATION_UUID) == 0) {
		int n=data[0];
		if (data == 0)
			strcpy(pv->vala, "Portrait");
		else if (data == 1)
			strcpy(pv->vala, "Landscape");
		else if (data == 2)
			strcpy(pv->vala, "Reverse Portrait");
		else if (data == 3)
			strcpy(pv->vala, "Reverse Landscape");
		else
			strcpy(pv->vala, "UNKNOWN");
	}
	else {
		float x=0;
		for (int i=0; i < len; i++) {
			if (i == 0)
				x += (float) (data[i]);
			else
				x += (float) (data[i]/100.0);
		}
		memcpy(pv->vala, &x, sizeof(float));
	}
	//postEvent(event);
}

static uint128_t str_to_128t(const char *string) {
	uint32_t data0, data4;
	uint16_t data1, data2, data3, data5;
	uint128_t u128;
	uint8_t *val = (uint8_t *) &u128;

	if(sscanf(string, "%08x-%04hx-%04hx-%04hx-%08x%04hx",
				&data0, &data1, &data2,
				&data3, &data4, &data5) != 6)
		printf("scan fail\n");

	memcpy(&val[0], &data0, 4);
	memcpy(&val[4], &data1, 2);
	memcpy(&val[6], &data2, 2);
	memcpy(&val[8], &data3, 2);
	memcpy(&val[10], &data4, 4);
	memcpy(&val[14], &data5, 2);

	return u128;
}

// construct a 128 bit UUID for a Nordic thingy:52
// given its unique UUID component
static uuid_t thingyUUID(const char *id) {
	char buf[35];
	strcpy(buf, "EF68");
	strcat(buf, id);
	strcat(buf, "-9B35-4933-9B10-52FFA9740042");
	//printf("%s\n", buf);
	uint128_t uuid_val = str_to_128t(buf);
	uuid_t uuid = {.type=SDP_UUID128, .value.uuid128=uuid_val};
	return uuid;
}

static void *notificationListener(void *vargp) {
	NotifyArgs *args = (NotifyArgs *) vargp;
	gatt_connection_t *conn = get_connection();
	gattlib_register_notification(conn, writePV_callback, args);
	if (gattlib_notification_start(conn, &(args->uuid))) {
		printf("Failed to start notifications for UUID\n");
	}

	GMainLoop *loop = g_main_loop_new(NULL, 0);
	g_main_loop_run(loop);
}

static long subscribeUUID(aSubRecord *paSub) {
	char input[35];
	strcpy(input, paSub->a);
	strcat(input, paSub->b);
	uuid_t uuid = thingyUUID(input);

	NotifyArgs *args = malloc(sizeof(NotifyArgs));
	args->uuid = uuid;
	args->pv = paSub;
	strcpy(args->uuid_str, input);
	pthread_t thread_id;
	pthread_create(&thread_id, NULL, &notificationListener, (void *)args);
	paSub->pact = FALSE;
	return 0;
}

static long readUUID(aSubRecord *paSub) {
	int i, ret;
	char input[35];
	strcpy(input, paSub->a);
	strcat(input, paSub->b);
	gatt_connection_t *conn = get_connection();
	uuid_t uuid = thingyUUID(input);
	
	char byte[4];
	char uuid_buf[100];
	char out_buf[100];
	memset(out_buf, 0, sizeof(out_buf));
	size_t len = sizeof(uuid_buf);
	if (gattlib_read_char_by_uuid(conn, &uuid, uuid_buf, &len) == -1) {
		printf("Read of uuid %s failed.\n", input);
		return 1;
	}
	else {
		for (i=0; i < len; i++) {
			snprintf(byte, sizeof(byte), "%02x ", uuid_buf[i]);
			strcat(out_buf, byte);
		}
		//printf("%s\n", out_buf);
		memcpy(paSub->vala, out_buf, strlen(out_buf));
	}
	return 0;
}

/* Register these symbols for use by IOC code: */
epicsRegisterFunction(subscribeUUID);
epicsRegisterFunction(readUUID);
