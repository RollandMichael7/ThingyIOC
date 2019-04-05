
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include <pthread.h>
#include <inttypes.h>
#include <signal.h>

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


#define MAC_ADDRESS "D3:69:D6:BA:E3:31"

gatt_connection_t *gatt_connection = 0;
pthread_mutex_t connlock = PTHREAD_MUTEX_INITIALIZER;

#define TEMP_UUID "0201"
#define PRESSURE_UUID "0202"
#define HUMIDITY_UUID "0203"
#define AIRQUAL_UUID "0204"
#define LED_UUID "0301"
#define BUTTON_UUID "0302"
#define ORIENTATION_UUID "0403"
#define EULER_UUID "0407"
#define ROTATION_UUID "0408"
#define HEADING_UUID "0409"
#define GRAVITY_UUID "040A"
#define BATTERY_UUID "180F"

#define LED_OFF 0
#define LED_CONSTANT 1
#define LED_BREATHE 2
#define LED_ONCE 3

static char *led_colors[7] = {"Red", "Green", "Yellow", "Blue", "Purple", "Cyan", "White"};

static void disconnect();

// data for notification threads
typedef struct {
	char uuid_str[35];
	aSubRecord *pv;
	uuid_t uuid;
} NotifyArgs;

// linked list of subscribed UUIDs for cleanup
typedef struct {
	uuid_t *uuid;
	struct NotificationNode *next;
} NotificationNode;

NotificationNode *firstNode = 0;

// TODO: protect connection with lock without making everything hang
static gatt_connection_t *get_connection() {
	//pthread_mutex_lock(&connlock);
	if (gatt_connection != 0) {
		//pthread_mutex_unlock(&connlock);
		return gatt_connection;
	}
	gatt_connection = gattlib_connect(NULL, MAC_ADDRESS, BDADDR_LE_PUBLIC, BT_SEC_LOW, 0, 0);
	signal(SIGINT, disconnect);
	//pthread_mutex_unlock(&connlock);
	return gatt_connection;
}

static void disconnect() {
	gatt_connection_t *conn = gatt_connection;
	printf("Stopping notifications...\n");
	if (firstNode != 0) {
		NotificationNode *curr = firstNode; 
		NotificationNode *next;
		while (curr->next != 0) {
			next = curr->next;
			gattlib_notification_stop(conn, curr->uuid);
			free(curr);
			curr = next;
		}
		gattlib_notification_stop(conn, curr->uuid);
		free(curr);
	}
	gattlib_disconnect(conn);
	printf("Disconnected from device.\n");
	exit(1);
}

