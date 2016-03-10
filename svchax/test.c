#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "svchax.h"

static const char* test_result ="FAILED!!";

s32 set_test_result(void)
{
   __asm__ volatile("cpsid aif \n\t");
   test_result = "SUCCESS !!!";
   return 0;
}

int main(int argc, char** argv)
{
   gfxInit(GSP_BGR8_OES, GSP_RGB565_OES, false);
   gfxSet3D(false);
   consoleInit(GFX_BOTTOM, NULL);
   printf("svchax\n");
   printf("Press Start to exit.\n");

   osSetSpeedupEnable(false);
   svchax_init();
   osSetSpeedupEnable(true);

   if(__ctr_svchax)
      svcBackdoor(set_test_result);

   u32 frames = 0;
   printf("result : %s\n", test_result);
   printf("kernel version : %i.%i.%i", (int)*(u8*)(0x1FF80003), (int)*(u8*)(0x1FF80002), (int)*(u8*)(0x1FF80001));
   printf("\n");
   while (aptMainLoop())
   {
      hidScanInput();
      u32 kDown = hidKeysDown();

      if (kDown & KEY_START)
         break;

      printf("frames : %u\r", (unsigned)frames++);
      gspWaitForVBlank();
      fflush(stdout);

   }

   gfxExit();
   return 0;
}





