#ifndef __MSX_CONSTANTS_H
#define __MSX_CONSTANTS_H

// MSX BIOS routines

#define ENASLT 0x0024


// MSX-DOS functions

#define DOS 0x0005

#define _TERM0 0
#define _CONOUT 0x02
#define _OPEN 0x43
#define _CREATE 0x44
#define _CLOSE 0x45
#define _READ 0x48
#define _WRITE 0x49
#define _SEEK 0x4A
#define _TERM 0x62
#define _DOSVER 0x6F


// MSX work area

#define H_CHPU 0xFDA4
#define CSRX 0x0F3DD
#define CSRY 0xF3DC
#define TTYPOS 0xF661
#define CSRSW 0xFCA9
#define H_ERAC 0xFDAE
#define CURSAV 0xFBCC
#define SCRMOD 0xFCAF
#define LINLEN 0xF3B0
#define NAMBAS 0xF922
#define H_DSPC 0xFDA9
#define CGPBAS 0xF924
#define LINWRK 0xFC18
#define CSTYLE 0xFCAA
#define MODE 0xFAFC
#define GRPHED 0xFCA6
#define ESCCNT 0xFCA7
#define CNSDFG 0xF3DE
#define CRTCNT 0xF3B1
#define LINTTB 0xFBB2
#define ACPAGE 0xFAF6
#define NEWKEY 0xFBE5
#define EXTBIO 0xFFCA
#define RAMAD0 0xF341
#define RAMAD1 0xF342
#define RAMAD2 0xF343
#define SECBUF 0xF34D


/* Misc */

#define KEY_ENTER (1<<7)
#define KEY_ESC (1<<2)
#define MSXDOS_EOF 0xC7

#endif