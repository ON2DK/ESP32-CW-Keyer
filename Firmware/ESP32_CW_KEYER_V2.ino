/*
  ESP32_CW_KEYER_V2.ino
  ------------------------------------------------------------
  ESP32 CW Keyer V2 - hardware matched firmware baseline
  Project: ON2DK / ESP32-CW-Keyer

  Target hardware:
  - 38-pin ESP32 DevKit / ESP32-WROOM-32D
  - GMT024-08-SPI8P V1.3 / ST7789 240x320 TFT
  - Rotary encoder EC11
  - Paddle + straight key
  - LM386 audio/sidetone
  - 4 memory buttons on one ADC input
  - KEY / PTT transistor outputs
  - Raw 3.3 V CAT UART
  - Icom CI-V interface is implemented in hardware on the PCB

  Libraries:
  - Adafruit GFX Library
  - Adafruit ST7789
  - ESP32Encoder
  - Preferences

  IMPORTANT:
  The four memory buttons share GPIO34 through a resistor ladder.
  ADC values depend on the actual resistors and ESP32 ADC tolerance.
  Adjust MEM_ADC_TARGET[] after reading the live ADC values shown on
  the MEMORY CAL screen. The rest of the GPIO mapping below matches
  the definitive V2 PCB.
*/

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <ESP32Encoder.h>
#include <Preferences.h>

// ============================================================
// DEFINITIVE V2 PCB GPIO MAPPING
// ============================================================

#define TFT_DC          2
#define STRAIGHT_KEY    4
#define TFT_CS          5
#define TFT_RST        13
#define TOUCH_IRQ      14
#define ENC_A          16
#define ENC_B          17
#define TFT_SCK        18
#define KEY_CTRL       19
#define CAT_TX         21
#define PTT_CTRL       22
#define TFT_MOSI       23
#define AUDIO_PIN      25
#define ENC_SW         26
#define TFT_BL         27
#define PADDLE_DAH     32
#define CAT_RX         33
#define BUTTON_ADC     34
#define PADDLE_DIT     35

// UART0 GPIO1/GPIO3 remains available for USB/programming/debug.

// ============================================================
// DISPLAY / OBJECTS
// ============================================================

Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);
ESP32Encoder encoder;
Preferences prefs;
HardwareSerial CAT(2);

// 320x240 landscape after rotation(1)
static const uint16_t C_BLACK   = 0x0000;
static const uint16_t C_WHITE   = 0xFFFF;
static const uint16_t C_ORANGE  = 0xFD20;
static const uint16_t C_AMBER   = 0xFBE0;
static const uint16_t C_GREEN   = 0x07E0;
static const uint16_t C_RED     = 0xF800;
static const uint16_t C_CYAN    = 0x07FF;
static const uint16_t C_GREY    = 0xC618;
static const uint16_t C_DGREY   = 0x7BEF;

// ============================================================
// SETTINGS
// ============================================================

struct Settings {
  int wpm = 18;
  int toneHz = 700;
  bool pttOnMemory = false;
};

Settings settings;

int dotMs = 1200 / 18;

// Memory text can be changed later in the menu or firmware.
String memoryText[4] = {
  "CQ CQ CQ DE ON2DK ON2DK K",
  "ON2DK",
  "TNX FER QSO 73",
  "73 SK"
};

// ------------------------------------------------------------
// MEMORY ADC CALIBRATION
// ------------------------------------------------------------
// Replace these 4 values with the measured ADC values for M1..M4.
// Defaults are deliberately spaced placeholders.
// Unpressed should normally be near the top of the ADC range.
int MEM_ADC_TARGET[4] = {650, 1300, 2000, 2750};
const int MEM_ADC_TOLERANCE = 220;
const int MEM_ADC_RELEASE = 3600;

// ============================================================
// MORSE TABLE
// ============================================================

struct MorseEntry {
  char c;
  const char *code;
};

