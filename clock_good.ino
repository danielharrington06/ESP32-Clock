// Sketch to draw an analogue clock on the screen
// This uses anti-aliased drawing functions that are built into TFT_eSPI

// Anti-aliased lines can be drawn with sub-pixel resolution and permit lines to be
// drawn with less jaggedness.

// Based on a sketch by DavyLandman:
// https://github.com/Bodmer/TFT_eSPI/issues/905

#include "WiFi.h"

#include <WiFiManager.h>

#include <Arduino.h>
#include <TFT_eSPI.h> // Master copy here: https://github.com/Bodmer/TFT_eSPI
#include <SPI.h>

#include "NotoSansBold15.h"

TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h
TFT_eSprite face = TFT_eSprite(&tft);

#define CLOCK_X_POS 10
#define CLOCK_Y_POS 10

#define CLOCK_FG   TFT_BLUE
#define CLOCK_BG   TFT_CYAN
#define SECCOND_FG TFT_RED
#define LABEL_FG   TFT_MAGENTA

#define CLOCK_R       127.0f / 2.0f // Clock face radius (float type)
#define H_HAND_LENGTH CLOCK_R/2.0f
#define M_HAND_LENGTH CLOCK_R/1.4f
#define S_HAND_LENGTH CLOCK_R/1.3f

#define FACE_W CLOCK_R * 2 + 1
#define FACE_H CLOCK_R * 2 + 1

// Calculate 1 second increment angles. Hours and minute hand angles
// change every second so we see smooth sub-pixel movement
#define SECOND_ANGLE 360.0 / 60.0
#define MINUTE_ANGLE SECOND_ANGLE / 60.0
#define HOUR_ANGLE   MINUTE_ANGLE / 12.0

// Sprite width and height
#define FACE_W CLOCK_R * 2 + 1
#define FACE_H CLOCK_R * 2 + 1

// Time h:m:s
uint8_t h = 0, m = 0, s = 0;

float time_secs = h * 3600 + m * 60 + s;

// Load header after time_secs global variable has been created so it is in scope
#include "NTP_Time.h" // Attached to this sketch, see that tab for library needs

// Time for next tick
uint32_t targetTime = 0;

// long press to reset wifi
#define BUTTON_PIN 0
const unsigned long LONG_PRESS_DURATION = 3000; // 3 seconds
bool buttonPressed = false;
unsigned long buttonPressStart = 0;

// dump some details of the WiFi connection (if any) to the serial line
void print_wifi_info ()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("Connected to WiFi network: ");
    Serial.println(WiFi.SSID());
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Gateway address: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("Subnet mask: ");
    Serial.println(WiFi.subnetMask());
    Serial.print("MAC address: ");
    Serial.println(WiFi.macAddress());
  }
  else
  {
    Serial.println("WiFi not connected");
    Serial.print("MAC address: ");
    Serial.println(WiFi.macAddress());
  }
}

void wifi_connect( float timeout = 15 ) {
  unsigned long deadline;
  WiFiManager wm;

  // If WiFi credentials already exist, ESP will try to connect automatically.
  // If it fails, it will start a configuration portal (AP).
  bool res = wm.autoConnect("ESP32-Clock", "12345678"); // AP SSID & password

  deadline = millis() + (unsigned long)(timeout * 1000);

  while ((WiFi.status() != WL_CONNECTED) && (millis() < deadline))
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  if (!res) {
    Serial.println("Failed to connect, starting AP mode");
  } else {
    Serial.println("Connected to WiFi!");
    print_wifi_info();
  }
}

// wifi reset button
void reset_wifi() {
  WiFiManager wm;
  wm.resetSettings();
  Serial.println("WiFi credentials erased.");
  delay(1000);
  ESP.restart();
}

// =========================================================================
// Setup
// =========================================================================
void setup() {
  // format time space saved
  char* formatted = FormatTime(time_secs);

  Serial.begin(115200);
  Serial.println("Booting...");

  pinMode(BUTTON_PIN, INPUT_PULLUP); // HIGH = not pressed, LOW = pressed

  wifi_connect();

  // Initialise the screen
  tft.init();

  // Ideally set orientation for good viewing angle range because
  // the anti-aliasing effectiveness varies with screen viewing angle
  // Usually this is when screen ribbon connector is at the bottom
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  // Create the clock face sprite
  //face.setColorDepth(8); // 8-bit will work, but reduces effectiveness of anti-aliasing
  face.createSprite(FACE_W, FACE_H);

  // Only 1 font used in the sprite, so can remain loaded
  face.loadFont(NotoSansBold15);

  // Draw the whole clock - NTP time not available yet
  renderFace(time_secs);

  targetTime = millis() + 100;
}

