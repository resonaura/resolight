#include <Arduino.h>
#include <homekit/types.h>
#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <stdio.h>
#include <port.h>

extern void setYeelightPower(bool on, bool colorBulb);
extern void setYeelightBrightness(int brightness, bool colorBulb);
extern void setYeelightColorTemp(int temp, bool colorBulb);
extern void setYeelightHueAndSaturation(int hue, int saturation);

//const char * buildTime = __DATE__ " " __TIME__ " GMT";

#define ACCESSORY_NAME  ("Ceiling Light")
#define ACCESSORY_SN  ("RE:2107")  //SERIAL_NUMBER
#define ACCESSORY_MANUFACTURER ("Resonaura")
#define ACCESSORY_MODEL  ("ResoLight")

#define PIN_LED  2//D4

int led_bri = 100; //[0, 100]
unsigned int led_temp = 154;
float led_hue = 0; 
float led_saturation = 0; 
bool led_power = false; //true or flase
bool in_color_mode = false;

homekit_value_t led_on_get() {
	return HOMEKIT_BOOL(led_power);
}

void led_on_set(homekit_value_t value) {
	if (value.format != homekit_format_bool) {
		printf("Invalid on-value format: %d\n", value.format);
		return;
	}
	led_power = value.bool_value;
	if (led_power) {
		if (led_bri < 1) {
			led_bri = 100;
		}
	}

  setYeelightPower(led_power && !in_color_mode, false); // Вызываем функцию для Yeelight White
  setYeelightPower(led_power, true); // Вызываем функцию для Yeelight Color
}

homekit_value_t light_bri_get() {
	return HOMEKIT_INT(led_bri);
}

homekit_value_t light_temp_get() {
	return HOMEKIT_UINT32(led_temp);
}

homekit_value_t light_hue_get() {
	return HOMEKIT_FLOAT(led_hue);
}

homekit_value_t light_saturation_get() {
	return HOMEKIT_FLOAT(led_saturation);
}

void light_temp_set(homekit_value_t value) {
	if (value.format != homekit_format_uint32) {
		printf("Invalid temp-value format: %d\n", value.format);
		return;
	}

  led_temp = value.int_value;
  int ct = 1000000 / led_temp;

  led_power = true;
  in_color_mode = false;

	setYeelightColorTemp(ct, false);
  setYeelightColorTemp(ct, true);

  setYeelightPower(true, false);
  setYeelightPower(true, true);
}

void light_hue_set(homekit_value_t value) {
	if (value.format != homekit_format_float) {
		printf("Invalid hue-value format: %d\n", value.format);
		return;
	}

  led_power = true;
  in_color_mode = true;

  led_hue = value.float_value;
	setYeelightHueAndSaturation((int)led_hue, (int)led_saturation);

  setYeelightPower(false, false);
  setYeelightPower(true, true);
}

void light_saturation_set(homekit_value_t value) {
	if (value.format != homekit_format_float) {
		printf("Invalid saturation-value format: %d\n", value.format);
		return;
	}

  led_power = true;
  in_color_mode = true;

  led_saturation = value.float_value;
	setYeelightHueAndSaturation((int)led_hue, (int)led_saturation);

  setYeelightPower(false, false);
  setYeelightPower(true, true);
}


homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, ACCESSORY_NAME);
homekit_characteristic_t serial_number = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, ACCESSORY_SN);
homekit_characteristic_t occupancy_detected = HOMEKIT_CHARACTERISTIC_(OCCUPANCY_DETECTED, 0);
homekit_characteristic_t led_on = HOMEKIT_CHARACTERISTIC_(ON, false,
		//.callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch_on_callback),
		.getter=led_on_get,
		.setter=led_on_set
);

void occupancy_toggle() {
	const uint8_t state = occupancy_detected.value.uint8_value;
	occupancy_detected.value = HOMEKIT_UINT8((!state) ? 1 : 0);
	homekit_characteristic_notify(&occupancy_detected, occupancy_detected.value);
}

void led_bri_set(homekit_value_t value) {
	if (value.format != homekit_format_int) {
		return;
	}
	led_bri = value.int_value;

  led_power = true;

  setYeelightBrightness(in_color_mode ? 0 : led_bri, false); // Вызываем функцию для Yeelight White
  setYeelightBrightness(led_bri, true); // Вызываем функцию для Yeelight Color

  setYeelightPower(!in_color_mode && led_bri > 0, false);
  setYeelightPower(led_bri > 0, true);
}

void led_toggle() {
	led_on.value.bool_value = !led_on.value.bool_value;
	led_on.setter(led_on.value);
	homekit_characteristic_notify(&led_on, led_on.value);
}

void accessory_identify(homekit_value_t _value) {
	printf("accessory identify\n");
}

homekit_accessory_t *accessories[] =
		{
				HOMEKIT_ACCESSORY(
						.id = 1,
						.category = homekit_accessory_category_lightbulb,
						.services=(homekit_service_t*[]){
						HOMEKIT_SERVICE(ACCESSORY_INFORMATION,
						.characteristics=(homekit_characteristic_t*[]){
						&name,
						HOMEKIT_CHARACTERISTIC(MANUFACTURER, ACCESSORY_MANUFACTURER),
						&serial_number,
						HOMEKIT_CHARACTERISTIC(MODEL, ACCESSORY_MODEL),
						HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.0.9"),
						HOMEKIT_CHARACTERISTIC(IDENTIFY, accessory_identify),
						NULL
						}),
						HOMEKIT_SERVICE(LIGHTBULB, .primary=true,
						.characteristics=(homekit_characteristic_t*[]){
						HOMEKIT_CHARACTERISTIC(NAME, "Led"),
						&led_on,
						HOMEKIT_CHARACTERISTIC(BRIGHTNESS, 100, .getter=light_bri_get, .setter=led_bri_set),
            HOMEKIT_CHARACTERISTIC(COLOR_TEMPERATURE, 153, .getter=light_temp_get, .setter=light_temp_set),
            HOMEKIT_CHARACTERISTIC(HUE, 0, .getter=light_hue_get, .setter=light_hue_set),
            HOMEKIT_CHARACTERISTIC(SATURATION, 0, .getter=light_saturation_get, .setter=light_saturation_set),
						NULL
						}),
						NULL
						}),
				NULL
		};

homekit_server_config_t config = {
		.accessories = accessories,
		.password = "666-66-666",
		//.on_event = on_homekit_event,
		.setupId = "RERE"
};

void accessory_init() {
	// pinMode(PIN_LED, OUTPUT);
}

