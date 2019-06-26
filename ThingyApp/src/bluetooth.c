
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include <pthread.h>
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

#include "thingy.h"

gatt_connection_t *connection = 0;
pthread_mutex_t connlock = PTHREAD_MUTEX_INITIALIZER;

// flag for determining whether the Thingy is connected
static int alive;
static int dead;
// flag for determining if the watchdog thread started
static int watching;
static void watchdog();
// flag for determining if notifications have started
// (to avoid false disconnections)
static int started;

static void disconnect();

static char *led_colors[7] = {"Red", "Green", "Yellow", "Blue", "Purple", "Cyan", "White"};
static char *tap_directions[6] = {"+X", "-X", "+Y", "-Y", "+Z", "-Z"};

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
	if (connection != 0) {
		return connection;
	}
	//pthread_mutex_lock(&connlock);
	if (connection != 0) {
		//pthread_mutex_unlock(&connlock);
		return connection;
	}
	//printf("Connecting to device %s...\n", mac_address);
	connection = gattlib_connect(NULL, mac_address, BDADDR_LE_PUBLIC, BT_SEC_LOW, 0, 0);
	// start watchdog thread
	if (watching == 0) {
		pthread_t pid;
		pthread_create(&pid, NULL, &watchdog, NULL);
		watching = 1;
	}
	alive = 1;
	signal(SIGINT, disconnect);
	//pthread_mutex_unlock(&connlock);
	//printf("Connected.\n");
	return connection;
}

static void disconnect() {
	started = 0;
	printf("Stopping notifications...\n");
	if (firstNode != 0) {
		NotificationNode *curr = firstNode; 
		NotificationNode *next;
		while (curr->next != 0) {
			next = curr->next;
			gattlib_notification_stop(connection, curr->uuid);
			free(curr);
			curr = next;
		}
		gattlib_notification_stop(connection, curr->uuid);
		free(curr);
	}
	gattlib_disconnect(connection);
	printf("Disconnected from device.\n");
	exit(1);
}

// thread function to ensure the Thingy is connected and attempt reconnection if necessary
static void watchdog() {
	while(1) {
		if (started && alive == 0) {
			dead = 1;
			printf("ERROR: Lost connection to Thingy\n");
			printf("Attempting reconnection...\n");
			connection = 0;
			get_connection();
			if (connection == 0) {
				printf("Unable to reconnect. Reattempt in %d seconds\n", WATCHDOG_DELAY);
			}
		}
		alive = 0;
		sleep(WATCHDOG_DELAY);
	}
}