const MorseEntry MORSE[] = {
  {'A',".-"},{'B',"-..."},{'C',"-.-."},{'D',"-.."},{'E',"."},
  {'F',"..-."},{'G',"--."},{'H',"...."},{'I',".."},{'J',".---"},
  {'K',"-.-"},{'L',".-.."},{'M',"--"},{'N',"-."},{'O',"---"},
  {'P',".--."},{'Q',"--.-"},{'R',".-."},{'S',"..."},{'T',"-"},
  {'U',"..-"},{'V',"...-"},{'W',".--"},{'X',"-..-"},{'Y',"-.--"},
  {'Z',"--.."},{'0',"-----"},{'1',".----"},{'2',"..---"},
  {'3',"...--"},{'4',"....-"},{'5',"....."},{'6',"-...."},
  {'7',"--..."},{'8',"---.."},{'9',"----."},
  {'.',".-.-.-"},{',',"--..--"},{'?',"..--.."},{'/',"-..-."},
  {'=',"-...-"},{'+',".-.-."},{'-',"-....-"}
};
const int MORSE_COUNT = sizeof(MORSE) / sizeof(MORSE[0]);

// ============================================================
// UI STATE
// ============================================================

enum Screen {
  SCR_MAIN,
  SCR_FREE_KEY,
  SCR_SETTINGS,
  SCR_MEMORY_CAL,
  SCR_CAT_STATUS
};

Screen screen = SCR_MAIN;

const char* MAIN_ITEMS[] = {
  "Vrij seinen",
  "Geheugen M1-M4",
  "Instellingen",
  "Memory ADC kalibratie",
  "CAT status"
};
const int MAIN_COUNT = 5;

const char* SET_ITEMS[] = {
  "WPM",
  "Toonhoogte",
  "PTT bij memory",
  "Terug"
};
const int SET_COUNT = 4;

int menuIndex = 0;
int setIndex = 0;
long lastEncoderCount = 0;

bool lastEncButton = HIGH;
unsigned long encPressStarted = 0;
bool encLongHandled = false;

// ============================================================
// KEYER / INPUT STATE
// ============================================================

enum ElementPhase { EL_IDLE, EL_TONE, EL_GAP };
ElementPhase elementPhase = EL_IDLE;

char activeElement = 0;
unsigned long phaseStarted = 0;
unsigned long lastElementFinished = 0;

String currentMorse = "";
String decodedLine = "";

bool straightWasDown = false;
unsigned long straightDownAt = 0;

int lastMemButton = -1;
unsigned long memLastChange = 0;

// ============================================================
// HELPERS
// ============================================================

void loadSettings() {
  prefs.begin("cwkeyer-v2", true);
  settings.wpm = prefs.getInt("wpm", 18);
  settings.toneHz = prefs.getInt("tone", 700);
  settings.pttOnMemory = prefs.getBool("pttmem", false);
  for (int i = 0; i < 4; i++) {
    String key = "mem" + String(i + 1);
    memoryText[i] = prefs.getString(key.c_str(), memoryText[i]);
  }
  prefs.end();

  settings.wpm = constrain(settings.wpm, 5, 50);
  settings.toneHz = constrain(settings.toneHz, 300, 1200);
  dotMs = 1200 / settings.wpm;
}

void saveSettings() {
  prefs.begin("cwkeyer-v2", false);
  prefs.putInt("wpm", settings.wpm);
  prefs.putInt("tone", settings.toneHz);
  prefs.putBool("pttmem", settings.pttOnMemory);
  for (int i = 0; i < 4; i++) {
    String key = "mem" + String(i + 1);
    prefs.putString(key.c_str(), memoryText[i]);
  }
  prefs.end();
}

void keyRadio(bool on) {
  digitalWrite(KEY_CTRL, on ? HIGH : LOW);
}

void setPTT(bool on) {
  digitalWrite(PTT_CTRL, on ? HIGH : LOW);
}

void sidetone(bool on) {
  if (on) {
    tone(AUDIO_PIN, settings.toneHz);
    keyRadio(true);
  } else {
    noTone(AUDIO_PIN);
    keyRadio(false);
  }
}

