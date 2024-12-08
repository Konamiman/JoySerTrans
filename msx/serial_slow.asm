;Routines to transfer data via the joystick port
;using the standard RS232 serial protocol (2400-19200 bauds).
;
;Requires a Z80 clock speed of 3.57 MHz.
;See README.md for pinout and protocol.
;
;Based on https://github.com/rolandvans/msx_softserial
;
;Use Nestor80 to build:
;N80 serial_slow.asm

    area _CODE  ;To force SDAS output format

;----------------------------------------------------------

;--- Select joystick port 2 (default is port 1)
;
;    C signature:
;    void SelectPort2Slow()

_SelectPort2Slow::
	;Change "RES 6,A" into "SET 6,A"
	ld a,#F7
	ld (_SelectJoyPortSlow1+1),a
	ld (_SelectJoyPortSlow2+1),a
	ld (_SelectJoyPortSlow3+1),a

	ld a,#97
	ld (_SetDataLineTo0Slow+1),a	;Change "RES 0,A" to "RES 2,A"
	ld a,#D7
	ld (_SetDataLineTo1Slow+1),a	;Change "SET 0,A" to "SET 2,A"

	ld a,#EF
	ld (_SetRTSto1Slow1+1),a		;Change "SET 4,A" to "SET 5,A"
	ld (_SetRTSto1Slow2+1),a
	ld a,#AF
	ld (_SetRTSto0Slow1+1),a		;Change "RES 4,A" to "RES 5,A"
	ld (_SetRTSto0Slow2+1),a

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
;    unsigned char SerialReceiveSlow(unsigned char* address, int length)

