/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#ifndef SNES9X_PORT_H_
#define SNES9X_PORT_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <time.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <sys/types.h>

// Platform-specific pixel format
#ifdef __MACOSX__
#define PIXEL_FORMAT RGB555
#else
#define PIXEL_FORMAT RGB565
#endif

// Force inline attribute (both clang and gcc support this)
#define alwaysinline inline __attribute__((always_inline))

// Standard types using stdint.h (always available on modern platforms)
typedef unsigned char		bool8;
typedef intptr_t				pint;
typedef int8_t					int8;
typedef uint8_t					uint8;
typedef int16_t					int16;
typedef uint16_t				uint16;
typedef int32_t					int32;
typedef uint32_t				uint32;
typedef int64_t					int64;
typedef uint64_t				uint64;

#ifndef TRUE
#define TRUE	1
#endif
#ifndef FALSE
#define FALSE	0
#endif

#ifndef PATH_MAX
#define PATH_MAX	1024
#endif

#include "common/fscompat.h"

#define S9xDisplayString	DisplayStringFromBottom

// POSIX path separators (macOS and Android)
#define SLASH_STR	"/"
#define SLASH_CHAR	'/'

#ifndef TITLE
#define TITLE "Snes9x"
#endif

// Byte order: macOS and Android are both little-endian
#define LSB_FIRST
#define FAST_LSB_WORD_ACCESS

// Fast word access macros (little-endian platforms)
#define READ_WORD(s)		(*(uint16 *) (s))
#define READ_3WORD(s)		(*(uint32 *) (s) & 0x00ffffff)
#define READ_DWORD(s)		(*(uint32 *) (s))
#define WRITE_WORD(s, d)	*(uint16 *) (s) = (d)
#define WRITE_3WORD(s, d)	*(uint16 *) (s) = (uint16) (d), *((uint8 *) (s) + 2) = (uint8) ((d) >> 16)
#define WRITE_DWORD(s, d)	*(uint32 *) (s) = (d)

// Byte swap macros
#define SWAP_WORD(s)		((s) = (((s) & 0xff) <<  8) | (((s) & 0xff00) >> 8))
#define SWAP_DWORD(s)		((s) = (((s) & 0xff) << 24) | (((s) & 0xff00) << 8) | (((s) & 0xff0000) >> 8) | (((s) & 0xff000000) >> 24))

#include "ppu/pixform.h"

#endif