const char* morseFor(char c) {
  c = toupper((unsigned char)c);
  for (int i = 0; i < MORSE_COUNT; i++) {
    if (MORSE[i].c == c) return MORSE[i].code;
  }
  return nullptr;
}

char decodeMorse(const String &s) {
  for (int i = 0; i < MORSE_COUNT; i++) {
    if (s == MORSE[i].code) return MORSE[i].c;
  }
  return '#';
}

void centered(const String &txt, int y, int sz, uint16_t color) {
  int16_t x1, y1;
  uint16_t w, h;
  tft.setTextSize(sz);
  tft.setTextColor(color, C_BLACK);
  tft.getTextBounds(txt, 0, y, &x1, &y1, &w, &h);
  tft.setCursor((320 - w) / 2, y);
  tft.print(txt);
}

void topBar(const String &title) {
  tft.fillRect(0, 0, 320, 36, C_BLACK);
  tft.drawFastHLine(0, 36, 320, C_ORANGE);
  tft.setTextSize(2);
  tft.setTextColor(C_ORANGE, C_BLACK);
  tft.setCursor(8, 9);
  tft.print(title);
}

void footer(const String &txt) {
  tft.fillRect(0, 218, 320, 22, C_BLACK);
  tft.drawFastHLine(0, 217, 320, C_DGREY);
  centered(txt, 224, 1, C_GREY);
}

void menuLine(int y, const String &txt, bool selected) {
  uint16_t bg = selected ? C_ORANGE : C_BLACK;
  uint16_t fg = selected ? C_BLACK : C_WHITE;
  tft.fillRoundRect(16, y, 288, 30, 4, bg);
  tft.setTextSize(2);
  tft.setTextColor(fg, bg);
  tft.setCursor(27, y + 7);
  tft.print(selected ? "> " : "  ");
  tft.print(txt);
}

void drawMain() {
  screen = SCR_MAIN;
  tft.fillScreen(C_BLACK);
  topBar("CW_KEYER_V2");
  for (int i = 0; i < MAIN_COUNT; i++) {
    menuLine(44 + i * 34, MAIN_ITEMS[i], i == menuIndex);
  }
  footer("MENU: draai = kies, druk = open");
}

void drawFree() {
  screen = SCR_FREE_KEY;
  tft.fillScreen(C_BLACK);
  topBar("VRIJ SEINEN");
  centered("PADDLE / STRAIGHT", 48, 2, C_AMBER);
  centered(currentMorse.length() ? currentMorse : "...", 88, 4,
           currentMorse.length() ? C_CYAN : C_DGREY);
  tft.drawFastHLine(20, 140, 280, C_DGREY);
  centered(decodedLine.length() ? decodedLine : "Uitvoer", 158, 3,
           decodedLine.length() ? C_GREEN : C_GREY);
  footer("Lang MENU = terug");
}

void drawSettings() {
  screen = SCR_SETTINGS;
  tft.fillScreen(C_BLACK);
  topBar("INSTELLINGEN");

  for (int i = 0; i < SET_COUNT; i++) {
    int y = 48 + i * 38;
    uint16_t bg = i == setIndex ? C_ORANGE : C_BLACK;
    uint16_t fg = i == setIndex ? C_BLACK : C_WHITE;
    tft.fillRoundRect(14, y, 292, 32, 4, bg);
    tft.setTextColor(fg, bg);
    tft.setTextSize(2);
    tft.setCursor(25, y + 8);
    tft.print(i == setIndex ? "> " : "  ");
    tft.print(SET_ITEMS[i]);

    if (i == 0) {
      tft.setCursor(228, y + 8);
      tft.print(settings.wpm);
    } else if (i == 1) {
      tft.setCursor(228, y + 8);
      tft.print(settings.toneHz);
    } else if (i == 2) {
      tft.setCursor(234, y + 8);
      tft.print(settings.pttOnMemory ? "AAN" : "UIT");
    }
  }
  footer("Draai = kies/wijzig, druk = bevestig");
}

void drawMemoryCal() {
  screen = SCR_MEMORY_CAL;
  tft.fillScreen(C_BLACK);
  topBar("MEMORY ADC KALIBRATIE");
  centered("Druk M1, M2, M3, M4", 58, 2, C_WHITE);
  centered("Lees ADC waarde af", 88, 2, C_AMBER);
  footer("Lang MENU = terug");
}

void drawCatStatus() {
  screen = SCR_CAT_STATUS;
  tft.fillScreen(C_BLACK);
  topBar("CAT STATUS");
  tft.setTextSize(2);
  tft.setTextColor(C_WHITE, C_BLACK);
  tft.setCursor(18, 58);
  tft.print("TX GPIO21");
  tft.setCursor(18, 88);
  tft.print("RX GPIO33");
  tft.setCursor(18, 118);
  tft.print("3.3V UART");
  tft.setTextColor(C_ORANGE, C_BLACK);
  tft.setCursor(18, 153);
  tft.print("NIET RS-232");
  footer("Lang MENU = terug");
}

void redrawFreeInput() {
  tft.fillRect(0, 82, 320, 52, C_BLACK);
  centered(currentMorse.length() ? currentMorse : "...", 88, 4,
           currentMorse.length() ? C_CYAN : C_DGREY);
}

void redrawDecoded() {
  tft.fillRect(0, 150, 320, 60, C_BLACK);
  centered(decodedLine.length() ? decodedLine : "Uitvoer", 158, 3,
           decodedLine.length() ? C_GREEN : C_GREY);
}

// ============================================================
// MEMORY ADC
// ============================================================

int readMemoryButton() {
  int v = analogRead(BUTTON_ADC);

  if (v >= MEM_ADC_RELEASE) return -1;

  int best = -1;
  int bestDiff = 9999;
  for (int i = 0; i < 4; i++) {
    int d = abs(v - MEM_ADC_TARGET[i]);
    if (d < bestDiff) {
      bestDiff = d;
      best = i;
    }
  }

  return bestDiff <= MEM_ADC_TOLERANCE ? best : -1;
}

void updateMemoryCal() {
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw < 100) return;
  lastDraw = millis();

  int v = analogRead(BUTTON_ADC);
  tft.fillRect(45, 125, 230, 70, C_BLACK);
  centered("ADC = " + String(v), 128, 4, C_CYAN);

  int b = readMemoryButton();
  if (b >= 0) {
    centered("M" + String(b + 1), 177, 2, C_GREEN);
  } else {
    centered("geen knop", 177, 2, C_GREY);
  }
}

// ============================================================
// MORSE OUTPUT
// ============================================================

void sendElement(char e) {
  sidetone(true);
  delay(e == '.' ? dotMs : dotMs * 3);
  sidetone(false);
  delay(dotMs);
}

void sendCharacter(char c) {
  const char *m = morseFor(c);
  if (!m) {
    delay(dotMs * 7);
    return;
  }
  for (int i = 0; m[i]; i++) sendElement(m[i]);
  delay(dotMs * 2); // + one element gap already sent = 3 dots total
}

void sendText(const String &s) {
  if (settings.pttOnMemory) {
    setPTT(true);
    delay(100);
  }

  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == ' ') {
      delay(dotMs * 4); // previous char already supplied 3 dots -> total 7
    } else {
      sendCharacter(c);
    }
  }

  if (settings.pttOnMemory) {
    delay(100);
    setPTT(false);
  }
}

void triggerMemory(int idx) {
  if (idx < 0 || idx > 3) return;

  tft.fillRect(0, 185, 320, 28, C_BLACK);
  centered("M" + String(idx + 1) + ": " + memoryText[idx], 190, 1, C_ORANGE);
  sendText(memoryText[idx]);
}

// ============================================================
// PADDLE / STRAIGHT KEY
// ============================================================

void appendElement(char e) {
  if (currentMorse.length() < 8) currentMorse += e;
  lastElementFinished = millis();
  if (screen == SCR_FREE_KEY) redrawFreeInput();
}

void startElement(char e) {
  activeElement = e;
  phaseStarted = millis();
  sidetone(true);
  elementPhase = EL_TONE;
}

