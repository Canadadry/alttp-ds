@ dsp_run(Dsp *dsp, int n)
@
@ Same sample as zelda3's dsp_cycle: 4-tap Gaussian, full envelope, echo and
@ noise. Offsets match this Dsp / DspChannel (channel 86 bytes, channels at
@ 0x84, sample buffer at 0x378, sampleOffset at 0xbd0).
@
@ Halfword loads only take a 0..255 offset, so fields past 255 are reached
@ from a base. MUL keeps Rd different from Rm.

.syntax unified
.arch armv4t
.arm
.text
.global dsp_run
.type dsp_run, %function

@ r11 is 0x7fff for the whole call. Do not clamp r0 or r11.
.macro clamp rd
	cmp	\rd, r11
	movgt	\rd, r11
	mvn	r0, r11
	cmp	\rd, r0
	movlt	\rd, r0
.endm

@ One FIR tap. r9 is the buffer index, r2 is dsp+0x300. Sums live in r5 and r10.
@ The coefficient stays in r3 so the echo address in r1 is left alone.
.macro fir_tap n
	add	r12, r9, #(\n + 1)
	and	r12, r12, #7
	add	r0, r2, #0x58
	add	r0, r0, r12, lsl #1
	ldrsh	r0, [r0]
	ldrsb	r3, [r2, #(0x4f + \n)]
	mul	r0, r3, r0
	add	r5, r5, r0, asr #6
	add	r0, r2, #0x68
	add	r0, r0, r12, lsl #1
	ldrsh	r0, [r0]
	mul	r0, r3, r0
	add	r10, r10, r0, asr #6
.endm

dsp_run:
	push	{r4-r11, lr}
	sub	sp, sp, #28
	mov	r4, r0
	ldr	r6, =gaussValues
	mov	r11, #0x7f00
	add	r11, r11, #0xff
	str	r1, [sp, #4]
0:
	ldr	r0, [sp, #4]
	cmp	r0, #0
	beq	9f
	sub	r0, r0, #1
	str	r0, [sp, #4]

	mov	r7, #0
	mov	r8, #0
	str	r7, [sp, #20]
	str	r8, [sp, #24]
	add	r5, r4, #0x84
	mov	r9, #0
	add	r2, r4, #0x300
	ldrb	r0, [r2, #0x38]
	cmp	r0, #0
	bne	1f

@ Sustain and release are where a note spends its time. Anything else
@ (attack, decay, noise, pitch modulation, gain mode) uses the full voice
@ loop below, starting at this voice.
.Lfast:
	ldrb	r0, [r5, #54]
	cmp	r0, #0
	bne	1f
	cmp	r9, #0
	beq	.Lnomod
	ldrb	r0, [r5, #4]
	cmp	r0, #0
	bne	1f
.Lnomod:
	ldrb	r0, [r5, #70]
	cmp	r0, #0
	bne	1f
	ldrb	r0, [r5, #66]
	cmp	r0, #2
	beq	.Lsus
	cmp	r0, #4
	bne	1f
	ldrh	r0, [r5, #76]
	cmp	r0, #0
	bne	.Lfrel
	ldrb	r1, [r5, #78]
	cmp	r1, #0
	bne	1f
	strh	r0, [r5, #80]
	b	.Lfast_next
.Lfrel:
	sub	r0, r0, #8
	cmp	r0, r11
	movhi	r0, #0
	strh	r0, [r5, #76]
	cmp	r0, #0
	bne	.Lgauss
	ldrb	r1, [r5, #78]
	cmp	r1, #0
	bne	.Lgauss
	strh	r0, [r5, #80]
	b	.Lfast_next
.Lsus:
	ldrh	r2, [r5, #60]
	cmp	r2, #0
	beq	.Lgauss
	ldrh	r0, [r5, #64]
	add	r0, r0, #1
	strh	r0, [r5, #64]
	mov	r0, r0, lsl #16
	mov	r0, r0, lsr #16
	cmp	r0, r2
	blo	.Lgauss
	mov	r0, #0
	strh	r0, [r5, #64]
	ldrh	r0, [r5, #76]
	sub	r1, r0, #1
	mov	r1, r1, asr #8
	add	r1, r1, #1
	sub	r0, r0, r1
	strh	r0, [r5, #76]
.Lgauss:
	ldrh	r0, [r5, #0]
	ldrh	r1, [r5, #2]
	add	r1, r1, r0
	movs	r2, r1, lsr #16
	beq	.Lnodec
	str	r1, [sp, #8]
	mov	r0, r4
	mov	r1, r9
	bl	dsp_decodeBrr
	ldr	r1, [sp, #8]
.Lnodec:
	strh	r1, [r5, #2]
	mov	r10, r1, lsr #4
	and	r10, r10, #0xff
	mov	r3, r1, lsr #12
	and	r3, r3, #0xf
	add	r3, r5, r3, lsl #1
	add	r3, r3, #6
	ldrsh	r0, [r3, #0]
	rsb	r2, r10, #0xff
	lsl	r2, r2, #1
	ldrsh	r2, [r6, r2]
	mul	r1, r0, r2
	mov	r12, r1, asr #10
	ldrsh	r0, [r3, #2]
	rsb	r2, r10, #0xff
	add	r2, r2, #0x100
	lsl	r2, r2, #1
	ldrsh	r2, [r6, r2]
	mul	r1, r0, r2
	add	r12, r12, r1, asr #10
	ldrsh	r0, [r3, #4]
	add	r2, r10, #0x100
	lsl	r2, r2, #1
	ldrsh	r2, [r6, r2]
	mul	r1, r0, r2
	add	r12, r12, r1, asr #10
	mov	r12, r12, lsl #16
	mov	r12, r12, asr #16
	ldrsh	r0, [r3, #6]
	lsl	r2, r10, #1
	ldrsh	r2, [r6, r2]
	mul	r1, r0, r2
	add	r12, r12, r1, asr #10
	clamp	r12
	mov	r12, r12, asr #1
	ldrh	r0, [r5, #76]
	mul	r1, r12, r0
	mov	r12, r1, asr #11
	mov	r12, r12, lsl #16
	mov	r12, r12, asr #16
	strh	r12, [r5, #80]
	ldrsh	r0, [r5, #80]
	ldrsb	r1, [r5, #82]
	mul	r2, r0, r1
	ldrsb	r1, [r5, #83]
	mul	r3, r0, r1
	ldrb	r0, [r5, #84]
	cmp	r0, #0
	beq	.Lvol
	ldr	r10, [sp, #20]
	add	r10, r10, r2, asr #6
	clamp	r10
	str	r10, [sp, #20]
	ldr	r10, [sp, #24]
	add	r10, r10, r3, asr #6
	clamp	r10
	str	r10, [sp, #24]
.Lvol:
	add	r7, r7, r2, asr #6
	clamp	r7
	add	r8, r8, r3, asr #6
	clamp	r8
.Lfast_next:
	add	r5, r5, #86
	add	r9, r9, #1
	cmp	r9, #8
	blt	.Lfast
	b	.Lepilogue

1:
	@ Released and silent: sampleOut = 0. Key-on restarts the sample.
	add	r2, r4, #0x300
	ldrb	r0, [r2, #0x38]
	cmp	r0, #0
	bne	2f
	ldrb	r0, [r5, #66]
	cmp	r0, #4
	bne	2f
	ldrh	r0, [r5, #76]
	cmp	r0, #0
	bne	2f
	ldrb	r0, [r5, #78]
	cmp	r0, #0
	bne	2f
	mov	r0, #0
	strh	r0, [r5, #80]
	b	.Lnext

2:
	ldrh	r0, [r5, #0]
	cmp	r9, #0
	beq	3f
	ldrb	r1, [r5, #4]
	cmp	r1, #0
	beq	3f
	sub	r2, r5, #86
	ldrsh	r2, [r2, #80]
	mov	r2, r2, asr #4
	add	r2, r2, #0x400
	mul	r3, r0, r2
	mov	r3, r3, asr #10
	mov	r3, r3, lsl #16
	mov	r3, r3, lsr #16
	mov	r2, #0x3f00
	add	r2, r2, #0xff
	cmp	r3, r2
	movhi	r0, r2
	movls	r0, r3
3:
	ldrh	r1, [r5, #2]
	add	r1, r1, r0
	movs	r2, r1, lsr #16
	beq	4f
	str	r1, [sp, #8]
	mov	r0, r4
	mov	r1, r9
	bl	dsp_decodeBrr
	ldr	r1, [sp, #8]
4:
	strh	r1, [r5, #2]
	ldrb	r0, [r5, #54]
	cmp	r0, #0
	beq	5f
	add	r2, r4, #0x300
	ldrsh	r12, [r2, #0x3c]
	b	6f

5:
	mov	r10, r1, lsr #4
	and	r10, r10, #0xff
	mov	r3, r1, lsr #12
	and	r3, r3, #0xf
	add	r3, r5, r3, lsl #1
	add	r3, r3, #6

	ldrsh	r0, [r3, #0]
	rsb	r2, r10, #0xff
	lsl	r2, r2, #1
	ldrsh	r2, [r6, r2]
	mul	r1, r0, r2
	mov	r12, r1, asr #10

	ldrsh	r0, [r3, #2]
	rsb	r2, r10, #0xff
	add	r2, r2, #0x100
	lsl	r2, r2, #1
	ldrsh	r2, [r6, r2]
	mul	r1, r0, r2
	add	r12, r12, r1, asr #10

	ldrsh	r0, [r3, #4]
	add	r2, r10, #0x100
	lsl	r2, r2, #1
	ldrsh	r2, [r6, r2]
	mul	r1, r0, r2
	add	r12, r12, r1, asr #10

	mov	r12, r12, lsl #16
	mov	r12, r12, asr #16

	ldrsh	r0, [r3, #6]
	lsl	r2, r10, #1
	ldrsh	r2, [r6, r2]
	mul	r1, r0, r2
	add	r12, r12, r1, asr #10
	clamp	r12
	mov	r12, r12, asr #1

6:
	@ Sustain, direct gain, and release are the samples a song actually
	@ spends time in. They do the same update as the full envelope below.
	add	r2, r4, #0x300
	ldrb	r0, [r2, #0x38]
	cmp	r0, #0
	bne	.Lslow
	ldrb	r0, [r5, #66]
	cmp	r0, #4
	beq	.Lrel
	ldrb	r1, [r5, #70]
	cmp	r1, #0
	bne	.Lgain
	cmp	r0, #2
	bne	.Lslow
	ldrh	r2, [r5, #60]
	cmp	r2, #0
	beq	.Lout
	ldrh	r3, [r5, #64]
	add	r3, r3, #1
	strh	r3, [r5, #64]
	mov	r3, r3, lsl #16
	mov	r3, r3, lsr #16
	cmp	r3, r2
	blo	.Lout
	mov	r3, #0
	strh	r3, [r5, #64]
	ldrh	r0, [r5, #76]
	sub	r1, r0, #1
	mov	r1, r1, asr #8
	add	r1, r1, #1
	sub	r0, r0, r1
	strh	r0, [r5, #76]
	b	.Lout
.Lgain:
	ldrb	r1, [r5, #72]
	cmp	r1, #0
	beq	.Lslow
	ldrh	r0, [r5, #74]
	strh	r0, [r5, #76]
	b	.Lout
.Lrel:
	ldrh	r0, [r5, #76]
	sub	r0, r0, #8
	mov	r1, #0x7f0
	add	r1, r1, #15
	cmp	r0, r1
	movhi	r0, #0
	strh	r0, [r5, #76]
	b	.Lout
.Lslow:
	add	r2, r4, #0x300
	ldrb	r0, [r2, #0x38]
	cmp	r0, #0
	beq	6f
	mov	r0, #4
	strb	r0, [r5, #66]
	mov	r0, #0
	strh	r0, [r5, #76]
	mov	r0, #4
6:
	ldrb	r0, [r5, #66]
	mov	r1, #0
	cmp	r0, #4
	beq	6f
	ldrb	r2, [r5, #70]
	cmp	r2, #0
	beq	6f
	ldrb	r2, [r5, #72]
	cmp	r2, #0
	movne	r1, #1
6:
	cmp	r0, #4
	moveq	r2, #0
	beq	6f
	add	r3, r5, #56
	lsl	r2, r0, #1
	ldrh	r2, [r3, r2]
6:
	cmp	r0, #4
	beq	6f
	cmp	r1, #0
	bne	6f
	cmp	r2, #0
	beq	6f
	ldrh	r3, [r5, #64]
	add	r3, r3, #1
	strh	r3, [r5, #64]
6:
	cmp	r0, #4
	beq	6f
	cmp	r1, #0
	bne	7f
	cmp	r2, #0
	beq	7f
	ldrh	r3, [r5, #64]
	cmp	r3, r2
	blo	7f
	mov	r3, #0
	strh	r3, [r5, #64]
6:
	str	r12, [sp, #12]
	str	r1, [sp, #16]
	mov	r0, r4
	mov	r1, r9
	bl	dsp_handleGain
	ldr	r12, [sp, #12]
	ldr	r1, [sp, #16]
7:
	cmp	r1, #0
	beq	7f
	ldrh	r0, [r5, #74]
	strh	r0, [r5, #76]
7:
.Lout:
	ldrh	r0, [r5, #76]
	mul	r3, r12, r0
	mov	r12, r3, asr #11
	mov	r12, r12, lsl #16
	mov	r12, r12, asr #16
	strh	r12, [r5, #80]

	ldrsh	r0, [r5, #80]
	ldrsb	r1, [r5, #82]
	mul	r2, r0, r1
	ldrsb	r1, [r5, #83]
	mul	r3, r0, r1
	ldrb	r0, [r5, #84]
	cmp	r0, #0
	beq	7f
	ldr	r10, [sp, #20]
	add	r10, r10, r2, asr #6
	clamp	r10
	str	r10, [sp, #20]
	ldr	r10, [sp, #24]
	add	r10, r10, r3, asr #6
	clamp	r10
	str	r10, [sp, #24]
7:
	add	r7, r7, r2, asr #6
	clamp	r7
	add	r8, r8, r3, asr #6
	clamp	r8

.Lnext:
	add	r5, r5, #86
	add	r9, r9, #1
	cmp	r9, #8
	blt	1b

.Lepilogue:
	add	r2, r4, #0x300
	ldrsb	r0, [r2, #0x39]
	mul	r1, r7, r0
	mov	r7, r1, asr #7
	clamp	r7
	ldrsb	r0, [r2, #0x3a]
	mul	r1, r8, r0
	mov	r8, r1, asr #7
	clamp	r8

	bl	.Lecho

	add	r2, r4, #0x300
	ldrb	r0, [r2, #0x37]
	cmp	r0, #0
	movne	r7, #0
	movne	r8, #0

	ldrh	r0, [r2, #0x3e]
	cmp	r0, #0
	beq	.Lnskip
	ldrh	r1, [r2, #0x40]
	add	r1, r1, #1
	strh	r1, [r2, #0x40]
	mov	r1, r1, lsl #16
	mov	r1, r1, lsr #16
	cmp	r1, r0
	blo	.Lnskip
	ldrh	r3, [r2, #0x3c]
	and	r1, r3, #1
	mov	r12, r3, lsr #1
	and	r12, r12, #1
	eor	r1, r1, r12
	mov	r3, r3, lsr #1
	mov	r12, #0x3f00
	add	r12, r12, #0xff
	and	r3, r3, r12
	orr	r3, r3, r1, lsl #14
	mov	r12, #0x7f00
	add	r12, r12, #0xff
	and	r3, r3, r12
	mov	r3, r3, lsl #1
	mov	r3, r3, lsl #16
	mov	r3, r3, asr #16
	mov	r3, r3, asr #1
	strh	r3, [r2, #0x3c]
	mov	r3, #0
	strh	r3, [r2, #0x40]
.Lnskip:

	add	r1, r4, #0xb00
	ldrh	r0, [r1, #0xd0]
	mov	r2, #512
	add	r2, r2, #22
	cmp	r0, r2
	bhs	8f
	add	r3, r4, #0x300
	add	r3, r3, #0x78
	add	r3, r3, r0, lsl #2
	strh	r7, [r3]
	strh	r8, [r3, #2]
	add	r0, r0, #1
	strh	r0, [r1, #0xd0]
8:
	add	r2, r4, #0x300
	ldrb	r0, [r2, #0x36]
	eor	r0, r0, #1
	strb	r0, [r2, #0x36]
	b	0b
9:
	add	sp, sp, #28
	pop	{r4-r11, pc}

@ Echo for one sample. Same result as dsp_handleEcho, without a C call.
@ r7/r8 are the master outputs. r4, r6 and r11 stay put. Echo input is
@ already summed at [sp, #20] and [sp, #24].
.Lecho:
	ldr	r0, [r4]
	add	r2, r4, #0x300
	ldrh	r9, [r2, #0x4c]
	ldrh	r1, [r2, #0x46]
	add	r1, r1, r9, lsl #2
	mov	r1, r1, lsl #16
	mov	r1, r1, lsr #16
	add	r3, r0, r1
	ldrsh	r5, [r3]
	ldrsh	r10, [r3, #2]
	ldrb	r9, [r2, #0x4e]
	mov	r5, r5, asr #1
	mov	r10, r10, asr #1
	add	r0, r2, #0x58
	add	r0, r0, r9, lsl #1
	strh	r5, [r0]
	add	r0, r2, #0x68
	add	r0, r0, r9, lsl #1
	strh	r10, [r0]
	mov	r5, #0
	mov	r10, #0
	ldrb	r0, [r2, #0x43]
	ldrb	r3, [r2, #0x44]
	orr	r0, r0, r3
	ldrb	r3, [r2, #0x45]
	orrs	r0, r0, r3
	beq	.Lecho_out
	fir_tap	0
	fir_tap	1
	fir_tap	2
	fir_tap	3
	fir_tap	4
	fir_tap	5
	fir_tap	6
	mov	r5, r5, lsl #16
	mov	r5, r5, asr #16
	mov	r10, r10, lsl #16
	mov	r10, r10, asr #16
	fir_tap	7
.Lecho_out:
	clamp	r5
	clamp	r10
	ldrsb	r0, [r2, #0x43]
	mul	r3, r5, r0
	add	r7, r7, r3, asr #7
	clamp	r7
	ldrsb	r0, [r2, #0x44]
	mul	r3, r10, r0
	add	r8, r8, r3, asr #7
	clamp	r8
	ldrsb	r3, [r2, #0x45]
	ldr	r12, [sp, #20]
	mul	r0, r5, r3
	add	r12, r12, r0, asr #7
	clamp	r12
	bic	r12, r12, #1
	str	r12, [sp, #0]
	ldr	r12, [sp, #24]
	mul	r0, r10, r3
	add	r12, r12, r0, asr #7
	clamp	r12
	bic	r12, r12, #1
	ldrb	r3, [r2, #0x42]
	cmp	r3, #0
	beq	.Lecho_idx
	ldr	r3, [r4]
	add	r3, r3, r1
	ldr	r0, [sp, #0]
	strh	r0, [r3]
	strh	r12, [r3, #2]
.Lecho_idx:
	ldrb	r0, [r2, #0x4e]
	add	r0, r0, #1
	and	r0, r0, #7
	strb	r0, [r2, #0x4e]
	ldrh	r0, [r2, #0x4c]
	add	r0, r0, #1
	strh	r0, [r2, #0x4c]
	ldrh	r0, [r2, #0x4a]
	sub	r0, r0, #1
	strh	r0, [r2, #0x4a]
	mov	r0, r0, lsl #16
	movs	r0, r0, lsr #16
	bne	.Lecho_done
	ldrh	r0, [r2, #0x48]
	strh	r0, [r2, #0x4a]
	mov	r0, #0
	strh	r0, [r2, #0x4c]
.Lecho_done:
	bx	lr

.ltorg
.size dsp_run, .-dsp_run
