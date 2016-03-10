#pragma once

void InvalidateEntireInstructionCache(void);
void InvalidateEntireDataCache(void);
void CleanEntireDataCache();
void dsb();
void DisableInterrupts();
void EnableInterrupts();
void DisableMMUandJump(void *pa);