// parse notification and save to PV
static void writePV_callback(const uuid_t *uuidObject, const uint8_t *data, size_t len, void *user_data) {
	NotifyArgs *args = (NotifyArgs *) user_data;
	aSubRecord *pv = args->pv;
	char *uuid = args->uuid_str;

	if (strcmp(uuid, TEMP_UUID) == 0) {
		float x= data[0] + (float)(data[1]/100.0);
		memcpy(pv->vala, &x, sizeof(float));
	}
	else if (strcmp(uuid, PRESSURE_UUID) == 0) {
		int32_t n = (data[0]) | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
		float x = n + (float)data[4]/100.0;
		memcpy(pv->vala, &x, sizeof(float));
	}
	else if (strcmp(uuid, HUMIDITY_UUID) == 0 || strcmp(uuid, BUTTON_UUID) == 0) {
		float n = data[0];
		memcpy(pv->vala, &n, sizeof(float));
	}
	else if (strcmp(uuid, AIRQUAL_UUID) == 0) {
		uint16_t co = (data[0]) | (data[1] << 8);
		char buf1[32];
		snprintf(buf1, sizeof(buf1), "%" PRIu16 " eCO2 ppm\n", co);
		uint16_t tvoc = (data[2]) | (data[3] << 8);
		char buf2[32];
		snprintf(buf2, sizeof(buf2), "%" PRIu16 " TVOC ppb", tvoc);
		char buf3[64];
		strncat(buf3, buf1, sizeof(buf1));
		strncat(buf3, buf2, sizeof(buf2));
		//printf("%s\n", buf3);
		strcpy(pv->vala, buf3);
	}
	else if (strcmp(uuid, ORIENTATION_UUID) == 0) {
		int n=data[0];
		if (n == 0)
			strcpy(pv->vala, "Portrait");
		else if (n == 1)
			strcpy(pv->vala, "Landscape");
		else if (n == 2)
			strcpy(pv->vala, "Reverse Portrait");
		else if (n == 3)
			strcpy(pv->vala, "Reverse Landscape");
		else
			strcpy(pv->vala, "UNKNOWN");
	}
	else if (strcmp(uuid, EULER_UUID) == 0 || strcmp(uuid, GRAVITY_UUID) == 0) {
		int choice, i;
		float x;
		memcpy(&choice, pv->c, sizeof(int));
		if (choice == 1)
			i = 0;
		else if (choice == 2)
			i = 4;
		else if (choice == 3)
			i = 8;
		if (strcmp(uuid, EULER_UUID) == 0) {
			// 16Q16 fixed point
			int32_t raw = (data[i]) | (data[i+1] << 8) | (data[i+2] << 16) | (data[i+3] << 24);
			// convert to float
			x = ((float)(raw) / (float)(1 << 16));
		} else {
			x = (data[i]) | (data[i+1] << 8) | (data[i+2] << 16) | (data[i+3] << 24);
		}
		memcpy(pv->vala, &x, sizeof(float));
	}
	else if (strcmp(uuid, ROTATION_UUID) == 0) {
		char buf[300];
		int i=0;
		while (i < (18-1)) {
			// 2Q14 fixed point
			int16_t raw = (data[i]) | (data[i+1] << 8);
			float x = ((float)(raw)) / (float)(1 << 14);
			char val[20];
			snprintf(val, sizeof(val), "[%f]\n", x);
			strncat(buf, val, sizeof(val));
			i+=2;
		}
		strncpy(pv->vala, buf, strlen(buf));
	}
	else if (strcmp(uuid, HEADING_UUID) == 0) {
		int32_t raw = (data[0]) | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
		float x = ((float)(raw) / (float)(1 << 16));
		memcpy(pv->vala, &x, sizeof(float));
	}
	else if (strcmp(uuid, BATTERY_UUID) == 0) {
		pv->vala = data[0];
	}
	scanOnce(pv);
}


