#ifndef SERIAL_CMD_H
#define SERIAL_CMD_H

#include <Arduino.h>

// ── Serial Commands ───────────────────────────────────────────────
void handleSerialCommand(String cmd);
void printStatus();

#endif