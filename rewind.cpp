/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#include <cstdlib>
#include <cstring>
#include <vector>

#include "snes9x.h"
#include "snapshot.h"
#include "rewind.h"

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

static constexpr int    CAPTURE_INTERVAL  = 3;    // capture once every N frames
static constexpr size_t RING_CAPACITY     = 200;  // max snapshots in the ring
static constexpr int    KEYFRAME_INTERVAL = 30;   // insert a full keyframe every N captures

// ---------------------------------------------------------------------------
// Snapshot slot
// ---------------------------------------------------------------------------

struct RewindSlot
{
    bool     is_key   = false;   // true = full state, false = XOR delta
    uint8_t *data     = nullptr;
    size_t   data_len = 0;

    void free_data()
    {
        if (data) { free(data); data = nullptr; }
        data_len = 0;
        is_key   = false;
    }
};

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------

static RewindSlot *s_ring        = nullptr;
static size_t      s_ring_size   = 0;       // always RING_CAPACITY when active
static int         s_head        = -1;      // index of most recent written slot
static int         s_count       = 0;       // number of valid slots
static int         s_cursor      = -1;      // position while rewinding
static int         s_frame_ctr   = 0;       // frame counter between captures
static bool        s_rewinding   = false;

static uint32_t    s_state_size  = 0;       // bytes per save state
static uint8_t    *s_cur_state   = nullptr; // scratch: current freeze
static uint8_t    *s_prev_state  = nullptr; // copy of previous full state for delta
static bool        s_have_prev   = false;
static int         s_key_ctr     = 0;       // captures since last keyframe

// ---------------------------------------------------------------------------
// Ring helpers
// ---------------------------------------------------------------------------

static inline int ring_prev(int i) { return (i - 1 + (int)s_ring_size) % (int)s_ring_size; }
static inline int ring_next(int i) { return (i + 1) % (int)s_ring_size; }
static inline int ring_tail()      { return (s_head - s_count + 1 + (int)s_ring_size) % (int)s_ring_size; }

// ---------------------------------------------------------------------------
// XOR two buffers: dst[i] ^= src[i]
// ---------------------------------------------------------------------------

static void xor_apply(uint8_t *dst, const uint8_t *src, size_t len)
{
    size_t n8 = len / 8;
    uint64_t       *d = reinterpret_cast<uint64_t *>(dst);
    const uint64_t *s = reinterpret_cast<const uint64_t *>(src);
    for (size_t i = 0; i < n8; i++)
        d[i] ^= s[i];
    for (size_t i = n8 * 8; i < len; i++)
        dst[i] ^= src[i];
}

// ---------------------------------------------------------------------------
// Reconstruct the full state at ring index `idx` into s_cur_state.
// Walks backwards to the nearest keyframe, then replays deltas forward.
// ---------------------------------------------------------------------------

