#ifndef __SERIAL_H
#define __SERIAL_H

#include "types.h"

#define SERIAL_SPEED_2400 0
#define SERIAL_SPEED_4800 1
#define SERIAL_SPEED_9600 2
#define SERIAL_SPEED_19200 3
#define SERIAL_SPEED_57600 4

void SerialSetSpeedSlow(byte speed);
byte SerialReceiveSlow(byte* address, int length);
void SerialSendSlow(byte* address, int length);
byte SerialReceive57k(byte* address, int length);
void SerialSend57k(byte* address, int length);

#endif