// taken from gattlib; convert string to 128 bit UUID object
static uint128_t str_to_128t(const char *string) {
	uint32_t data0, data4;
	uint16_t data1, data2, data3, data5;
	uint128_t u128;
	uint8_t *val = (uint8_t *) &u128;

	if(sscanf(string, "%08x-%04hx-%04hx-%04hx-%08x%04hx",
				&data0, &data1, &data2,
				&data3, &data4, &data5) != 6) {
		printf("Parse of UUID %s failed\n", string);
		memset(&u128, 0, sizeof(uint128_t));
		return u128;
	}

	data0 = htonl(data0);
	data1 = htons(data1);
	data2 = htons(data2);
	data3 = htons(data3);
	data4 = htonl(data4);
	data5 = htons(data5);

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

// thread function to begin listening for UUID notifications from device
static void *notificationListener(void *vargp) {
	NotifyArgs *args = (NotifyArgs *) vargp;
	gatt_connection_t *conn = get_connection();
	gattlib_register_notification(conn, writePV_callback, args);
	if (gattlib_notification_start(conn, &(args->uuid))) {
		printf("ERROR: Failed to start notifications for UUID %s (pv %s)\n", args->uuid_str, args->pv->name);
		free(args);
		return;
	}
	printf("Starting notifications for pv %s\n", args->pv->name);
	NotificationNode *node = malloc(sizeof(NotificationNode));
	node->uuid = &(args->uuid);
	node->next = 0;
	if (firstNode == 0)
		firstNode = node;
	else {
		NotificationNode *curr = firstNode;
		while (curr->next != 0)
			curr = curr->next;
		curr->next = node;
	}
	//nthreads++;
	GMainLoop *loop = g_main_loop_new(NULL, 0);
	g_main_loop_run(loop);
}

// read a single-byte UUID once
static uint8_t *readOnce(aSubRecord *pv, uuid_t uuid) {
	printf("read once %s\n", pv->name);
	uint8_t data[10];
	size_t len = sizeof(data);
	gatt_connection_t *conn = get_connection();
	if((gattlib_read_char_by_uuid(conn, &uuid, data, &len)) == -1) {
		printf("Failed to read pv %s\n", pv->name);
		return -1;
	}
	return data[0];
}

static long subscribeUUID(aSubRecord *paSub) {
	char input[35];
	char *b = paSub->b;
	if (strlen(b) > 3) {
		int len = strlen(b)-2;
		char *remainder = b + len;
		int decimal = strtol(remainder, NULL, 10);
		char hex = 55 + decimal;
		char buf[5];
		memset(buf, 0, sizeof(buf));
		strncat(buf, b, len);
		strncat(buf, &hex, 1);
		b = buf;
	}
	strcpy(input, paSub->a);
	strcat(input, b);
	uuid_t uuid;
	// battery UUID is 16 bits instead of 128 for some reason
	if (strcmp(b, BATTERY_UUID) == 0) {
		uuid_t batt = CREATE_UUID16(0x2A19);
		//uint8_t level = readOnce(paSub, batt);
		//paSub->vala = level;
		//scanOnce(paSub);
		uuid = batt;
	}
	else
		uuid = thingyUUID(input);
	NotifyArgs *args = malloc(sizeof(NotifyArgs));
	args->uuid = uuid;
	args->pv = paSub;
	strcpy(args->uuid_str, input);
	pthread_t thread_id;
	pthread_create(&thread_id, NULL, &notificationListener, (void *)args);
	return 0;
}

static long readUUID(aSubRecord *paSub) {
	char input[35];
	strcpy(input, paSub->a);
	strcat(input, paSub->b);
	gatt_connection_t *conn = get_connection();
	uuid_t uuid = thingyUUID(input);

	int i;
	char byte[4];
	char data[100];
	char out_buf[100];
	memset(out_buf, 0, sizeof(out_buf));
	size_t len = sizeof(data);
	if (gattlib_read_char_by_uuid(conn, &uuid, data, &len) == -1) {
		printf("Read of uuid %s failed.\n", input);
		return 1;
	}
	else {
		if (strcmp(input, LED_UUID) == 0) {
			int mode = (int) data[i];
			if (mode == LED_OFF) {
				char buf[4] = "Off";
				memcpy(paSub->vala, buf, strlen(buf));
			}
			else if (mode == LED_CONSTANT) {
				uint8_t red = (uint8_t) data[1];
				uint8_t green = (uint8_t) data[2];
				uint8_t blue = (uint8_t) data[3];
				char buf[25];
				snprintf(buf, sizeof(buf), "Constant R%dG%dB%d", red, green, blue);
				memcpy(paSub->vala, buf, strlen(buf));
			}
			else if (mode == LED_BREATHE) {
				int i = (int) data[1];
				char color[10];
				strcpy(color, led_colors[i-1]);
				int intensity = (int) data[2];
				uint16_t interval = (data[3]) | (data[4] << 8);
				char buf[100];
				snprintf(buf, sizeof(buf), "Breathe %s\nIntensity: %d Interval: %dms", color, intensity, interval);
				memcpy(paSub->vala, buf, strlen(buf));
			}
			else if (mode == LED_ONCE) {
				char buf[20] = "One shot";
				memcpy(paSub->vala, buf, strlen(buf));
			}
			else
				printf("LED unknown?!?\n");
		}
		else {
			for (i=0; i < len; i++) {
				snprintf(byte, sizeof(byte), "%02x ", data[i]);
				strcat(out_buf, byte);
			}
			//printf("%s\n", out_buf);
			memcpy(paSub->vala, out_buf, strlen(out_buf));
		}
	}
	return 0;
}

/* Register these symbols for use by IOC code: */
epicsRegisterFunction(subscribeUUID);
epicsRegisterFunction(readUUID);
