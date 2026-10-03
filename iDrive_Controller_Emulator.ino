#include <SPI.h>
#include <mcp_can.h>

const int SPI_CS_CAN = 10;
MCP_CAN CAN(SPI_CS_CAN);

const uint32_t IDRIVE_ID = 0x25B;

uint8_t rotaryLoIdx = 1;
uint16_t rotaryPosition = 0x7FFF;
bool     rotaryPrimed   = false;
const uint16_t ROTARY_STEP_DELAY_MS = 10;

uint8_t idleFrame[8] = {0x00, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0xC0, 0xC0};

uint8_t frameCounter = 0x00;

const uint16_t STEP_DELAY_MS = 40;

struct ButtonCmd {
  const char* name;
  uint8_t byteIndex;
  uint8_t pressVal;
  uint8_t touchVal;
  uint8_t releaseVal;
  bool    hasTouch;
};

ButtonCmd buttons[] = {
  { "BACK",   4, 0x20, 0x80, 0x00, true  },
  { "HOME",   4, 0x04, 0x10, 0x00, true  },
  { "COM",    5, 0x08, 0x20, 0x00, true  },
  { "OPTION", 5, 0x01, 0x04, 0x00, true  },
  { "MEDIA",  6, 0xC1, 0xC4, 0xC0, true  },
  { "NAV",    6, 0xC8, 0xE0, 0xC0, true  },
  { "MAP",    7, 0xC1, 0xC4, 0xC0, true  },
  { "GLOBE",  7, 0xC8, 0xE0, 0xC0, true  },
  { "PUSH",   3, 0x01, 0x00, 0x00, false },
  { "LEFT",   3, 0xA0, 0x00, 0x00, false },
  { "UP",     3, 0x10, 0x00, 0x00, false },
  { "RIGHT",  3, 0x40, 0x00, 0x00, false },
  { "DOWN",   3, 0x70, 0x00, 0x00, false },
};
const uint8_t NUM_BUTTONS = sizeof(buttons) / sizeof(buttons[0]);

