/*
    sdcc --code-loc 0x180 --data-loc 0 -mz80 --disable-warning 85 --disable-warning 196 
         --no-std-crt0 crt0_msxdos_advanced.rel serial_slow.rel serial57k.rel jget.c

    hex2bin -e com jget.ihx (or: objcopy -I ihex -O binary jget.ihx jget.com)
*/

#include <stdlib.h>
#include "files.h"
#include "types.h"
#include "serial.h"
#include "msx_constants.h"
#include "printf.h"

const char* strTitle=
    "Serial via joystick port file receiver v1.0\r\n"
    "By Konamiman, 9/2024\r\n"
    "\r\n";
    
const char* strUsage=
    "Usage: jget <speed>\r\n"
    "\r\n"
    "Speed:\r\n"
    "0 = 2400 BPS, 1 = 4800 BPS, 2 = 9600 BPS, 3 = 19200 BPS, 4 = 57600 BPS";
    
const char* strInvParam = "Invalid parameter";
const char* strCRLF = "\r\n";

byte fileHandle = 0;
byte result;
uint crc;
ulong remaining;
uint chunkSize;
bool is57k;

struct {
    char fileName[13];
    ulong fileSize;
    uint crc;
} header;

#define BUFFER ((byte*)0x8000)
#define MAX_CHUNK_SIZE 1024

#define SerialReceive(address, length) (is57k ? SerialReceive57k(address, length) : SerialReceiveSlow(address, length))
#define SerialSend(address, length)  { if(is57k) SerialSend57k(address, length); else SerialSendSlow(address, length); }
#define SerialSendByte(value) { BUFFER[0]=value; BUFFER[1]=value; BUFFER[2]=value; BUFFER[3]=value; SerialSend(BUFFER,4); }

bool IsDos2();
void Terminate(byte errorCode);
void TerminateCore(byte errorCode);
uint crc16(byte* data_p, uint length);

int main(char** argv, int argc) {
    printf(strTitle);

    if(!IsDos2()) {
        printf("*** This program requires MSX-DOS 2\r\n");
        return 0;
    }

    if(argc != 1) {
        printf(strUsage);
        return 0;
    }

    if(argv[0][0] == '4') {
        is57k = true;
    }
    else {
        is57k = false;
        SerialSetSpeedSlow(argv[0][0] - '0');
    }

    printf("Connecting... ");
    
    result = SerialReceive((byte*)&header, (uint)sizeof(header));
    if(result == 1) {
        printf("\r\n*** RS232 line is not high");
        return 1;
    }
    if(result == 2) {
        printf("\r\n*** Data reception timeout");
        return 2;
    }

    crc = crc16((byte*)&header, (uint)(sizeof(header)-2));
    if(crc != header.crc) {
        printf("\r\n*** Header CRC mismtach. Received: 0x%x. Calculated: 0x%x.", header.crc, crc);
        SerialSendByte(3);
        return 3;
    }

    printf("\r\nFile name: %s\r\n", header.fileName);
    printf("File size: %lu\r\n\r\n", header.fileSize);

    printf(".=1KByte, !=CRC error\r\n");
    printf("Receiving: ");
    remaining = header.fileSize;

    fileHandle = CreateFile(header.fileName);

    SerialSendByte(0); //Send the header confirmation only after we are ready to get data.

    chunkSize = remaining > MAX_CHUNK_SIZE ? MAX_CHUNK_SIZE : remaining;
    while(remaining > 0) {
        result = SerialReceive(BUFFER, chunkSize+2);
        if(result == 1) {
            printf("\r\n*** RS232 line is not high");
            Terminate(1);
        }
        if(result == 2) {
            printf("\r\n*** Data reception timeout");
            Terminate(2);
        }

        crc = crc16(BUFFER, chunkSize);
        if(crc != *(uint*)(BUFFER+chunkSize)) {
            //printf("\r\n*** Data CRC mismtach. Received: 0x%x. Calculated: 0x%x.", header.crc, crc);
            printf("!");
            SerialSendByte(1);
            continue;
        }
        
        result = WriteToFile(BUFFER, chunkSize);
        if(result != 0) {
            SerialSendByte(result);
            Terminate(result);
        }

        SerialSendByte(0);
        printf(".");

        remaining -= chunkSize;
        chunkSize = remaining > MAX_CHUNK_SIZE ? MAX_CHUNK_SIZE : remaining;
    }

    printf("\r\nDone!");
    return 0;
}

bool IsDos2() __naked
{
    __asm

    ld c,#_DOSVER
    call #DOS
    
    ld a,#0
    ret nz

    ld a,b
    cp #2
    ld a,#0
    ret c

    cpl
    ret

    __endasm;
}

void Terminate(byte errorCode)
{
    if(fileHandle != 0) {
        CloseFile(fileHandle);
    }
    printf("\r\n");
    TerminateCore(errorCode);
}

void TerminateCore(byte errorCode) __naked
{
    __asm

    ld b,a
    ld c,#_TERM
    call #DOS

    ;Fallback for DOS 1
    ld c,#_TERM0
    call #DOS ;"jp #DOS" here fails with "Error: <a> machine specific addressing or addressing mode error" ???
    ret

    __endasm;
}


#define POLY 0x8408

/*
//                                      16   12   5
// this is the CCITT CRC 16 polynomial X  + X  + X  + 1.
// This works out to be 0x1021, but the way the algorithm works
// lets us use 0x8408 (the reverse of the bit pattern).  The high
// bit is always assumed to be set, thus we only use 16 bits to
// represent the 17 bit value.
*/
unsigned int crc16(byte* data_p, unsigned int length)
{
      unsigned char i;
      unsigned long data;
      unsigned long crc = 0xffff;

      if (length == 0)
            return (~crc);

      do
      {
            for (i=0, data=(unsigned long)0xff & *data_p++;
                 i < 8; 
                 i++, data >>= 1)
            {
                  if ((crc & 0x0001) ^ (data & 0x0001))
                        crc = (crc >> 1) ^ POLY;
                  else  crc >>= 1;
            }
      } while (--length);

      crc = ~crc;
      data = crc;
      crc = (crc << 8) | (data >> 8 & 0xff);

      return (crc);
}

#define SUPPORT_LONG
#define COM_FILE
#include "printf.c"
#include "files.c"