void updatePaddle() {
  unsigned long now = millis();
  bool dit = digitalRead(PADDLE_DIT) == LOW;
  bool dah = digitalRead(PADDLE_DAH) == LOW;

  switch (elementPhase) {
    case EL_IDLE:
      if (dit && !dah) startElement('.');
      else if (dah && !dit) startElement('-');
      break;

    case EL_TONE: {
      unsigned long dur = activeElement == '.' ? dotMs : dotMs * 3UL;
      if (now - phaseStarted >= dur) {
        sidetone(false);
        appendElement(activeElement);
        phaseStarted = now;
        elementPhase = EL_GAP;
      }
      break;
    }

    case EL_GAP:
      if (now - phaseStarted >= (unsigned long)dotMs) {
        bool repeat =
          (activeElement == '.' && dit && !dah) ||
          (activeElement == '-' && dah && !dit);

        if (repeat) startElement(activeElement);
        else elementPhase = EL_IDLE;
      }
      break;
  }
}

void updateStraightKey() {
  bool down = digitalRead(STRAIGHT_KEY) == LOW;
  unsigned long now = millis();

  if (down && !straightWasDown) {
    straightWasDown = true;
    straightDownAt = now;
    sidetone(true);
  }

  if (!down && straightWasDown) {
    straightWasDown = false;
    sidetone(false);

    unsigned long held = now - straightDownAt;
    // threshold around 2 dot lengths: short = dit, long = dah
    appendElement(held < (unsigned long)(dotMs * 2) ? '.' : '-');
  }
}

void decodeIfComplete() {
  if (currentMorse.length() == 0) return;
  if (elementPhase != EL_IDLE || straightWasDown) return;

  if (millis() - lastElementFinished >= (unsigned long)(dotMs * 3)) {
    char c = decodeMorse(currentMorse);
    currentMorse = "";

    if (decodedLine.length() >= 14) decodedLine.remove(0, 1);
    decodedLine += c;

    if (screen == SCR_FREE_KEY) {
      redrawFreeInput();
      redrawDecoded();
    }
  }
}

// ============================================================
// ENCODER
// ============================================================

int encoderStep() {
  long c = encoder.getCount();
  long d = c - lastEncoderCount;
  if (abs(d) < 2) return 0;
  lastEncoderCount = c;
  return d > 0 ? 1 : -1;
}

void resetEncoder() {
  encoder.setCount(0);
  lastEncoderCount = 0;
}

void handleRotation() {
  int d = encoderStep();
  if (!d) return;

  if (screen == SCR_MAIN) {
    menuIndex = (menuIndex + d + MAIN_COUNT) % MAIN_COUNT;
    drawMain();
  } else if (screen == SCR_SETTINGS) {
    if (setIndex == 0) {
      settings.wpm = constrain(settings.wpm + d, 5, 50);
      dotMs = 1200 / settings.wpm;
    } else if (setIndex == 1) {
      settings.toneHz = constrain(settings.toneHz + d * 10, 300, 1200);
    } else {
      setIndex = (setIndex + d + SET_COUNT) % SET_COUNT;
    }
    drawSettings();
  }
}

void shortPress() {
  if (screen == SCR_MAIN) {
    switch (menuIndex) {
      case 0: drawFree(); break;
      case 1:
        drawFree();
        centered("Gebruik M1-M4", 48, 2, C_ORANGE);
        break;
      case 2: drawSettings(); break;
      case 3: drawMemoryCal(); break;
      case 4: drawCatStatus(); break;
    }
    resetEncoder();
  } else if (screen == SCR_SETTINGS) {
    if (setIndex == 0 || setIndex == 1) {
      setIndex++;
      if (setIndex >= SET_COUNT) setIndex = 0;
    } else if (setIndex == 2) {
      settings.pttOnMemory = !settings.pttOnMemory;
    } else {
      saveSettings();
      menuIndex = 2;
      drawMain();
    }
    saveSettings();
    drawSettings();
    resetEncoder();
  } else if (screen == SCR_FREE_KEY) {
    currentMorse = "";
    decodedLine = "";
    drawFree();
  }
}