_SerialReceiveSlow::
	LD	BC,#0099			;B=0 -> ~4 SECONDS TIME-OUT, C = VDP STATUS REGISTER

	DI					;NO INTERRUPTS, TIME CRITICAL ROUTINE

	LD	A,#0F			;PSG REGISTER 15, SELECT JOYSTICK PORT 2
	OUT	(#A0),A
	IN	A,(#A2)
_SelectJoyPortSlow1:
	res	6,A	;SELECT JOY1
_SetRTSto0Slow1:
	res	4,a	;Unset pin 8 for now (our RTS, CTS of peer) - we're not ready to receive data just yet
	OUT	(#A1),A

	LD	A,#0E			;SET PSG #14
	OUT	(#A0),A

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
_SetRTSto1Slow1:
	set 4,a	;Now set our RTS (signal CTS to peer) - we are ready to receive data
	OUT	(#A1),A

	LD	A,#0E			;SET PSG #14
	OUT	(#A0),A

	LD	BC,#0099

_STARTBIT:
	IN	A,(#A2)
	AND	#01
	JP	Z,_STARTREAD		;YES, WE HAVE A START BIT

	IN	F,(C)			;VDP INTERRUPT?
	JP	P,_STARTBIT		;NO INTERRUPT

	IN	A,(#A2)
	AND	#01
	JP	Z,_STARTREAD		;YES, WE HAVE A START BIT

	DJNZ	_STARTBIT
	LD	A,#02			;ERROR START BIT TIME-OUT ~4-5S
	SCF
	EI
	RET
_STARTREAD:
	LD	A,(DELAY_START)	;DELAY FROM START BIT -> BIT 0
	CALL	DELAY			;WAIT FOR BIT0
	LD	B,7				;WE NEED 8 BITS, READ AS 7+1
_READBITS:
	IN	A,(#A2)
	RRCA				;SHIFT DATA BIT (0) -> CARRY
	RR	(HL)				;SHIFT CARRY -> [HL]
	LD	A,(DELAY_BITS)	;DELAY FROM BIT N -> BIT N+1
	CALL	DELAY
	DJNZ	_READBITS
	IN	A,(#A2)			;LAST BIT, OTHER DELAY (STOPBIT)
	RRCA				;SHIFT DATA BIT (0) -> CARRY
	RR	(HL)				;SHIFT CARRY -> [HL]
_NEXTBYTE:
	LD	A,(DELAY_STOP)	;DELAY BIT 7 TO ENSURE WE ARE AT STOPBIT
	CALL	DELAY
	LD	B,A				;LD B,0 BUT A=0
	INC	HL
	DEC	DE
	LD	A,D
	OR	E
	JP	Z,_FINISH		;WE ARE FINISHED
	IN	A,(#A2)			;READ ACTUAL STOPBIT VALUE
	AND	#01
	JR	NZ,_STARTBIT		;NEXT BYTE OR STOPBIT ERROR
_STOPBITERROR:
	LD	A,3
	SCF
	EI
	RET
_FINISH:
	XOR	A				;RESET CARRY FLAG
	EI
	RET

;----------------------------------------------------------

;--- Send data
;    Input:  HL = Source address
;            DE = Length
;    Output: A = Error code:
;                0: Ok
;                1: CTS timeout
;
;    C signature:
;    unsigned char SerialSendSlow(unsigned char* address, int length)

_SerialSendSlow::
	DI		;NO INTERRUPTS

	LD	A,#0F	;SELECT PSG REG #15
	OUT	(#A0),A

	IN	A,(#A2)
_SelectJoyPortSlow2:
	res	6,A	;JOY1
_SetRTSto1Slow2:
	set	4,A	;Set our RTS (signals we want to send data)
	out	(#A1),a

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
_SetRTSto0Slow2:
	res	4,a	;Clear our RTS
	out	(#A1),a

	ld	b,d
	ld	c,e

	LD	A,#0F	;SELECT PSG REG #15
	OUT	(#A0),A
	IN	A,(#A2)
	PUSH	AF		;SAVE VALUE OF REG #15
_SelectJoyPortSlow3:
	res	6,A	;JOY1
_SetDataLineTo0Slow:
	RES	0,A	;TRIG1 LOW
	LD	E,A		;0V VALUE (0) IN E
_SetDataLineTo1Slow:
	SET	0,A	;TRIG1 HIGH
	LD	D,A		;5V VALUE (1) IN D
_BYTELOOP:
	PUSH	BC
	LD	A,E			;START BIT (=0)
;_STARTBIT:
	LD	C,(HL)
	LD	B,#08
	OUT	(#A1),A
	ADD	A,#00		;DUMMY 8 CYCLES
	LD	A,(DELAY_BITS)
	CALL	DELAY
_BITLOOP:
	RRC	C
	LD	A,D			;ASSUME BIT=1
	JR	C,_SETBIT
	LD	A,E			;NO, BIT=0
_SETBIT:
	OUT	(#A1),A
	LD	A,(DELAY_BITS)
	CALL	DELAY
	DJNZ	_BITLOOP
_STOPBIT:
	LD	A,D
	OUT	(#A1),A		;STOP BIT (=1)
	LD	A,(DELAY_STOP)
	CALL	DELAY
	POP	BC
	DEC	BC
	INC	HL
	LD	A,B
	OR	C
	JP	NZ,_BYTELOOP
_EXIT:
	POP	AF
	OUT	(#A1),A		;RESTORE REG #15 OF PSG
	xor a
	EI
	RET

DELAY:
	DEC	A
	JP	NZ,DELAY
	RET

;----------------------------------------------------------

;--- Set the communication speed (default is 9600 bauds)
;    Input: A = Speed (bauds):
;               0: 2400
;               1: 4800
;               2: 9600
;               3: 19200
;    C signature:
;    void SerialSetSpeedSlow(unsigned char speed)

_SerialSetSpeedSlow::
	AND	3
	LD	HL,SERIALSPEEDDATA
	LD	E,A
	SLA	A
	ADD	A,E
	LD	E,A
	LD	D,0
	ADD	HL,DE
	LD	DE,DELAY_START
	LD	BC,3
	LDIR
	RET

DELAY_START:
	DB	28
DELAY_BITS:
	DB	18
DELAY_STOP:
	DB	16

SERIALSPEEDDATA:
;2400 BPS (0)
	DB	133,88,85
;4800 BPS (1)
	DB	63,41,38
;9600 BPS (2)
	DB	28,18,16
;19200 BPS (3)
	DB	11,6,4
