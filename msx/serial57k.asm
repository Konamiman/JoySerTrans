;Routines to transfer data via the joystick port
;using the standard RS232 serial protocol (57600 bauds).
;
;Requires a Z80 clock speed of 3.57 MHz.
;See README.md for pinout and protocol.
;
;Based on https://github.com/rolandvans/msx_softserial
;
;Use Nestor80 to build:
;N80 serial57k.asm -ofe rel

    area _CODE  ;To force SDAS output format

;----------------------------------------------------------

;--- Select joystick port 2 (default is port 1)
;
;    C signature:
;    void SelectPort2_57k()

_SelectPort2_57k::
	;Change "RES 6,A" into "SET 6,A"
	ld a,#F7
	ld (_SelectJoyPort57k1+1),a
	ld (_SelectJoyPort57k2+1),a
	ld (_SelectJoyPort57k3+1),a

	ld a,#97
	ld (_SetDataLineTo0_57k+1),a	;Change "RES 0,A" to "RES 2,A"
	ld a,#D7
	ld (_SetDataLineTo1_57k+1),a	;Change "SET 0,A" to "SET 2,A"

	ld a,#EF
	ld (_SetRTSto1_57k1+1),a		;Change "SET 4,A" to "SET 5,A"
	ld (_SetRTSto1_57k2+1),a
	ld a,#AF
	ld (_SetRTSto0_57k1+1),a		;Change "RES 4,A" to "RES 5,A"
	ld (_SetRTSto0_57k2+1),a

	ret

;----------------------------------------------------------

;--- Receive data
;    Input:  HL = Destination address
;            DE = Length
;    Output: A = Error code:
;                0: Ok
;                1: RTS timeout
;                2: Start bit timeout
;                3: Stop bit error
;
;    C signature:
;    unsigned char SerialReceive57k(unsigned char* address, int length)

