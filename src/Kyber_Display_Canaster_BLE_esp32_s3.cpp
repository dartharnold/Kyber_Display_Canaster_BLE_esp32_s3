#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Adafruit_NeoPixel.h>
#include "esp_random.h"   // for random seed on ESP32

// ----------------------- NeoPixel configuration -----------------------
#define STRIPCOUNT 2
#define PIXELCOUNT 3
#define NEOBRIGHT 75
#define PIXELFORMAT NEO_GRB + NEO_KHZ800

Adafruit_NeoPixel strip[STRIPCOUNT];

// Update these to whatever GPIOs your two strips are actually on
// On XIAO-ESP32-S3, verify these GPIOs are available on your header and free.
uint8_t pins[STRIPCOUNT] = {7, 9};  // GPIO7, GPIO9

// ----------------------- Device Info -----------------------
const char* DEVNAME = "KyberVault";

// ----------------------- Timers & Counters -----------------------
#define CHANGEDELY 5*1000     // 5 second Delay (was 15s in original)  [1](https://utsystemadmin-my.sharepoint.com/personal/jarnold_utsystem_edu/Documents/Microsoft%20Copilot%20Chat%20Files/Kyber_Display_Canaster_BLE.cpp)

// ----------------------- Location IDs -----------------------
#define NOBEACON     0
#define MARKETPLACE  1
#define DROIDDEPOT   2
#define RESISTANCE   3
#define UNKNOWN      4
#define ALERT        5
#define DOKONDARS    6
#define FIRSTORDER   7

// ----------------------- Crystal Colors -----------------------
#define BLACK       0x00000000
#define WHITE       0x00FFFFFF
#define RED         0x00FF0000
#define CRIMSONRED  0x00DC143C
#define PURPLE      0x00FF00FF
#define DARKPURPLE  0x00301934
#define BLUE        0x0000008B
#define CYAN        0x0000FFFF
#define GREEN       0x0000FF00
#define YELLOW      0x00FFBF00
#define ORANGE      0x00FF7E00

// ----------------------- Filters -----------------------
#define RSSI           -75         // Minimum RSSI to consider   [1](https://utsystemadmin-my.sharepoint.com/personal/jarnold_utsystem_edu/Documents/Microsoft%20Copilot%20Chat%20Files/Kyber_Display_Canaster_BLE.cpp)
#define BLE_DISNEY     0x0183      // Manufacturer Company ID    [1](https://utsystemadmin-my.sharepoint.com/personal/jarnold_utsystem_edu/Documents/Microsoft%20Copilot%20Chat%20Files/Kyber_Display_Canaster_BLE.cpp)
const String IGNOREHOST = "SITH-TLBX"; // Ignore a specific beacon host  [1](https://utsystemadmin-my.sharepoint.com/personal/jarnold_utsystem_edu/Documents/Microsoft%20Copilot%20Chat%20Files/Kyber_Display_Canaster_BLE.cpp)

// ----------------------- State -----------------------
uint32_t last_activity;
int8_t   scan_rssi;
uint16_t area_num = 0, last_area_num = 9;
String   beacon_name = "";

// ----------------------- Forward Declarations -----------------------
void updatePixels();
void colorPulse();

// ----------------------- NimBLE scanning callback -----------------------
class MyAdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(const NimBLEAdvertisedDevice* dev) override {
    // RSSI filter
    int rssi = dev->getRSSI();
    if (rssi < RSSI) return;

    // Manufacturer data: first 2 bytes = company ID (little-endian on BLE advertising)
    std::string mfg = dev->getManufacturerData();
    if (mfg.size() < 5) return; // we need at least [0]=LSB company, [1]=MSB, [2]=type, [3..]=payload

    const uint8_t* md = reinterpret_cast<const uint8_t*>(mfg.data());
    uint16_t company = (uint16_t)md[1] << 8 | md[0];
    if (company != BLE_DISNEY) return;

    // Your original beacon type check: md[2] == 0x0A for "Location"
    if (md[2] != 0x0A) return;

    // Name (Complete Local Name) — comes via scan response if active scan is enabled
    std::string n = dev->getName();
    beacon_name = n.c_str();

    // Host ignore & timing guard
    if (beacon_name == IGNOREHOST) return;
    if ((millis() - last_activity) < CHANGEDELY) return;

    last_activity = millis();

    // Area/location ID is at md[4] in your original payload layout
    area_num = md[4];
    scan_rssi = rssi;

    // // Debug (optional)
    // Serial.printf("Host: %s  RSSI:%d  Area:%u\n", beacon_name.c_str(), scan_rssi, area_num);
  }
} MyAdvertisedDeviceCallbacks;

static MyAdvertisedDeviceCallbacks advCB;

void setup() {
  Serial.begin(115200);
  // while (!Serial) delay(10);

  Serial.println("SWGE_Beacon_Scan (NimBLE on XIAO-ESP32-S3)");
  Serial.println("----------------\n");

  last_activity = CHANGEDELY;

  // ----------------------- NeoPixel init -----------------------
  for (int x = 0; x <= 1; x++) {
    strip[x].updateLength(PIXELCOUNT);
    strip[x].updateType(PIXELFORMAT);
    strip[x].setPin(pins[x]);
  }
  for (int x = 0; x <= 1; x++) {
    strip[x].begin();
    strip[x].show();
    for (int p = 0; p <= 2; p++) {
      strip[x].setPixelColor(p, BLACK);
    }
    strip[x].setBrightness(25);
    strip[x].show();
  }

  // Better seed for ESP32
  randomSeed((uint32_t)esp_random());

  // ----------------------- NimBLE init -----------------------
  NimBLEDevice::init(DEVNAME);          // sets the local name, too  [1](https://utsystemadmin-my.sharepoint.com/personal/jarnold_utsystem_edu/Documents/Microsoft%20Copilot%20Chat%20Files/Kyber_Display_Canaster_BLE.cpp)
  NimBLEDevice::setPower(ESP_PWR_LVL_P3); // optional: TX power level; adjust if needed

  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(&advCB, false); // false = skip duplicates
  scan->setActiveScan(true);     // needed to get scan responses (for names)  [1](https://utsystemadmin-my.sharepoint.com/personal/jarnold_utsystem_edu/Documents/Microsoft%20Copilot%20Chat%20Files/Kyber_Display_Canaster_BLE.cpp)
  scan->setInterval(160);        // 160*0.625ms=100ms, matching your original  [1](https://utsystemadmin-my.sharepoint.com/personal/jarnold_utsystem_edu/Documents/Microsoft%20Copilot%20Chat%20Files/Kyber_Display_Canaster_BLE.cpp)
  scan->setWindow(80);           // 80*0.625ms = 50ms                         [1](https://utsystemadmin-my.sharepoint.com/personal/jarnold_utsystem_edu/Documents/Microsoft%20Copilot%20Chat%20Files/Kyber_Display_Canaster_BLE.cpp)
  scan->setDuplicateFilter(true);

  // Start scanning continuously (no auto-stop)
  scan->start(0, nullptr, false);

  Serial.println("Starting SWGE Location Scanning ...");
}

void updatePixels() {
  switch (area_num) {
    case MARKETPLACE:
      strip[0].setPixelColor(0, GREEN);
      strip[1].setPixelColor(0, BLUE);
      strip[0].setPixelColor(1, YELLOW);
      strip[1].setPixelColor(1, DARKPURPLE);
      strip[0].setPixelColor(2, CYAN);
      strip[1].setPixelColor(2, ORANGE);
      break;

    case ALERT:
      strip[0].setPixelColor(0, YELLOW);
      strip[1].setPixelColor(0, ORANGE);
      strip[0].setPixelColor(1, CYAN);
      strip[1].setPixelColor(1, YELLOW);
      strip[0].setPixelColor(2, ORANGE);
      strip[1].setPixelColor(2, CYAN);
      break;

    case RESISTANCE:
      strip[0].setPixelColor(0, BLUE);
      strip[1].setPixelColor(0, GREEN);
      strip[0].setPixelColor(1, CYAN);
      strip[1].setPixelColor(1, PURPLE);
      strip[0].setPixelColor(2, WHITE);
      strip[1].setPixelColor(2, YELLOW);
      break;

    case UNKNOWN:
      for (uint16_t s = 0; s <= STRIPCOUNT - 1; s++) {
        for (uint16_t p = 0; p <= PIXELCOUNT - 1; p++) {
          strip[s].setPixelColor(p, WHITE);
        }
      }
      break;

    case DROIDDEPOT:
      strip[0].setPixelColor(0, CYAN);
      strip[0].setPixelColor(1, BLUE);
      strip[0].setPixelColor(2, CYAN);
      for (uint16_t p = 0; p <= PIXELCOUNT - 1; p++) {
        strip[1].setPixelColor(p, GREEN);
      }
      break;

    case DOKONDARS:
      strip[0].setPixelColor(0, PURPLE);
      strip[1].setPixelColor(0, BLUE);
      strip[0].setPixelColor(1, GREEN);
      strip[1].setPixelColor(1, DARKPURPLE);
      strip[0].setPixelColor(2, BLUE);
      strip[1].setPixelColor(2, GREEN);
      break;

    case FIRSTORDER:
      for (uint16_t s = 0; s <= STRIPCOUNT - 1; s++) {
        for (uint16_t p = 0; p <= PIXELCOUNT - 1; p++) {
          strip[s].setPixelColor(p, RED);
        }
      }
      break;

    case NOBEACON:
    default:
      strip[0].setPixelColor(0, PURPLE);
      strip[1].setPixelColor(0, BLUE);
      strip[0].setPixelColor(1, CYAN);
      strip[1].setPixelColor(1, GREEN);
      strip[0].setPixelColor(2, YELLOW);
      strip[1].setPixelColor(2, ORANGE);
      break;
  }

  for (uint16_t s = 0; s <= STRIPCOUNT - 1; s++) {
    strip[s].setBrightness(NEOBRIGHT);
    strip[s].show();
  }
}

void colorPulse() {
  int stripNumber = random(0, 2);
  int iDelay = random(50, 201);
  int iBright = random(150, 256);
  delay(iDelay);
  strip[stripNumber].setBrightness(iBright);
  strip[stripNumber].show();
}

void loop() {
  if (area_num != last_area_num) {
    updatePixels();
    last_area_num = area_num;
  }
  colorPulse();
}