static bool reconstruct(int idx)
{
    std::vector<int> chain;
    int cur  = idx;
    int tail = ring_tail();

    for (;;)
    {
        chain.push_back(cur);
        if (s_ring[cur].is_key)
            break;
        if (cur == tail)
            return false; // no keyframe found -- should never happen
        cur = ring_prev(cur);
    }

    // chain.back() is the keyframe -- copy it into s_cur_state
    memcpy(s_cur_state, s_ring[chain.back()].data, s_state_size);

    // replay deltas forward (skip keyframe)
    for (int i = (int)chain.size() - 2; i >= 0; i--)
        xor_apply(s_cur_state, s_ring[chain[i]].data, s_state_size);

    return true;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void RewindInit()
{
    RewindDeinit();

    s_state_size = S9xFreezeSize();
    if (s_state_size == 0)
        return;

    s_ring_size = RING_CAPACITY;
    s_ring = new RewindSlot[s_ring_size]();

    s_cur_state  = (uint8_t *)calloc(1, s_state_size);
    s_prev_state = (uint8_t *)calloc(1, s_state_size);

    s_head       = -1;
    s_count      = 0;
    s_cursor     = -1;
    s_frame_ctr  = 0;
    s_rewinding  = false;
    s_have_prev  = false;
    s_key_ctr    = 0;
}

void RewindDeinit()
{
    if (s_ring)
    {
        for (size_t i = 0; i < s_ring_size; i++)
            s_ring[i].free_data();
        delete[] s_ring;
        s_ring = nullptr;
    }
    s_ring_size = 0;

    free(s_cur_state);  s_cur_state  = nullptr;
    free(s_prev_state); s_prev_state = nullptr;

    s_head       = -1;
    s_count      = 0;
    s_cursor     = -1;
    s_frame_ctr  = 0;
    s_rewinding  = false;
    s_have_prev  = false;
    s_key_ctr    = 0;
    s_state_size = 0;
}

void RewindCapture()
{
    if (!s_ring || s_rewinding)
        return;

    if (++s_frame_ctr < CAPTURE_INTERVAL)
        return;
    s_frame_ctr = 0;

    // Freeze current emulator state.
    if (!S9xFreezeGameMem(s_cur_state, s_state_size))
        return;

    // Advance head, possibly overwriting oldest slot.
    int new_head = (s_head < 0) ? 0 : ring_next(s_head);
    s_ring[new_head].free_data();

    // Decide whether this capture should be a keyframe.
    // The first capture is always a keyframe, and every KEYFRAME_INTERVAL
    // captures thereafter.  Periodic keyframes guarantee that when the ring
    // wraps and the oldest slot is overwritten, the new tail will find a
    // keyframe within a bounded number of steps.
    bool make_key = !s_have_prev || (s_key_ctr >= KEYFRAME_INTERVAL);

    if (make_key)
    {
        s_ring[new_head].is_key   = true;
        s_ring[new_head].data_len = s_state_size;
        s_ring[new_head].data     = (uint8_t *)malloc(s_state_size);
        memcpy(s_ring[new_head].data, s_cur_state, s_state_size);
        s_key_ctr = 0;
    }
    else
    {
        // XOR delta: cur ^ prev.
        s_ring[new_head].is_key   = false;
        s_ring[new_head].data_len = s_state_size;
        s_ring[new_head].data     = (uint8_t *)malloc(s_state_size);
        memcpy(s_ring[new_head].data, s_cur_state, s_state_size);
        xor_apply(s_ring[new_head].data, s_prev_state, s_state_size);
        s_key_ctr++;
    }

    s_head = new_head;
    if (s_count < (int)s_ring_size)
        s_count++;

    // Save current state as prev for next delta.
    memcpy(s_prev_state, s_cur_state, s_state_size);
    s_have_prev = true;
}

bool RewindStep()
{
    if (!s_ring || s_count == 0)
        return false;

    if (!s_rewinding)
    {
        // Entering rewind mode.  Start at head.
        s_rewinding = true;
        s_cursor    = s_head;

        // Reconstruct and apply the state at cursor.
        if (!reconstruct(s_cursor))
            return false;
        S9xUnfreezeGameMem(s_cur_state, s_state_size);
        return true;
    }

    // Already rewinding -- step back one slot.
    int tail = ring_tail();
    if (s_cursor == tail)
        return false; // no more history

    int prev = ring_prev(s_cursor);
    // Make sure we don't step past the tail.
    // Walk from tail to prev to confirm prev is within range.
    bool valid = false;
    {
        int t = tail;
        for (int i = 0; i < s_count; i++)
        {
            if (t == prev) { valid = true; break; }
            t = ring_next(t);
        }
    }
    if (!valid)
        return false;

    s_cursor = prev;

    if (!reconstruct(s_cursor))
        return false;
    S9xUnfreezeGameMem(s_cur_state, s_state_size);
    return true;
}

void RewindRelease()
{
    if (!s_rewinding)
        return;

    // Discard all snapshots newer than cursor.
    // Set head = cursor, adjust count.
    if (s_cursor >= 0)
    {
        int tail = ring_tail();

        // Recalculate count: number of slots from tail to cursor inclusive.
        int new_count = 0;
        int t = tail;
        for (int i = 0; i < s_count; i++)
        {
            new_count++;
            if (t == s_cursor)
                break;
            t = ring_next(t);
        }

        // Free slots between old head and cursor that are being discarded.
        int d = ring_next(s_cursor);
        while (d != ring_next(s_head))
        {
            s_ring[d].free_data();
            d = ring_next(d);
        }

        s_head  = s_cursor;
        s_count = new_count;
    }

    // Update prev_state so that the next capture produces a correct delta.
    if (s_count > 0 && reconstruct(s_head))
    {
        memcpy(s_prev_state, s_cur_state, s_state_size);
        s_have_prev = true;
    }
    else
    {
        s_have_prev = false;
    }

    s_rewinding = false;
    s_cursor    = -1;
    s_frame_ctr = 0;
    s_key_ctr   = 0;
}

bool RewindActive()
{
    return s_rewinding;
}