_SerialReceive57k::
	LD A,E	; FAST LOOP WITH BC (GRAUW, FAST LOOPS)
	DEC DE	; COMPENSATION FOR THE CASE C=0
	INC D
	LD E,D	; SWAP B AND C FOR LOOP STRUCTURE
	LD D,A

	DI			;NO INTERRUPTS, TIME CRITICAL ROUTINE

	LD A,#0F	;PSG REGISTER 15, SELECT JOYSTICK PORT 2
	OUT (#A0),A
	IN A,(#A2)
_SelectJoyPort57k1:
	res 6,A		;SELECT JOY1
_SetRTSto0_57k1:
    res	4,a	;Unset pin 8 for now (our RTS, CTS of peer) - we're not ready to receive data just yet
	OUT (#A1),A

	LD A,#0E	;SET PSG #14
	OUT (#A0),A

    ld bc,#0099

	;Wait for the peer to set its RTS (thus signaling it wants to send data)

_WAIT_RTS:	
	in	a,(#a2)
	and	2
	jp	z,_SETRTS

	in	f,(c)
	jp	p,_WAIT_RTS

	in	a,(#a2)
	and	2
	jp	z,_SETRTS

	djnz	_WAIT_RTS
	ld	a,1
	scf
	ei
	ret

_SETRTS:
	LD	A,#0F
	OUT	(#A0),A
	IN	A,(#A2)
_SetRTSto1_57k1:
	set 4,a	;Now set our RTS (signal CTS to peer) - we are ready to receive data
	OUT	(#A1),A

	LD	A,#0E			;SET PSG #14
	OUT	(#A0),A

    ld b,d
    ld c,e

	LD E,#01	;FOR FASTER 'AND' OPERATION 'AND r'(5) VS. 'AND n'(8)
;THE NEXT PART IS TIME CRITICAL. EVERY CYCLE COUNTS
.FIRSTSTARTBIT:		;WAIT FOR FIRST STARTBIT
	IN A,(#A2)
	AND E
	JP NZ,.FIRSTSTARTBIT
	JP .READFIRSTBIT	;START READING BIT0, COMPENSATED FOR 12 CYCLES, 1 JP
;.STARTBIT:	

	if 1
	IN A,(#A2)	;WAIT FOR THE HIGH->LOW TRANSITION (STARTBIT)
	AND E
	JP Z,.READBITS
	IN A,(#A2)	;WAIT FOR THE HIGH->LOW TRANSITION (STARTBIT)
	AND E
	JP Z,.READBITS
	IN A,(#A2)	;WAIT FOR THE HIGH->LOW TRANSITION (STARTBIT)
	AND E
	JP Z,.READBITS
	IN A,(#A2)	;WAIT FOR THE HIGH->LOW TRANSITION (STARTBIT)
	AND E
	JP Z,.READBITS
	else
	call .WAITSTARTBIT
	jp z,.READBITS
	endif

	LD A,#02	;ERROR, STARTBIT TIMEOUT
	SCF
	EI
	RET
.READBITSNEXTBLOCK:
	INC HL
	DEC C	
	JR NZ,.INITBITLOOP	;NEXT BLOCK UNLESS WE ARE DONE (C=0)
	JR .EXIT
.READBITSNEXTBYTE:
	INC HL 
	NOP	;DUMMY
	JR .INITBITLOOP
.READBITS:
	IN A,(#A2)	;DUMMY
.READFIRSTBIT:	;ALT TIMING TO COMPENSATE FOR 1 JP
	ADD A,#00	;DUMMY
	NOP	;DUMMY
.INITBITLOOP:
	LD D,B
	LD B,#08
.BITLOOP:	
	INC HL	;DUMMY
	DEC HL	;DUMMY
	IN A,(#A2)
	RRCA		;SHIFT DATA BIT (0) -> CARRY
	RR (HL)		;SHIFT CARRY -> [HL]
	DJNZ .BITLOOP
	LD B,D
	DJNZ .STARTBITNEXTBYTE	;NEXT BYTE, SKIP STOPBIT AND WAIT FOR STARTBIT
	NOP	;DUMMY
.STARTBITNEXTBLOCK:
	if 1
	IN A,(#A2)	;WAIT FOR THE HIGH->LOW TRANSITION (STARTBIT)
	AND E
	JP Z,.READBITSNEXTBLOCK
	IN A,(#A2)	;WAIT FOR THE HIGH->LOW TRANSITION (STARTBIT)
	AND E
	JP Z,.READBITSNEXTBLOCK
	IN A,(#A2)	;WAIT FOR THE HIGH->LOW TRANSITION (STARTBIT)
	AND E
	JP Z,.READBITSNEXTBLOCK
	IN A,(#A2)	;WAIT FOR THE HIGH->LOW TRANSITION (STARTBIT)
	AND E
	JP Z,.READBITSNEXTBLOCK
	else
	call .WAITSTARTBIT
	jp z,.READBITSNEXTBLOCK
	endif

	DEC C	;POSTPONED CHECK
	JR Z,.EXIT	;IF C=0, WE ARE DONE SO EXIT OTHERWISE ERROR
	LD A,#02 ;ERROR STARTBIT TIMEOUT
	SCF
	EI
	RET
.STARTBITNEXTBYTE:
	IN A,(#A2)	;WAIT FOR THE HIGH->LOW TRANSITION (STARTBIT)
	AND E
	JP Z,.READBITSNEXTBYTE
	IN A,(#A2)	;WAIT FOR THE HIGH->LOW TRANSITION (STARTBIT)
	AND E
	JP Z,.READBITSNEXTBYTE
	IN A,(#A2)	;WAIT FOR THE HIGH->LOW TRANSITION (STARTBIT)
	AND E
	JP Z,.READBITSNEXTBYTE
	IN A,(#A2)	;WAIT FOR THE HIGH->LOW TRANSITION (STARTBIT)
	AND E
	JP Z,.READBITSNEXTBYTE
	else
	call .WAITSTARTBIT
	jp z,.READBITSNEXTBYTE

	LD A,#02
	SCF
	EI
	RET
.EXIT:
	XOR A	;RESET CARRY
	EI
	RET

.WAITSTARTBIT:
	push bc
	ld b,0
.WSB:
	IN A,(#A2)	;WAIT FOR THE HIGH->LOW TRANSITION (STARTBIT)
	AND E
	JR Z,.GOTSTARTBIT
	djnz .WSB
	or a
	pop bc
	ret

.GOTSTARTBIT:
	pop bc
	ret

;----------------------------------------------------------

;--- Send data
;    Input:  HL = Source address
;            DE = Length
;    Output: A = Error code:
;                0: Ok
;                1: CTS timeout
;
;    C signature:
;    unsigned char SerialSend57k(unsigned char* address, int length)

_SerialSend57k::
	DI	;NO INTERRUPTS

	LD A,#0F	;SELECT PSG REG #15
	OUT (#A0),A

	IN A,(#A2)	;SAVE VALUE
_SelectJoyPort57k2:
	res 6,A		;JOY1
_SetRTSto1_57k2:
    set	4,A	;Set our RTS (signals we want to send data)
    out (#A1),a

    LD	A,#0E	;SET PSG #14
	OUT	(#A0),A

	LD	BC,#0099

	;Wait for the peer to set its RTS (interpreted by us as CTS, we're good to send data)

_WAIT_CTS:	
	in	a,(#a2)
	and	2
	jp	z,_DOSEND

	in	f,(c)
	jp	p,_WAIT_CTS

	in	a,(#a2)
	and	2
	jp	z,_DOSEND

	djnz	_WAIT_CTS

	ld	a,1
	scf
	ei
	ret

_DOSEND:
	ld	a,#0F
	out	(#A0),a
	in	a,(#A2)
	push af
_SelectJoyPort57k3:	
    res 6,A		;JOY1
_SetRTSto0_57k2:
	res	4,a	;Clear our RTS
	out	(#A1),a

	ld	b,d
	ld	c,e

_SetDataLineTo0_57k:
	RES 0,A		;TRIG1 LOW
	LD E,A		;0V VALUE (0) IN E
_SetDataLineTo1_57k:
	SET 0,A		;TRIG1 HIGH
	LD D,A		;5V VALUE (1) IN D
.BYTELOOP:	
	PUSH BC
	LD A,E
;.STARTBIT:	
	OUT (#A1),A
	LD C,(HL)
	LD B,#08
.BITLOOP2:	
	RRC C
;ASSUME BIT=1		
	LD A,D
	JR C,.SETBIT
;NO, BIT=0		
	LD A,E
.SETBIT:	
	ADD A,#00	;DUMMY
	OUT (#A1),A
	DJNZ .BITLOOP2
	LD A,E
	POP BC
	DEC BC
	NOP	;DUMMY
	ADD A,#00	;DUMMY
.STOPBIT:	
	LD A,D
	OUT (#A1),A
	INC HL
	LD A,B
	OR C
	NOP	;DUMMY
	JP NZ,.BYTELOOP
;.EXIT:
	NOP	;DUMMY
	POP AF
	OUT (#A1),A
    xor a
	EI
	RET
END:
