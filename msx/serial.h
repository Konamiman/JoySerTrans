#ifndef __SERIAL_H
#define __SERIAL_H

#include "types.h"

#define SERIAL_SPEED_2400 0
#define SERIAL_SPEED_4800 1
#define SERIAL_SPEED_9600 2
#define SERIAL_SPEED_19200 3

byte SerialReceive(byte* address, int length);
void SerialSend(byte* address, int length);
void SerialSetSpeed(byte speed);

#endif