void longPress() {
  sidetone(false);
  setPTT(false);
  elementPhase = EL_IDLE;
  currentMorse = "";
  screen = SCR_MAIN;
  drawMain();
  resetEncoder();
}

void updateEncoderButton() {
  bool s = digitalRead(ENC_SW);

  if (lastEncButton == HIGH && s == LOW) {
    encPressStarted = millis();
    encLongHandled = false;
  }

  if (s == LOW && !encLongHandled &&
      millis() - encPressStarted >= 900) {
    encLongHandled = true;
    longPress();
  }

  if (lastEncButton == LOW && s == HIGH && !encLongHandled) {
    shortPress();
  }

  lastEncButton = s;
}

// ============================================================
// MEMORY BUTTON UPDATE
// ============================================================

void updateMemoryButtons() {
  int b = readMemoryButton();

  if (b != lastMemButton) {
    memLastChange = millis();
    lastMemButton = b;
  }

  if (b >= 0 && millis() - memLastChange > 35) {
    static int fired = -1;
    if (fired != b) {
      fired = b;
      triggerMemory(b);
    }
  }

  if (b < 0) {
    static int dummy = 0;
    (void)dummy;
    // Separate static in a block cannot reset the one above,
    // so use the global-style latch below through a helper.
  }
}

// Simpler release latch for memory buttons
int memoryFired = -1;

void updateMemoryButtonsStable() {
  int b = readMemoryButton();

  if (b >= 0 && memoryFired < 0) {
    delay(25);
    if (readMemoryButton() == b) {
      memoryFired = b;
      triggerMemory(b);
    }
  }

  if (b < 0) memoryFired = -1;
}

// ============================================================
// CAT PASS-THROUGH / DEBUG
// ============================================================

void updateCatBridge() {
  // Raw 3.3 V UART on GPIO21/33.
  // This does NOT convert to RS-232.
  while (CAT.available()) {
    uint8_t c = CAT.read();
    Serial.write(c);
  }

  // Optional debug-to-radio bridge:
  // type characters in Serial Monitor to send them to CAT.
  while (Serial.available()) {
    CAT.write((uint8_t)Serial.read());
  }
}

// ============================================================
// SETUP / LOOP
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  pinMode(ENC_SW, INPUT_PULLUP);

  // GPIO35 and GPIO34 are input-only and have no internal pull-up.
  // The PCB provides the required external biasing.
  pinMode(PADDLE_DIT, INPUT);
  pinMode(BUTTON_ADC, INPUT);

  pinMode(PADDLE_DAH, INPUT_PULLUP);
  pinMode(STRAIGHT_KEY, INPUT_PULLUP);

  pinMode(KEY_CTRL, OUTPUT);
  pinMode(PTT_CTRL, OUTPUT);
  pinMode(AUDIO_PIN, OUTPUT);

  keyRadio(false);
  setPTT(false);
  noTone(AUDIO_PIN);

  loadSettings();

  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  encoder.attachHalfQuad(ENC_A, ENC_B);
  resetEncoder();

  SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
  tft.init(240, 320);
  tft.setRotation(1);
  tft.setTextWrap(false);

  // CAT raw UART. Start with 38400; change per radio model/menu later.
  CAT.begin(38400, SERIAL_8N1, CAT_RX, CAT_TX);

  tft.fillScreen(C_BLACK);
  centered("CW_KEYER_V2", 86, 4, C_ORANGE);
  centered("ON2DK", 142, 2, C_WHITE);
  delay(900);

  drawMain();
}

void loop() {
  updateEncoderButton();
  handleRotation();
  updateCatBridge();

  if (screen == SCR_MEMORY_CAL) {
    updateMemoryCal();
  }

  // Memory buttons remain active on all normal screens.
  if (screen != SCR_MEMORY_CAL) {
    updateMemoryButtonsStable();
  }

  if (screen == SCR_FREE_KEY) {
    updatePaddle();
    updateStraightKey();
    decodeIfComplete();
  }

  delay(1);
}
