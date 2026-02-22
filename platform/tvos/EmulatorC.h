#ifndef EMULATOR_C_H
#define EMULATOR_C_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool EmulatorC_Init(const char *config_path);
void EmulatorC_SetSaveDirectory(const char *path);
bool EmulatorC_LoadROM(const char *rom_path);
void EmulatorC_RunFrame(void);
void EmulatorC_Shutdown(void);
void EmulatorC_SetRewindEnabled(bool enabled);

void EmulatorC_RewindStartContinuous(void);
void EmulatorC_RewindStop(void);
void EmulatorC_RewindTick(void);
bool EmulatorC_IsRewinding(void);
int EmulatorC_GetRewindBufferDepth(void);
int EmulatorC_GetRewindPosition(void);

void EmulatorC_Suspend(void);
void EmulatorC_Resume(void);

void EmulatorC_SetButtonState(int pad, uint16_t buttons);

const uint16_t *EmulatorC_GetFrameBuffer(void);
int EmulatorC_GetFrameWidth(void);
int EmulatorC_GetFrameHeight(void);
bool EmulatorC_IsPAL(void);
const char *EmulatorC_GetROMName(void);

#ifdef __cplusplus
}
#endif

#endif
