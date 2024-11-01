/*
    File receiver via joystick port v1.0
    By Konamiman, 11/2024

    See README.md for the joystick port pinout and the protocol.

    Use SDCC to build (see also serial_slow.asm and serial57k.asm):

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
    "File receiver via joystick port v1.0\r\n"
    "By Konamiman, 11/2024\r\n"
    "\r\n";
    
const char* strUsage=
    "Usage: jget <port> <speed> [<file path>]\r\n"
    "\r\n"
    "<port>: 1 or 2\r\n"
    "<speed> (BPS): 0 = 2400, 1 = 4800, 2 = 9600, 3 = 19200, 4 = 57600\r\n"
    "<file path>: if omitted, received file name in current directory\r\n"
    "\r\n"
    "Joystick port pinout and protocol:\r\n"
    "https://github.com/Konamiman/JoySerTrans";
    
const char* strInvParam = "Invalid parameter";
const char* strCRLF = "\r\n";

#define BUFFER ((byte*)0x8000)
#define MAX_CHUNK_SIZE 1024
#define CPU_Z80 0

byte fileHandle = 0;
byte result;
uint calculatedCrc;
ulong remaining;
uint chunkSize;
bool is57k;
bool isPort2;
char* filePath = 0;
byte currentCpu = CPU_Z80;

struct {
    char fileName[13];
    ulong fileSize;
    uint crc;
} header;

#define SerialSend(address, length) (is57k ? SerialSend57k(address, length) : SerialSendSlow(address, length))

bool IsDos2();
void Terminate(byte errorCode);
void TerminateCore(byte errorCode);
uint crc16(byte* data_p, uint length);
void SerialSendByte(byte value, bool printOnError);
void ProcessReceiveError(byte value);
byte SerialReceive(byte* address, int length);
byte GetCpu();
void SetCpu(byte cpu);

int main(char** argv, int argc) {
    printf(strTitle);

    if(!IsDos2()) {
        printf("*** This program requires MSX-DOS 2\r\n");
        return 0;
    }

    if(argc < 2) {
        printf(strUsage);
        return 0;
    }

    if(argv[1][0] == '4') {
        is57k = true;
    }
    else {
        is57k = false;
        SerialSetSpeedSlow(argv[1][0] - '0');
    }

    if(argv[0][0] == '2') {
        if(is57k) {
            SelectPort2_57k();
        }
        else {
            SelectPort2Slow();
        }
    }

    if(argc > 2) {
        fileHandle = CreateFileAndGetPath(argv[2], BUFFER);
        printf("File path: %s\r\n\r\n", BUFFER);
    }

    currentCpu = GetCpu();
    printf("Connecting... ");
    
    result = SerialReceive((byte*)&header, (uint)sizeof(header));
    ProcessReceiveError(result);

    calculatedCrc = crc16((byte*)&header, (uint)(sizeof(header)-2));
    if(calculatedCrc != header.crc) {
        printf("\r\n*** Header CRC mismtach. Received: 0x%x. Calculated: 0x%x.", header.crc, calculatedCrc);
        SerialSendByte(2, false);
        return 5;
    }

    printf("\r\nFile name: %s\r\n", header.fileName);
    printf("File size: %lu\r\n\r\n", header.fileSize);

    printf(".=1KByte, !=CRC error\r\n");
    printf("Receiving: ");
    remaining = header.fileSize;

    if(fileHandle == 0) {
        fileHandle = CreateFile(header.fileName);
    }

    SerialSendByte(0, true); //Send the header confirmation only after we are ready to get data.

    chunkSize = remaining > MAX_CHUNK_SIZE ? MAX_CHUNK_SIZE : remaining;
    while(remaining > 0) {
        result = SerialReceive(BUFFER, chunkSize+2);
        ProcessReceiveError(result);

        calculatedCrc = crc16(BUFFER, chunkSize);
        if(calculatedCrc != *(uint*)(BUFFER+chunkSize)) {
            //printf("\r\n*** Data CRC mismtach. Received: 0x%x. Calculated: 0x%x.", header.crc, crc);
            printf("!");
            SerialSendByte(1, true);
            continue;
        }
        
        result = WriteToFile(BUFFER, chunkSize);
        if(result != 0) {
            SerialSendByte(result, false);
            Terminate(result);
        }

        SerialSendByte(0, true);
        printf(".");

        remaining -= chunkSize;
        chunkSize = remaining > MAX_CHUNK_SIZE ? MAX_CHUNK_SIZE : remaining;
    }

    printf("\r\nDone!");
    Terminate(0);
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
    if(currentCpu != CPU_Z80) {
        SetCpu(currentCpu);
    }
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

void SerialSendByte(byte value, bool printOnError)
{
    if(currentCpu != CPU_Z80) {
        SetCpu(CPU_Z80);
    }

    BUFFER[0]=value;
    BUFFER[1]=value;
    BUFFER[2]=value;
    BUFFER[3]=value;
    result=SerialSend(BUFFER,4);

    if(currentCpu != CPU_Z80) {
        SetCpu(currentCpu);
    }

    if(result != 0) {
        if(printOnError) {
            printf("\r\n*** CTS line timeout", result);
        }
        Terminate(5);
    }
}

void ProcessReceiveError(byte value)
{
    if(result == 1) {
        printf("\r\n*** RTS line timeout");
        Terminate(1);
    }
    if(result == 2) {
        printf("\r\n*** Data reception timeout");
        Terminate(2);
    }
    if(result == 3) {
        printf("\r\n*** Stop bit error");
        Terminate(3);
    }
    if(result != 0) {
        printf("\r\n*** Unexpected error: %i", result);
        Terminate(4);
    }
}

uint crc16(byte* data_p, uint length) __naked
{
    //XMODEM CRC calculation
    //https://mdfs.net/Info/Comp/Comms/CRC16.htm
    //"The XMODEM CRC is CRC-16 with a start value of &0000, the end value is not XORed, and uses a polynoimic of 0x1021."

    __asm

    ld b,d
    ld c,e
    ld de,#0

bytelp:
    PUSH BC
    LD A,(HL)         ; Save count, fetch byte from memory

; The following code updates the CRC in DE with the byte in A ---+
    XOR D                     ; XOR byte into CRC top byte
    LD B,#8                   ; Prepare to rotate 8 bits

rotlp:
    SLA E
    ADC A,A             ; Rotate CRC
    JP NC,clear         ; b15 was zero
    LD D,A              ; Put CRC high byte back into D
    LD A,E
    XOR #0x21
    LD E,A              ; CRC=CRC XOR &1021, XMODEM polynomic
    LD A,D
    XOR #0x10           ; And get CRC top byte back into A
clear:
    DEC B               ; Decrement bit counter
    JP NZ,rotlp         ; Loop for 8 bits
    LD D,A              ; Put CRC top byte back into D
; ---------------------------------------------------------------+

    INC HL
    POP BC             ; Step to next byte, get count back
    DEC BC             ; num=num-1
    LD A,B
    OR C
    JP NZ,bytelp  ; Loop until num=0
    RET

    __endasm;
}

byte SerialReceive(byte* address, int length) 
{
    if(currentCpu != CPU_Z80) {
        SetCpu(CPU_Z80);
    }

    result = is57k ? SerialReceive57k(address, length) : SerialReceiveSlow(address, length);

    if(currentCpu != CPU_Z80) {
        SetCpu(currentCpu);
    }

    return result;
}

byte GetCpu() __naked
{
    __asm

    ld a,(#EXPTBL)
    ld hl,#CHGCPU
    call RDSLT
    cp #0xC3 ;"JP", so routine is available
    ld a,#0
    ret nz   ;No CHGCPU routine? Return "Z80" then

    push ix
    push iy
    ld iy,(#EXPTBL-1)
    ld ix,#GETCPU
    call CALSLT
    pop iy
    pop ix
    ret ;Return current CPU in A

    __endasm;
}

void SetCpu(byte cpu) __naked
{
    __asm

    ;A = cpu

    and #3
    jr z,_SETCPU_GO ;Prevent turbo led from blinking on-off
    or #128 ;Set turbo led
_SETCPU_GO:
    push ix
    push iy
    ld iy,(#EXPTBL-1)
    ld ix,#CHGCPU
    call CALSLT
    pop iy
    pop ix
    ret

    __endasm;
}

#define SUPPORT_LONG
#define COM_FILE
#include "printf.c"
#include "files.c"
