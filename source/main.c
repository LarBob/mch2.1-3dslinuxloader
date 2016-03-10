#include <3ds.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "hid.h"
#include "utils.h"
#include "svchax.h"
#include "linux_config.h"

#define LINUX_BUFFER_SIZE (20*1024*1024)

extern void *linux_payloads_start;
extern void *linux_payloads_end;

static inline void *fcram_phys2linear(void *linear_start, unsigned int phys_addr)
{
	return (void *)(linear_start + (phys_addr - 0x20000000));
}

static void flush_dcache()
{
	unsigned int i;
	volatile unsigned int *p;
	void *dummymem;

	if (!(dummymem = malloc(2*1024*1024)))
		return;

	p = dummymem;
	for (i = 0; i < 2*1024*1024; i+=4)
		*p++;

	free(dummymem);
}

s32 kern_func()
{
	unsigned int linux_payloads_size = (u32)&linux_payloads_end - (u32)&linux_payloads_start;

	// Copy ARM11 payload to va(0x23F00000)
	memcpy((void *)0xE3F00000, &linux_payloads_start, linux_payloads_size);

	CleanEntireDataCache();
	InvalidateEntireInstructionCache();

	DisableMMUandJump((void *)0x23F00000);

	return 0;
}

int main(int argc, char **argv)
{
	// First allocate the buffer for the Linux image + DTB
	void *linux_buffer = linearAlloc(LINUX_BUFFER_SIZE);
	memset(linux_buffer, 0, LINUX_BUFFER_SIZE);

	// Initialize services
	gfxInitDefault();
	gfxSwapBuffers();
	sdmcInit();

	consoleInit(GFX_BOTTOM, NULL);

	printf("Linux loader by xerpi/svchax added by LarBob)\n\n");


	printf("[+] svchax inited!\n");

	printf("[+] Opening the linux image...\n");

	FILE *zImage = fopen(LINUXIMAGE_FILENAME, "rb");

	printf("[+] fopen returned: %p\n", zImage);

	if (zImage == NULL) {
		printf("[+] Error opening the Linux zImage\n"
			"Press any key to reboot.");
		wait_any_key();
		goto error;
	}

	//Load the kernel image code to ZIMAGE_ADDR
	unsigned int n = 0;
	unsigned int linux_bin_size = 0;

	while ((n = fread(fcram_phys2linear(linux_buffer, ZIMAGE_ADDR)+linux_bin_size, 1, 0x10000, zImage)) > 0) {
		linux_bin_size += n;
	}

	fclose(zImage);

	printf("[+] Loaded kernel:\n");
	printf("[+]     address: %p,\n", (void *)ZIMAGE_ADDR);
	printf("[+]     size:    0x%08X bytes\n", linux_bin_size);

	//Load the device tree to PARAMS_ADDR
	printf("\n[+] Opening " DTB_FILENAME "\n");

	FILE *dtb = fopen(DTB_FILENAME, "rb");

	printf("[+] FileOpen returned: %p\n", dtb);

	if (dtb == NULL) {
		printf("[+] Error opening " DTB_FILENAME "\n"
			"Press any key to reboot.");
		wait_any_key();
		goto error;
	}

	n = 0;
	unsigned int dtb_bin_size = 0;
	while ((n = fread(fcram_phys2linear(linux_buffer, PARAMS_TMP_ADDR)+dtb_bin_size, 1, 0x10000, dtb)) > 0) {
		dtb_bin_size += n;
	}

	fclose(dtb);

	printf("[+] Loaded " DTB_FILENAME ":\n");
	printf("[+]     temp address: %p,\n", (void *)PARAMS_TMP_ADDR);
	printf("[+]     dest address: %p,\n", (void *)PARAMS_ADDR);
	printf("[+]     size:    0x%08X bytes\n", dtb_bin_size);

	// Store the DTB size to *PARAMS_SIZE_ADDR
	*(unsigned int *)fcram_phys2linear(linux_buffer, PARAMS_SIZE_ADDR) = dtb_bin_size;

	GSPGPU_FlushDataCache(fcram_phys2linear(linux_buffer, ZIMAGE_ADDR), linux_bin_size);
	GSPGPU_FlushDataCache(fcram_phys2linear(linux_buffer, PARAMS_ADDR), dtb_bin_size);
	GSPGPU_FlushDataCache(fcram_phys2linear(linux_buffer, PARAMS_SIZE_ADDR), sizeof(unsigned int));

	printf("[+] Loading Linux Payloads...\n");

	unsigned int linux_payloads_size = (u32)&linux_payloads_end - (u32)&linux_payloads_start;

	printf("[+]     size %i\n", linux_payloads_size);

	flush_dcache();

	printf("[+] Jumping to ARM11 payload...\n");

	svcBackdoor(kern_func);

error:
	linearFree(linux_buffer);
	gfxExit();
	sdmcExit();
	amExit();
	return 0;
}