// =========================================================================
// Loop
// =========================================================================
void loop() {
  // --- Button long press detection ---
  bool currentState = digitalRead(BUTTON_PIN) == LOW;
  
  if (currentState && !buttonPressed) {
    buttonPressed = true;
    buttonPressStart = millis();
  }

  if (!currentState && buttonPressed) {
    buttonPressed = false;
  }

  if (buttonPressed && (millis() - buttonPressStart >= LONG_PRESS_DURATION)) {
    Serial.println("Long press detected -> resetting WiFi...");
    reset_wifi();
    buttonPressed = false; // Prevent multiple triggers
  }

  // Update time periodically
  if (targetTime < millis()) {

    // Update next tick time in 100 milliseconds for smooth movement
    targetTime = millis() + 100;

    // Increment time by 100 milliseconds
    time_secs += 0.100;

    // Midnight roll-over
    if (time_secs >= (60 * 60 * 24)) time_secs = 0;

    if (fabs(fmod(time_secs, 60.0)) < 0.1) {
      char* formatted = FormatTime(time_secs+3600);
      Serial.println(formatted);
      Serial.println("\n");
    }


    // All graphics are drawn in sprite to stop flicker
    renderFace(time_secs+3600);

    // Request time from NTP server and synchronise the local clock
    // (clock may pause since this may take >100ms)
    syncTime();
  }
}

// =========================================================================
// Draw the clock face in the sprite
// =========================================================================
static void renderFace(float t) {
  float h_angle = t * HOUR_ANGLE;
  float m_angle = t * MINUTE_ANGLE;
  float s_angle = t * SECOND_ANGLE;

  // The face is completely redrawn - this can be done quickly
  face.fillSprite(TFT_BLACK);

  // Draw the face circle
  face.fillSmoothCircle( CLOCK_R, CLOCK_R, CLOCK_R, CLOCK_BG );

  // Set text datum to middle centre and the colour
  face.setTextDatum(MC_DATUM);

  // The background colour will be read during the character rendering
  face.setTextColor(CLOCK_FG, CLOCK_BG);

  // Text offset adjustment
  constexpr uint32_t dialOffset = CLOCK_R - 10;

  float xp = 0.0, yp = 0.0; // Use float pixel position for smooth AA motion

  // Draw digits around clock perimeter
  for (uint32_t h = 1; h <= 12; h++) {
    getCoord(CLOCK_R, CLOCK_R, &xp, &yp, dialOffset, h * 360.0 / 12);
    face.drawNumber(h, xp, 2 + yp);
  }

  // Add "DH" monogram
  face.setTextColor(LABEL_FG, CLOCK_BG);
  face.drawString("DH", CLOCK_R, CLOCK_R * 0.75);

  // Draw minute hand
  getCoord(CLOCK_R, CLOCK_R, &xp, &yp, M_HAND_LENGTH, m_angle);
  face.drawWideLine(CLOCK_R, CLOCK_R, xp, yp, 6.0f, CLOCK_FG);
  face.drawWideLine(CLOCK_R, CLOCK_R, xp, yp, 2.0f, CLOCK_BG);

  // Draw hour hand
  getCoord(CLOCK_R, CLOCK_R, &xp, &yp, H_HAND_LENGTH, h_angle);
  face.drawWideLine(CLOCK_R, CLOCK_R, xp, yp, 6.0f, CLOCK_FG);
  face.drawWideLine(CLOCK_R, CLOCK_R, xp, yp, 2.0f, CLOCK_BG);

  // Draw the central pivot circle
  face.fillSmoothCircle(CLOCK_R, CLOCK_R, 4, CLOCK_FG);

  // Draw second hand
  getCoord(CLOCK_R, CLOCK_R, &xp, &yp, S_HAND_LENGTH, s_angle);
  face.drawWedgeLine(CLOCK_R, CLOCK_R, xp, yp, 2.5, 1.0, SECCOND_FG);
  face.pushSprite(5, 5, TFT_TRANSPARENT);

  // Draw digital time below the clock, directly on the screen
  char* formatted = FormatTime(time_secs+3600);
  tft.setTextColor(LABEL_FG, TFT_BLACK);  // Text color with screen background
  tft.setTextDatum(MC_DATUM);             // Centered text alignment
  tft.setTextSize(4);                     // Scale up built-in font (optional)
  tft.drawString(FormatTime((time_t)(t)), tft.width() / 2, FACE_H + 60);
}

// =========================================================================
// Get coordinates of end of a line, pivot at x,y, length r, angle a
// =========================================================================
// Coordinates are returned to caller via the xp and yp pointers
#define DEG2RAD 0.0174532925
void getCoord(int16_t x, int16_t y, float *xp, float *yp, int16_t r, float a)
{
  float sx1 = cos( (a - 90) * DEG2RAD);
  float sy1 = sin( (a - 90) * DEG2RAD);
  *xp =  sx1 * r + x;
  *yp =  sy1 * r + y;
}

// format time for output to serial monitor
// uses buffer for correct formatting

char* FormatTime(time_t t)
{
  static char buffer[6];  // "HH:MM" + null terminator
  sprintf(buffer, "%02d:%02d", hour(t), minute(t));
  return buffer;
}

