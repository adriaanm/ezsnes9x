# controls.h Functions Actually Used Outside controls.cpp

## Called by Core Emulator

### From ppu.cpp (Hardware I/O)
- `S9xControlsReset()` - Reset controller state
- `S9xControlsSoftReset()` - Soft reset controller state
- `S9xSetJoypadLatch(bool latch)` - Set joypad latch bit (write to $4016)
- `S9xReadJOYSERn(int n)` - Read joypad serial port (read $4016/$4017)

### From cpuexec.cpp (Auto-joypad)
- `S9xDoAutoJoypad()` - Automatic joypad read (declared in ppu.h, not controls.h!)

### From gfx.cpp (End of frame)
- `S9xControlEOF()` - End-of-frame controller processing (3 calls)

### From memmap.cpp (ROM loading)
- `S9xVerifyControllers()` - Verify controller configuration is valid

### From snapshot.cpp (Save states)
- `S9xControlPreSaveState(struct SControlSnapshot *s)` - Pre-save state hook
- `S9xControlPostLoadState(struct SControlSnapshot *s)` - Post-load state hook

## Called by Platform Frontend

### From platform/shared/emulator.cpp
**Setup:**
- `S9xSetController(int port, enum controllers controller, int8 id1, id2, id3, id4)` - Configure controller on port
- `S9xVerifyControllers()` - Verify controller configuration

**Input:**
- `S9xSetJoypadButtons(int pad, uint16 buttons)` - Set joypad button state directly

**Port Interface Stubs (must be implemented by frontend):**
- `S9xPollButton(uint32 id, bool *pressed)` - Poll button state (stub)
- `S9xPollPointer(uint32 id, int16 *x, int16 *y)` - Poll pointer state (stub)
- `S9xPollAxis(uint32 id, int16 *value)` - Poll axis state (stub)
- `S9xHandlePortCommand(s9xcommand_t cmd, int16 data1, data2)` - Handle port command (stub)
- `S9xGetPortCommandT(const char *name)` - Get port command from name (stub)
- `S9xGetPortCommandName(s9xcommand_t command)` - Get port command name (stub)
- `S9xSetupDefaultKeymap()` - Setup default keymap (stub)
- `S9xMapInput(const char *name, s9xcommand_t *cmd)` - Map input (stub)

## UNUSED Functions (declared but never called outside controls.cpp)

- `S9xUnmapAllControls()` - Unmap all controls
- `S9xGetController()` - Get controller configuration
- `S9xReportControllers()` - Report controller status
- `S9xGetCommandName()` - Get command name
- `S9xGetCommandT()` - Get command from name
- `S9xGetAllSnes9xCommands()` - Get all command names
- `S9xGetMapping()` - Get mapping for ID
- `S9xUnmapID()` - Unmap ID
- `S9xMapButton()` - Map button
- `S9xReportButton()` - Report button state
- `S9xMapPointer()` - Map pointer
- `S9xReportPointer()` - Report pointer state
- `S9xMapAxis()` - Map axis
- `S9xReportAxis()` - Report axis state
- `S9xApplyCommand()` - Apply command

## Summary

**Used: 19 functions**
- 9 called by core emulator (ppu, cpu, gfx, memmap, snapshot)
- 10 called/implemented by platform frontend (emulator.cpp)

**Unused: 17 functions**
- Mostly complex input mapping system from old frontends
- Button/Pointer/Axis mapping/reporting functions
- Command name translation functions

The unused functions appear to be from the old frontend input mapping system that has been replaced by the simpler direct joypad interface (`S9xSetJoypadButtons`).
