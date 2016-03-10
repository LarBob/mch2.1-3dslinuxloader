#include "linux_config.h"

.arm
.align 4
.code 32
.text

@ LCD Frambuffers stuff (Physical Addresses)
#define LCD_FB_PDC0           (0x10400400)
#define LCD_FB_PDC1           (0x10400500)
#define LCD_FB_A_ADDR_OFFSET  (0x68)
#define LCD_FB_SELECT_OFFSET  (0x78)
#define LCD_FB_B_ADDR_OFFSET  (0x94)
#define FB_TOP_SIZE           (400*240*3)
#define FB_BOT_SIZE           (320*240*3)
#define FB_TOP_LEFT1          (FB_BASE_PA)
#define FB_TOP_LEFT2          (FB_TOP_LEFT1  + FB_TOP_SIZE)
#define FB_TOP_RIGHT1         (FB_TOP_LEFT2  + FB_TOP_SIZE)
#define FB_TOP_RIGHT2         (FB_TOP_RIGHT1 + FB_TOP_SIZE)
#define FB_BOT_1              (FB_TOP_RIGHT2 + FB_TOP_SIZE)
#define FB_BOT_2              (FB_BOT_1      + FB_BOT_SIZE)

@ This must be Position-independent Code

	.global linux_payloads_start
linux_payloads_start:

@@@@@@@@@@@@@@@@@@@@@@@@ ARM11 Stage 1 @@@@@@@@@@@@@@@@@@@@@@@@

	.cpu mpcore
linux_arm11_stage_start:

	@ Disable FIQs, IRQs, imprecise aborts
	@ and enter SVC mode
	CPSID aif, #0x13

	@ Invalidate Entire Instruction Cache,
	@ also flushes the branch target cache
	mov r0, #0
	mcr p15, 0, r0, c7, c5, 0

	@ Clear and Invalidate Entire Data Cache
	mov r0, #0
	mcr p15, 0, r0, c7, c14, 0

	@ Data Synchronization Barrier
	mov r0, #0
	mcr p15, 0, r0, c7, c10, 4

	@ Disable the MMU and data cache
	@ (the MMU is already disabled)
	mrc p15, 0, r1, c1, c0, 0
	bic r1, r1, #0b101
	mcr p15, 0, r1, c1, c0, 0

	@ Clear exclusive records
	clrex

	@@@@@ Map Framebuffers @@@@@

	@@@ Top screen @@@
	ldr r0, =LCD_FB_PDC0

	@ Left eye
	ldr r1, =FB_TOP_LEFT1
	str r1, [r0, #(LCD_FB_A_ADDR_OFFSET + 0)]
	ldr r1, =FB_TOP_LEFT2
	str r1, [r0, #(LCD_FB_A_ADDR_OFFSET + 4)]

	@ Right eye
	ldr r1, =FB_TOP_RIGHT1
	str r1, [r0, #(LCD_FB_B_ADDR_OFFSET + 0)]
	ldr r1, =FB_TOP_RIGHT2
	str r1, [r0, #(LCD_FB_B_ADDR_OFFSET + 4)]

	@ Select framebuffer 0
	mov r1, #0
	str r1, [r0, #LCD_FB_SELECT_OFFSET]

	@@@ Bottom screen @@@
	ldr r0, =LCD_FB_PDC1

	ldr r1, =FB_BOT_1
	str r1, [r0, #(LCD_FB_A_ADDR_OFFSET + 0)]
	ldr r1, =FB_BOT_2
	str r1, [r0, #(LCD_FB_A_ADDR_OFFSET + 4)]

	@ Select framebuffer 0
	mov r1, #0
	str r1, [r0, #LCD_FB_SELECT_OFFSET]

	@@@@@ Setup dtb @@@@@

	@ Get the Linux Params size
	ldr r2, =PARAMS_SIZE_ADDR
	ldr r2, [r2]

	@ Copy the Linux Params to its
	@ destination address
	ldr r0, =PARAMS_ADDR
	ldr r1, =PARAMS_TMP_ADDR
	bl memcpy32

	@@@@@ Jump to the kernel @@@@@

	@ Setup the registers before
	@ jumping to the kernel entry
	mov r0, #0
	ldr r1, =MACHINE_NUMBER
	ldr r2, =PARAMS_ADDR
	ldr lr, =ZIMAGE_ADDR

	@ Jump to the kernel!
	bx lr

	.ltorg

@ r0 = dst, r1 = src, r2 = size
memcpy32:
	cmp r2, #0
	ble _memcpy32_ret
	ldr r3, [r1]
	str r3, [r0]
	sub r2, #4
	add r0, #4
	add r1, #4
	b memcpy32
_memcpy32_ret:
	bx lr

	.ltorg

	.global linux_payloads_end
linux_payloads_end:
