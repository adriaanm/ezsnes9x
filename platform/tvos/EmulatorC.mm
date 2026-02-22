#import "EmulatorC.h"
#include "emulator.h"

extern "C" {

bool EmulatorC_Init(const char *config_path) {
    return Emulator::Init(config_path);
}

void EmulatorC_SetSaveDirectory(const char *path) {
    Emulator::SetSaveDirectory(path);
}

bool EmulatorC_LoadROM(const char *rom_path) {
    return Emulator::LoadROM(rom_path);
}

void EmulatorC_RunFrame(void) {
    Emulator::RunFrame();
}

void EmulatorC_Shutdown(void) {
    Emulator::Shutdown();
}

void EmulatorC_SetRewindEnabled(bool enabled) {
    Emulator::SetRewindEnabled(enabled);
}

void EmulatorC_RewindStartContinuous(void) {
    Emulator::RewindStartContinuous();
}

void EmulatorC_RewindStop(void) {
    Emulator::RewindStop();
}

void EmulatorC_RewindTick(void) {
    Emulator::RewindTick();
}

bool EmulatorC_IsRewinding(void) {
    return Emulator::IsRewinding();
}

int EmulatorC_GetRewindBufferDepth(void) {
    return Emulator::GetRewindBufferDepth();
}

int EmulatorC_GetRewindPosition(void) {
    return Emulator::GetRewindPosition();
}

void EmulatorC_Suspend(void) {
    Emulator::Suspend();
}

void EmulatorC_Resume(void) {
    Emulator::Resume();
}

void EmulatorC_SetButtonState(int pad, uint16_t buttons) {
    Emulator::SetButtonState(pad, buttons);
}

const uint16_t *EmulatorC_GetFrameBuffer(void) {
    return Emulator::GetFrameBuffer();
}

int EmulatorC_GetFrameWidth(void) {
    return Emulator::GetFrameWidth();
}

int EmulatorC_GetFrameHeight(void) {
    return Emulator::GetFrameHeight();
}

bool EmulatorC_IsPAL(void) {
    return Emulator::IsPAL();
}

const char *EmulatorC_GetROMName(void) {
    return Emulator::GetROMName();
}

}
