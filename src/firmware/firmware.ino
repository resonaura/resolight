#include <Arduino.h>
#include <ESP8266WiFi.h>

#include <arduino_homekit_server.h>

#include "include/ButtonDebounce.h"
#include "include/ButtonHandler.h"

#include "credentials.h"

#define PIN_LED 2//D0

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

#define SIMPLE_INFO(fmt, ...)   printf_P(PSTR(fmt "\n") , ##__VA_ARGS__);

WiFiClient client;
const char* bulbColor = BULB_COLOR_IP; // IP адрес Yeelight Color
const char* bulbWhite = BULB_WHITE_IP; // IP адрес Yeelight White
const uint16_t port = 55443; // Порт для Yeelight

void yeelightControlColorBulb(const String &command) {
  digitalWrite(PIN_LED, LOW);
  Serial.println(command);
  if (!client.connect(bulbColor, port)) {
    Serial.println("Connection to Yeelight Color failed");
    return;
  }
  client.println(command);
  client.stop();
  digitalWrite(PIN_LED, HIGH);
}

void yeelightControlWhiteBulb(const String &command) {
  digitalWrite(PIN_LED, LOW);
  Serial.println(command);
  if (!client.connect(bulbWhite, port)) {
    Serial.println("Connection to Yeelight White failed");
    return;
  }
  client.println(command);
  client.stop();
  digitalWrite(PIN_LED, HIGH);
}

extern "C" void setYeelightPower(bool on, bool colorBulb) {
  if (on) {
    String command = "{\"id\":1,\"method\":\"set_power\",\"params\":[\"on\",\"smooth\",500]}";
    
    if(colorBulb) {
      yeelightControlColorBulb(command);
    } else {
      yeelightControlWhiteBulb(command);
    }
  } else {
    String command = "{\"id\":1,\"method\":\"set_power\",\"params\":[\"off\",\"smooth\",500]}";

    if(colorBulb) {
      yeelightControlColorBulb(command);
    } else {
      yeelightControlWhiteBulb(command);
    }
  }
}

extern "C" void setYeelightBrightness(int brightness, bool colorBulb) {
  String command = "{\"id\":1,\"method\":\"set_bright\",\"params\":[";
  command += String(brightness) + ", \"smooth\", 500]}";
  
  if(colorBulb) {
    yeelightControlColorBulb(command);
  } else {
    yeelightControlWhiteBulb(command);
  }
}

extern "C" void setYeelightColorTemp(int temp, bool colorBulb) {
  int stable_temp = min(max(temp, 2700), 6500);
  String command = "{\"id\":1,\"method\":\"set_ct_abx\",\"params\":[";
  command += String(stable_temp) + ", \"smooth\", 500]}";

  if(colorBulb) {
    yeelightControlColorBulb(command);
  } else {
    yeelightControlWhiteBulb(command);
  }
}

extern "C" void setYeelightHueAndSaturation(int hue, int saturation) {
    Serial.println("h: " + String(hue) + ", s: " + String(saturation));

    int value = 100;
    
    String command = "{\"id\":1,\"method\":\"set_hsv\",\"params\":[";
    command += String(hue) + ", " + String(saturation) + ", " + String(value) + ", \"smooth\", 500]}";
    yeelightControlColorBulb(command);
}



void blink_led(int interval, int count) {
	for (int i = 0; i < count; i++) {
		builtinledSetStatus(true);
		delay(interval);
		builtinledSetStatus(false);
		delay(interval);
	}
}

void setup() {
	Serial.begin(115200);
	Serial.setRxBufferSize(32);
	Serial.setDebugOutput(false);

	pinMode(PIN_LED, OUTPUT);

  WiFi.hostname("Resonaura Tech Inc.");
	WiFi.mode(WIFI_STA);
	WiFi.persistent(false);
	WiFi.disconnect(false);
	WiFi.setAutoReconnect(true);
	WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  setYeelightColorTemp(6500, false);
  setYeelightColorTemp(6500, true);
  setYeelightBrightness(100, false);
  setYeelightBrightness(100, true);
  setYeelightPower(false, false);
  setYeelightPower(false, true);

	SIMPLE_INFO("");
	SIMPLE_INFO("SketchSize: %d", ESP.getSketchSize());
	SIMPLE_INFO("FreeSketchSpace: %d", ESP.getFreeSketchSpace());
	SIMPLE_INFO("FlashChipSize: %d", ESP.getFlashChipSize());
	SIMPLE_INFO("FlashChipRealSize: %d", ESP.getFlashChipRealSize());
	SIMPLE_INFO("FlashChipSpeed: %d", ESP.getFlashChipSpeed());
	SIMPLE_INFO("SdkVersion: %s", ESP.getSdkVersion());
	SIMPLE_INFO("FullVersion: %s", ESP.getFullVersion().c_str());
	SIMPLE_INFO("CpuFreq: %dMHz", ESP.getCpuFreqMHz());
	SIMPLE_INFO("FreeHeap: %d", ESP.getFreeHeap());
	SIMPLE_INFO("ResetInfo: %s", ESP.getResetInfo().c_str());
	SIMPLE_INFO("ResetReason: %s", ESP.getResetReason().c_str());
	INFO_HEAP();
	homekit_setup();
	INFO_HEAP();
	blink_led(200, 3);
}

void loop() {
	homekit_loop();
}

void builtinledSetStatus(bool on) {
	digitalWrite(PIN_LED, on ? LOW : HIGH);
}

//==============================
// Homekit setup and loop
//==============================

extern "C" homekit_server_config_t config;
extern "C" homekit_characteristic_t name;
extern "C" void occupancy_toggle();
extern "C" void led_toggle();
extern "C" void accessory_init();

ButtonDebounce btn(0, INPUT_PULLUP, LOW);
ButtonHandler btnHandler;

void IRAM_ATTR btnInterrupt() {
	btn.update();
}


void homekit_setup() {
	accessory_init();

	arduino_homekit_setup(&config);

  SIMPLE_INFO("Nya ^^");

	btn.setCallback(std::bind(&ButtonHandler::handleChange, &btnHandler,
			std::placeholders::_1));
	btn.setInterrupt(btnInterrupt);
	btnHandler.setIsDownFunction(std::bind(&ButtonDebounce::checkIsDown, &btn));
	btnHandler.setCallback([](button_event e) {
		if (e == BUTTON_EVENT_SINGLECLICK) {
			SIMPLE_INFO("Button Event: SINGLECLICK");
			led_toggle();
		} else if (e == BUTTON_EVENT_DOUBLECLICK) {
			SIMPLE_INFO("Button Event: DOUBLECLICK");
		} else if (e == BUTTON_EVENT_LONGCLICK) {
			SIMPLE_INFO("Button Event: LONGCLICK");
			SIMPLE_INFO("Rebooting...");
			homekit_storage_reset();
			ESP.restart(); // or system_restart();
		}
	});
}

void homekit_loop() {
	btnHandler.loop();
	arduino_homekit_loop();
	static uint32_t next_heap_millis = 0;
	uint32_t time = millis();
	if (time > next_heap_millis) {
		SIMPLE_INFO("heap: %d, sockets: %d",
				ESP.getFreeHeap(), arduino_homekit_connected_clients_count());
		next_heap_millis = time + 5000;
	}
}
