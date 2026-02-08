/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#ifndef _REWIND_H_
#define _REWIND_H_

// Rewind engine: ring buffer of XOR-delta compressed save state snapshots.
// Targets ~200 snapshots captured every 3 frames, giving roughly 10 seconds
// of rewind history at 60 fps.

void RewindInit();       // Allocate ring buffer
void RewindDeinit();     // Free ring buffer
void RewindCapture();    // Called every frame from main loop -- captures every Nth frame
bool RewindStep();       // Step back one snapshot, returns false if no more history
void RewindRelease();    // Resume forward play, discard frames after current position
bool RewindActive();     // Returns true if currently rewinding

#endif
