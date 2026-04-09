// SensESP Relay Controller
// LILYGO T-Relay 5V 8-Channel — Relais per Signal K schalten + Status lesen

#include <memory>
#include <esp_task_wdt.h>
#include <WiFi.h>

#include "sensesp.h"
#include "sensesp/sensors/sensor.h"
#include "sensesp/sensors/digital_output.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/signalk/signalk_put_request_listener.h"
#include "sensesp_app_builder.h"

using namespace sensesp;

// =======================
//   Relay Pins (T-Relay 8-Channel)
// =======================
const uint8_t RELAY_PINS[8] = {33, 32, 13, 12, 21, 19, 18, 5};
const int NUM_RELAYS = 8;

// =======================
//   setup()
// =======================
void setup() {
  SetupLogging(ESP_LOG_DEBUG);

  SensESPAppBuilder builder;
  sensesp_app = (&builder)
                    ->set_hostname("ESP32RELAY")
                    ->set_wifi_access_point("AP_ESP32RELAY", "4f14add866")
                    ->enable_ota("")
                    ->get_app();

  // Pro Relay: PUT-Listener empfaengt SK-Befehl -> DigitalOutput schaltet GPIO
  //            RepeatSensor liest GPIO-Zustand -> SKOutput meldet Status
  const char* skPaths[NUM_RELAYS] = {
    "electrical.switches.relay1.state",
    "electrical.switches.relay2.state",
    "electrical.switches.relay3.state",
    "electrical.switches.relay4.state",
    "electrical.switches.relay5.state",
    "electrical.switches.relay6.state",
    "electrical.switches.relay7.state",
    "electrical.switches.relay8.state"
  };

  for (int i = 0; i < NUM_RELAYS; i++) {
    int pin = RELAY_PINS[i];
    pinMode(pin, OUTPUT);

    // PUT empfangen -> GPIO schalten
    auto listener = new BoolSKPutRequestListener(skPaths[i]);
    auto relay_out = new DigitalOutput(RELAY_PINS[i]);
    listener->connect_to(relay_out);

    // GPIO-Zustand zurueck an SK melden (new statt shared_ptr, damit Objekte nicht am Loop-Ende zerstoert werden)
    auto relay_sensor = new RepeatSensor<bool>(
        1000, [pin]() -> bool {
          return digitalRead(pin) == HIGH;
        });

    auto relay_sk = new SKOutput<bool>(skPaths[i]);
    relay_sensor->connect_to(relay_sk);
  }

  // Heartbeat
  auto heartbeat_sensor = std::make_shared<RepeatSensor<int>>(
      5000, []() -> int { return 1; });
  auto heartbeat_sk = std::make_shared<SKOutput<int>>(
      "system.esp32relay.heartbeat");
  heartbeat_sensor->connect_to(heartbeat_sk);

  // Connectivity-Watchdog: Reboot wenn WiFi > 2 Min weg
  static unsigned long lastWifiOk = 0;
  constexpr unsigned long WIFI_TIMEOUT_MS = 120000;  // 2 Minuten

  event_loop()->onRepeat(10000, []() {
    if (WiFi.status() == WL_CONNECTED) {
      lastWifiOk = millis();
    } else {
      unsigned long down = millis() - lastWifiOk;
      debugW("WiFi nicht verbunden seit %lu s", down / 1000);
      if (down > WIFI_TIMEOUT_MS) {
        debugW("WiFi Timeout -> Neustart!");
        ESP.restart();
      }
    }
  });

  // Watchdog: 30s Timeout
  esp_task_wdt_init(30, true);
  esp_task_wdt_add(NULL);

  while (true) {
    loop();
  }
}

void loop() {
  esp_task_wdt_reset();
  event_loop()->tick();
}
