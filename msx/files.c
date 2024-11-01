
//#define FIB_BUFFER 0xC000

byte CreateFile(char* fileName) __naked
{
    __asm

    ;HL = *fileName

    ex de,hl
    ld a,#2 ;Open in write-only mode
    ld bc,#_CREATE  ;B=0 (no attributes, "crete new" not set)
    call #DOS

    ld c,a ;Error code
    ld a,b ;File handle
    ret z

    ld a,c
    jp _Terminate

    __endasm;
}

    byte FIB_BUFFER[64];

byte CreateFileAndGetPath(char* fileName, char* filePath) __naked
{
    __asm

    ;HL = *fileName
    ;DE = *filePath

    push de
    ex de,hl
    ld bc,#_FNEW ;B=0 (file attributes)
    push ix
    ld ix,#_FIB_BUFFER
    call #DOS
    pop ix
    pop de
    jp nz,_Terminate

    ld a,(#_FIB_BUFFER+25)
    dec a
    add #65  ;'A'
    ld (de),a
    inc de
    ld a,#58 ;':'
    ld (de),a
    inc de
    ld a,#92 ;\
    ld (de),a
    inc de

    ld c,#_WPATH
    call #DOS
    jp nz,_Terminate

    ld de,#_FIB_BUFFER
    xor a
    ld c,#_OPEN
    call #DOS
    jp nz,_Terminate

    ld a,b
    ret

    __endasm;
}


byte WriteToFile(byte* address, uint size) __naked
{
    __asm

    ;HL = address, DE = size

    ld a,(_fileHandle)
    ld b,a
    ex de,hl
    ld c,#_WRITE
    call #DOS
    ret

    __endasm;
}


void CloseFile(byte fileHandle) __naked
{
    __asm

    ;A = fileHandle

    ld b,a
    ld c,#_CLOSE
    call #DOS
    ret

    __endasm;
}


//Returns number of bytes read
int ReadFromFile(byte fileHandle, uint address) __naked
{
    __asm

    ;A = fileHandle, DE = address

    ;Fill page with 0xFF first
    push af
    push de
    ld h,d
    ld l,e
    inc de
    ld (hl),#0xFF
    ld bc,#16384-1
    ldir
    pop de
    pop af

    ld b,a
    ld hl,#0x4000  ;Size
    ld c,#_READ
    call #DOS
    ex de,hl  ;Number of bytes read to DE
    or a
    ret z     ;Returns number of bytes read in DE
    ld c,a    ;Error code
    cp #MSXDOS_EOF   ;Treat "End of file" as no error
    ld a,#0
    ret z
    ld a,c
    jp _Terminate

    __endasm;
}


byte OpenFile(char* fileName) __naked
{
    __asm

    ;HL = *fileName

    ex de,hl
    ld a,#1 ;Open in read-only mode
    ld c,#_OPEN
    call #DOS

    ld c,a ;Error code
    ld a,b ;File handle
    ret z

    ld a,c
    jp _Terminate

    __endasm;
}


bool FileIsEmpty(byte fileHandle) __naked
{
    __asm

    ;A = fileHandle

    ld b,a
    ld a,#2  ;A = Seek method: from end of file
    ld hl,#0  ;HLDE = Seek offset: 0
    ld de,#0
    ld c,#_SEEK
    push bc
    call #DOS
    pop bc
    jp nz,_Terminate

    ;HLDE = 0 if file is empty
    ld a,h
    or l
    or d
    or e
    ld a,#0xFF
    ret z

    ;Rewind file
    xor a   ;A = Seek method: from beginning of file
    ld hl,#0
    ld de,#0
    ld c,#_SEEK
    call #DOS
    xor a
    ret

    __endasm;
}