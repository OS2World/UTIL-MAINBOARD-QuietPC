; $Id: helpers.asm,v 1.2 2004/12/16 08:04:35 root Exp $
;
; Miscellaneous assembly code

	OPTION	OLDSTRUCTS
	OPTION	SEGMENT:USE16

.386p

	extrn	_tick_handler:near

	public	_exp_tick_handler

_TEXT   SEGMENT WORD PUBLIC 'CODE'

; Exported tick handler - saves all necessary registers

_exp_tick_handler	proc near
		push	ds
		push	es
		pusha
		call	_tick_handler
		popa
		pop	es
		pop	ds
		retf
_exp_tick_handler	ENDP

_TEXT   ENDS

	IFDEF	USERMODE		; The big red button

;
; IOPL-enabled segment for user mode
;

PRIVTEXT SEGMENT	WORD PUBLIC 'PRIVCODE'

; Outp/inp routines

	public _priv_inp, _priv_outp

_priv_inp:
		push	bp
		push	es
		mov	bp, sp
		or	bp, 03FFh
		and	bp, 0FFFCh
		les	bx, ss:[bp]
		mov	dx, es:[bx]
		in	al, dx
		pop	es
		pop	bp
		retf

_priv_outp:
		push	bp
		push	es
		mov	bp, sp
		or	bp, 03FFh
		and	bp, 0FFFCh
		les	bx, ss:[bp]
		mov	dx, es:[bx]
		mov	al, es:[bx+2]
		out	dx, al
		pop	es
		pop	bp
		retf
		
PRIVTEXT ENDS


	ELSE

;
; Segment headers for kernel mode
;

TEXTEND	SEGMENT WORD PUBLIC 'CODE'
		PUBLIC _EndOfCode
_EndOfCode:
TEXTEND	ENDS

DATAEND	SEGMENT	WORD PUBLIC 'ZD'
		PUBLIC _EndOfData
_EndOfData:
DATAEND	ENDS

; DHCALLS

CODE	SEGMENT	WORD	PUBLIC 'CODE'
CODE	ENDS

; APM

LIBCODE	SEGMENT	WORD	PUBLIC 'CODE'
LIBCODE	ENDS

	DGROUP	GROUP	DATAEND
	CODESEG GROUP	_TEXT, CODE, TEXTEND, LIBCODE
		
	ENDIF

        END