void canSendFrame(uint32_t id, uint8_t* data, uint8_t len) {
  byte sndStat = CAN.sendMsgBuf(id, 0, len, data);
  Serial.print("TX 0x");
  Serial.print(id, HEX);
  Serial.print(" ");
  for (uint8_t i = 0; i < len; i++) {
    if (data[i] < 0x10) Serial.print("0");
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.println(sndStat == CAN_OK ? "[OK]" : "[FAIL]");
}

void sendIdriveSubFrame(uint8_t byteIndex, uint8_t value) {
  uint8_t frame[8];
  memcpy(frame, idleFrame, 8);
  frame[0] = frameCounter++;
  // every frame carries the current rotary position otherwise the receiver sees a jump back to idle.
  frame[rotaryLoIdx]     = (uint8_t)(rotaryPosition & 0xFF);
  frame[rotaryLoIdx + 1] = (uint8_t)(rotaryPosition >> 8);
  frame[byteIndex] = value;
  canSendFrame(IDRIVE_ID, frame, 8);
}

void triggerButton(ButtonCmd &btn) {
  Serial.print(">> Triggering: ");
  Serial.println(btn.name);

  sendIdriveSubFrame(btn.byteIndex, btn.pressVal);
  delay(STEP_DELAY_MS);

  if (btn.hasTouch) {
    sendIdriveSubFrame(btn.byteIndex, btn.touchVal);
    delay(STEP_DELAY_MS);
  }

  sendIdriveSubFrame(btn.byteIndex, btn.releaseVal);
}

void sendRotaryFrame() {
  uint8_t frame[8];
  memcpy(frame, idleFrame, 8);
  frame[0] = frameCounter++;
  frame[rotaryLoIdx]     = (uint8_t)(rotaryPosition & 0xFF);
  frame[rotaryLoIdx + 1] = (uint8_t)(rotaryPosition >> 8);
  canSendFrame(IDRIVE_ID, frame, 8);
}

// direction: +1 = clockwise (scroll down), -1 = counter-clockwise (scroll up)
void rotateKnob(int8_t direction, uint16_t steps) {
  Serial.print(">> Rotary ");
  Serial.print(direction > 0 ? "DOWN x" : "UP x");
  Serial.println(steps);

  if (!rotaryPrimed) { 
    sendRotaryFrame();
    rotaryPrimed = true;
    delay(ROTARY_STEP_DELAY_MS);
  }
  for (uint16_t i = 0; i < steps; i++) {
    rotaryPosition = (uint16_t)(rotaryPosition + direction);
    sendRotaryFrame();
    delay(ROTARY_STEP_DELAY_MS);
  }
}

String inputLine = "";

bool hexNibble(char c, uint8_t &out) {
  if (c >= '0' && c <= '9') { out = c - '0'; return true; }
  if (c >= 'A' && c <= 'F') { out = c - 'A' + 10; return true; }
  if (c >= 'a' && c <= 'f') { out = c - 'a' + 10; return true; }
  return false;
}

int parseHexBytes(const String &hex, uint8_t* out, int maxBytes) {
  int len = hex.length();
  if (len % 2 != 0) return -1;
  int nBytes = len / 2;
  if (nBytes > maxBytes) return -1;
  for (int i = 0; i < nBytes; i++) {
    uint8_t hi, lo;
    if (!hexNibble(hex[i * 2], hi) || !hexNibble(hex[i * 2 + 1], lo)) return -1;
    out[i] = (hi << 4) | lo;
  }
  return nBytes;
}

void handleCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  String upper = cmd;
  upper.toUpperCase();

  if (upper.startsWith("SEND ")) {
    int firstSpace = cmd.indexOf(' ');
    int secondSpace = cmd.indexOf(' ', firstSpace + 1);
    if (secondSpace == -1) {
      Serial.println("Usage: SEND <hexID> <hexData 1-16 chars>");
      return;
    }
    String idStr = cmd.substring(firstSpace + 1, secondSpace);
    String dataStr = cmd.substring(secondSpace + 1);
    dataStr.trim();

    uint32_t id = strtoul(idStr.c_str(), NULL, 16);
    uint8_t data[8] = {0};
    int n = parseHexBytes(dataStr, data, 8);
    if (n < 0) {
      Serial.println("Bad hex data. Example: SEND 25B 03FF7F0000200000C0C0");
      return;
    }
    canSendFrame(id, data, n);
    return;
  }

  if (upper.startsWith("ROTBYTES")) {
    int sp = upper.indexOf(' ');
    long idx = (sp == -1) ? -1 : upper.substring(sp + 1).toInt();
    if (idx < 1 || idx > 6) { Serial.println("Usage: ROTBYTES <1-6>"); return; }
    rotaryLoIdx = idx;
    rotaryPosition = (idx == 1) ? 0x7FFF : 0x0000;
    rotaryPrimed = false;
    Serial.print("Rotary position now in bytes "); Serial.print(idx);
    Serial.print("-"); Serial.println(idx + 1);
    return;
  }

  if (upper.startsWith("SCROLLUP") || upper.startsWith("SCROLLDOWN")) {
    bool down = upper.startsWith("SCROLLDOWN");
    int sp = upper.indexOf(' ');
    long n = (sp == -1) ? 1 : upper.substring(sp + 1).toInt();
    if (n < 1) n = 1;
    if (n > 100) n = 100;
    rotateKnob(down ? 1 : -1, (uint16_t)n);
    return;
  }

  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    if (upper == buttons[i].name) {
      triggerButton(buttons[i]);
      return;
    }
  }

  Serial.print("Unknown command: ");
  Serial.println(cmd);
  Serial.println("Valid: BACK COM MEDIA HOME MAP NAV GLOBE OPTION LEFT RIGHT UP DOWN PUSH SCROLLUP SCROLLDOWN ROTBYTES SEND");
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("=== iDrive Controller Emulator ===");

  SPI.begin();
  pinMode(SPI_CS_CAN, OUTPUT);
  digitalWrite(SPI_CS_CAN, HIGH);

  Serial.print("Initializing CAN...");
  delay(10);
  if (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_16MHZ) != CAN_OK) {
    Serial.println(" ERROR ");
    while (1);
  }
  CAN.setMode(MCP_NORMAL);
  Serial.println(" OK ");

  Serial.println("Type any of these commands:");
  Serial.println("BACK COM MEDIA HOME MAP NAV GLOBE OPTION LEFT RIGHT UP DOWN PUSH");
  Serial.println("SCROLLUP / SCROLLDOWN / ROTBYTES <1-6>");
  Serial.println("or: SEND <hexID> <hexData>");
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      handleCommand(inputLine);
      inputLine = "";
    } else if (c != '\r') {
      inputLine += c;
    }
  }
}