// parse notification and save to PV
static void writePV_callback(const uuid_t *uuidObject, const uint8_t *data, size_t len, void *user_data) {
	NotifyArgs *args = (NotifyArgs *) user_data;
	aSubRecord *pv = args->pv;
	char *uuid = args->uuid_str;
	
	// confirm thingy is connected
	if (dead) {
		printf("Successfully reconnected.\n");
		dead = 0;
	}
	alive = 1;
	started = 1;

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
		char buf1[50];
		memset(buf1, 0, sizeof(buf1));
		snprintf(buf1, sizeof(buf1), "%u eCO2 ppm\n", (unsigned int)co);
		uint16_t tvoc = (data[2]) | (data[3] << 8);
		char buf2[50];
		memset(buf2, 0, sizeof(buf2));
		snprintf(buf2, sizeof(buf2), "%u TVOC ppb", (unsigned int)tvoc);
		char buf3[110];
		memset(buf3, 0, sizeof(buf3));
		strncat(buf3, buf1, strlen(buf1));
		strncat(buf3, buf2, strlen(buf2));
		strncpy(pv->vala, buf3, strlen(buf3));
	}
	else if (strcmp(uuid, LIGHT_UUID) == 0) {
		uint16_t r = (data[0]) | (data[1] << 8);
		uint16_t g = (data[2]) | (data[3] << 8);
		uint16_t b = (data[4]) | (data[5] << 8);
		uint16_t c = (data[6]) | (data[7] << 8);
		char buf[50];
		memset(buf, 0, sizeof(buf));
		//printf("R%d & %d, G%d & %d, B%d & %d, C%d & %d\n", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
		//printf("R %d %d %u\n", data[0], data[1], (unsigned int)r);
		snprintf(buf, sizeof(buf), "R%d G%d B%d\nClear %d", r,g,b,c);
		strncpy(pv->vala, buf, strlen(buf));
	}
	else if (strcmp(uuid, TAP_UUID) == 0) {
		uint8_t i = data[0];
		uint8_t count = data[1];
		char buf[30];
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf), "%d taps in %s direction", count, tap_directions[i-1]);
		strncpy(pv->vala, buf, strlen(buf));
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
	else if (strcmp(uuid, STEP_UUID) == 0) {
		int32_t steps = (data[0]) | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
		int32_t time = (data[4]) | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
		time = time / 1000;
		char buf[100];
		snprintf(buf, sizeof(buf), "%d in %dsec", steps, time);
		strncpy(pv->vala, buf, strlen(buf));
	}
	else if (strcmp(uuid, QUATERNION_UUID) == 0 || strcmp(uuid, EULER_UUID) == 0 || strcmp(uuid, GRAVITY_UUID) == 0) {
		int choice, i;
		float x;
		memcpy(&choice, pv->c, sizeof(int));
		if (choice < 1 || choice > 4) {
			printf("Invalid CHOICE for %s\n", pv->name);
			return;
		}
		i = 4 * (choice-1);
		int32_t raw = (data[i]) | (data[i+1] << 8) | (data[i+2] << 16) | (data[i+3] << 24);
		if (strcmp(uuid, EULER_UUID) == 0) {
			// 16Q16 fixed point -> float
			x = ((float)(raw) / (float)(1 << 16));
		} else if (strcmp(uuid, QUATERNION_UUID) == 0) {
			// 2Q30 fixed point -> float
			x = ((float)(raw) / (float)(1 << 30));
		} else {
			// gravity is just a float???
			//x = (data[i]) | (data[i+1] << 8) | (data[i+2] << 16) | (data[i+3] << 24);
			memcpy(&x, data+i, sizeof(x));
		}
		memcpy(pv->vala, &x, sizeof(float));
	}
	else if (strcmp(uuid, RAWMOTION_UUID) == 0) {
		int choice, i;
		memcpy(&choice, pv->c, sizeof(int));
		if (choice < 1 || choice > 9) {
			printf("Invalid CHOICE for %s\n", pv->name);
			return;
		}
		i = 2 * (choice-1);
		int16_t raw = (data[i]) | (data[i+1] << 8);
		float x;
		// Accelerometer: 6Q10 format
		if (choice >= 1 && choice <= 3)
			x = ((float)(raw)) / (float)(1 << 10);
		// Gyroscope: 11Q5 format
		else if (choice >= 4 && choice <= 6)
			x = ((float)(raw)) / (float)(1 << 5);
		// Compass: 12Q4 format
		else
			x = ((float)(raw)) / (float)(1 << 4);
		memcpy(pv->vala, &x, sizeof(float));

	}
	else if (strcmp(uuid, ROTATION_UUID) == 0) {
		char buf[300];
		memset(buf, 0, sizeof(buf));
		int i=0;
		// create rotation matrix in text form
		while (i < (18-1)) {
			// 2Q14 fixed point
			int16_t raw = (data[i]) | (data[i+1] << 8);
			float x = ((float)(raw)) / (float)(1 << 14);
			char val[20];
			if (i==4 || i==10)
				snprintf(val, sizeof(val), "%.2f\n", x);
			else
				snprintf(val, sizeof(val), "%.2f ", x);
			strncat(buf, val, strlen(val));
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

	if (ioc_started)
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
	char buf[40];
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
	get_connection();
	NotifyArgs *args = (NotifyArgs *) vargp;
	gattlib_register_notification(connection, writePV_callback, args);
	if (gattlib_notification_start(connection, &(args->uuid))) {
		printf("ERROR: Failed to start notifications for UUID %s (pv %s)\n", args->uuid_str, args->pv->name);
		free(args);
		return;
	}
	printf("Starting notifications for pv %s\n", args->pv->name);
	NotificationNode *node = malloc(sizeof(NotificationNode));
	node->uuid = &(args->uuid);
	node->next = 0;
	pthread_mutex_lock(&connlock);
	if (firstNode == 0)
		firstNode = node;
	else {
		NotificationNode *curr = firstNode;
		while (curr->next != 0)
			curr = curr->next;
		curr->next = node;
	}
	pthread_mutex_unlock(&connlock);
	GMainLoop *loop = g_main_loop_new(NULL, 0);
	g_main_loop_run(loop);
}

static long subscribeUUID(aSubRecord *pv) {
	char input[40];
	char *b = pv->b;
	// convert decimal to hex
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
	strcpy(input, pv->a);
	strcat(input, b);
	uuid_t uuid = thingyUUID(input);
	NotifyArgs *args = malloc(sizeof(NotifyArgs));
	args->uuid = uuid;
	args->pv = pv;
	strcpy(args->uuid_str, input);
	pthread_t thread_id;
	pthread_create(&thread_id, NULL, &notificationListener, (void *)args);
	return 0;
}

static long readUUID(aSubRecord *pv) {
	char input[40];
	strcpy(input, pv->a);
	strcat(input, pv->b);
	uuid_t uuid;
	if (strcmp(input, "018015") == 0) {
		// battery is a 16 bit UUID for some reason
		uuid_t batt = CREATE_UUID16(0x2A19);
		uuid = batt;
	}
	else
		uuid = thingyUUID(input);
	get_connection();

	int i;
	char byte[4];
	char data[100];
	char out_buf[100];
	memset(out_buf, 0, sizeof(out_buf));
	size_t len = sizeof(data);
	if (gattlib_read_char_by_uuid(connection, &uuid, data, &len) == -1) {
		printf("Read of uuid %s (pv %s) failed.\n", input, pv->name);
		return 1;
	}
	else {
		if (strcmp(input, "018015") == 0) {
			int level = data[0];
			char buf[5];
			snprintf(buf, sizeof(buf), "%d%%", level);
			strncpy(pv->vala, buf, strlen(buf));
		}
		else if (strcmp(input, LED_UUID) == 0) {
			int mode = (int) data[0];
			if (mode == LED_OFF) {
				char buf[4] = "Off";
				strncpy(pv->vala, buf, strlen(buf));
			}
			else if (mode == LED_CONSTANT) {
				uint8_t red = (uint8_t) data[1];
				uint8_t green = (uint8_t) data[2];
				uint8_t blue = (uint8_t) data[3];
				char buf[25];
				snprintf(buf, sizeof(buf), "Constant R%dG%dB%d", red, green, blue);
				strncpy(pv->vala, buf, strlen(buf));
			}
			else if (mode == LED_BREATHE) {
				int i = (int) data[1];
				char color[10];
				strcpy(color, led_colors[i-1]);
				int intensity = (int) data[2];
				uint16_t interval = (data[3]) | (data[4] << 8);
				char buf[100];
				snprintf(buf, sizeof(buf), "Breathe %s\n%d%% %dms", color, intensity, interval);
				//snprintf(buf, sizeof(buf), "Breathe %s\nIntensity %d%% Interval %dms", color, intensity, interval);
				//printf("%s\n", buf, strlen(buf));
				strncpy(pv->vala, buf, strlen(buf));
				//pv->vala = buf;
			}
			else if (mode == LED_ONCE) {
				char buf[20] = "One shot";
				strncpy(pv->vala, buf, strlen(buf));
			}
			else
				printf("LED undefined reading\n");
		}
		else {
			for (i=0; i < len; i++) {
				snprintf(byte, sizeof(byte), "%02x ", data[i]);
				strcat(out_buf, byte);
			}
			//printf("%s\n", out_buf);
			strncpy(pv->vala, out_buf, strlen(out_buf));
		}
	}
	return 0;
}


/* Register these symbols for use by IOC code: */
epicsRegisterFunction(subscribeUUID);
epicsRegisterFunction(readUUID);
