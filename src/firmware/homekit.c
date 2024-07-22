#include <Arduino.h>
#include <homekit/characteristics.h>
#include <homekit/homekit.h>
#include <homekit/types.h>
#include <port.h>
#include <stdio.h>

// Accessory info
#define ACCESSORY_NAME ("Ceiling Light")
#define ACCESSORY_SN ("RE:2107") // SERIAL_NUMBER
#define ACCESSORY_MANUFACTURER ("Resonaura")
#define ACCESSORY_MODEL ("ResoLight")

// Other constances
#define LED 2

// Yeelight API
extern void setYeelightPower(bool on, bool colorBulb);
extern void setYeelightBrightness(int brightness, bool colorBulb);
extern void setYeelightColorTemp(int temp, bool colorBulb);
extern void setYeelightHueAndSaturation(int hue, int saturation);

// State variables
int brightness = 100; //[0, 100]
unsigned int temperature = 154;
float hue = 0;
float saturation = 0;
bool on = false; // true or flase
bool inColorMode = false;

homekit_value_t onStateGet() { return HOMEKIT_BOOL(on); }

homekit_value_t temperatureStateGet() { return HOMEKIT_UINT32(temperature); }

homekit_value_t hueStateGet() { return HOMEKIT_FLOAT(hue); }

homekit_value_t saturationStateGet() { return HOMEKIT_FLOAT(saturation); }

homekit_value_t brightnessStateGet() { return HOMEKIT_INT(brightness); }

void onStateSet(homekit_value_t value)
{
  if (value.format != homekit_format_bool)
  {
    printf("Invalid on-value format: %d\n", value.format);
    return;
  }
  on = value.bool_value;
  if (on)
  {
    if (brightness < 1)
    {
      brightness = 100;
    }
  }

  setYeelightPower(on && !inColorMode,
                   false);    // Вызываем функцию для Yeelight White
  setYeelightPower(on, true); // Вызываем функцию для Yeelight Color
}

void temperatureStateSet(homekit_value_t value)
{
  if (value.format != homekit_format_uint32)
  {
    printf("Invalid temp-value format: %d\n", value.format);
    return;
  }

  temperature = value.int_value;
  int ct = 1000000 / temperature;

  on = true;
  inColorMode = false;

  setYeelightColorTemp(ct, false);
  setYeelightColorTemp(ct, true);

  setYeelightPower(true, false);
  setYeelightPower(true, true);
}

void hueStateSet(homekit_value_t value)
{
  if (value.format != homekit_format_float)
  {
    printf("Invalid hue-value format: %d\n", value.format);
    return;
  }

  on = true;
  inColorMode = true;

  hue = value.float_value;
  setYeelightHueAndSaturation((int)hue, (int)saturation);

  setYeelightPower(false, false);
  setYeelightPower(true, true);
}

void saturationStateSet(homekit_value_t value)
{
  if (value.format != homekit_format_float)
  {
    printf("Invalid saturation-value format: %d\n", value.format);
    return;
  }

  on = true;
  inColorMode = true;

  saturation = value.float_value;
  setYeelightHueAndSaturation((int)hue, (int)saturation);

  setYeelightPower(false, false);
  setYeelightPower(true, true);
}

void brightnessStateSet(homekit_value_t value)
{
  if (value.format != homekit_format_int)
  {
    return;
  }
  brightness = value.int_value;

  on = true;

  setYeelightBrightness(inColorMode ? 0 : brightness,
                        false); // Вызываем функцию для Yeelight White
  setYeelightBrightness(brightness,
                        true); // Вызываем функцию для Yeelight Color

  setYeelightPower(!inColorMode && brightness > 0, false);
  setYeelightPower(brightness > 0, true);
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, ACCESSORY_NAME);
homekit_characteristic_t serial_number =
    HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, ACCESSORY_SN);
homekit_characteristic_t led_on = HOMEKIT_CHARACTERISTIC_(
    ON, false,
    //.callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch_on_callback),
    .getter = onStateGet, .setter = onStateSet);

void ledToggle()
{
  led_on.value.bool_value = !led_on.value.bool_value;
  led_on.setter(led_on.value);
  homekit_characteristic_notify(&led_on, led_on.value);
}

void accessoryIdentify(homekit_value_t _value)
{
  printf("accessory identify\n");
}

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(
            .id = 1, .category = homekit_accessory_category_lightbulb,
            .services =
                (homekit_service_t *[]){
                    HOMEKIT_SERVICE(
                        ACCESSORY_INFORMATION,
                        .characteristics =
                            (homekit_characteristic_t *[]){
                                &name,
                                HOMEKIT_CHARACTERISTIC(MANUFACTURER,
                                                       ACCESSORY_MANUFACTURER),
                                &serial_number,
                                HOMEKIT_CHARACTERISTIC(MODEL, ACCESSORY_MODEL),
                                HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION,
                                                       "0.0.9"),
                                HOMEKIT_CHARACTERISTIC(IDENTIFY,
                                                       accessoryIdentify),
                                NULL}),
                    HOMEKIT_SERVICE(
                        LIGHTBULB, .primary = true,
                        .characteristics =
                            (homekit_characteristic_t *[]){
                                HOMEKIT_CHARACTERISTIC(NAME, "Led"), &led_on,
                                HOMEKIT_CHARACTERISTIC(
                                    BRIGHTNESS, 100,
                                    .getter = brightnessStateGet,
                                    .setter = brightnessStateSet),
                                HOMEKIT_CHARACTERISTIC(
                                    COLOR_TEMPERATURE,
                                    153,
                                    .getter = temperatureStateGet,
                                    .setter = temperatureStateSet),
                                HOMEKIT_CHARACTERISTIC(
                                    HUE,
                                    0, .getter = hueStateGet,
                                    .setter = hueStateSet),
                                HOMEKIT_CHARACTERISTIC(SATURATION, 0, .getter = saturationStateGet,
                                                       .setter =
                                                           saturationStateSet),
                                NULL}),
                    NULL}),
    NULL};

homekit_server_config_t config = {
    .accessories = accessories, .password = "666-66-666", .setupId = "RERE"};

void accessoryInit()
{
  // I will add here something later... maybe... :)
}
