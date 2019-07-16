// Delay (in seconds) between reconnection attempts
#define RECONNECT_DELAY 2

// UUIDs for Thingy sensors
#define TEMP_UUID "0201"
#define PRESSURE_UUID "0202"
#define HUMIDITY_UUID "0203"
#define AIRQUAL_UUID "0204"
#define LIGHT_UUID "0205"
#define LED_UUID "0301"
#define BUTTON_UUID "0302"
#define TAP_UUID "0402"
#define ORIENTATION_UUID "0403"
#define QUATERNION_UUID "0404"
#define STEP_UUID "0405"
#define RAWMOTION_UUID "0406"
#define EULER_UUID "0407"
#define ROTATION_UUID "0408"
#define HEADING_UUID "0409"
#define GRAVITY_UUID "040A"
#define BATTERY_UUID "180F"

// LED parameters
#define LED_OFF 0
#define LED_CONSTANT 1
#define LED_BREATHE 2
#define LED_ONCE 3

// Pointer for mac address given by thingyConfig()
char mac_address[100];

// Flag set in main C++ file when the IOC has started and PVs can be scanned
int ioc_started;
