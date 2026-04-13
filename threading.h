/* -*- mode: c; tab-width: 4; c-basic-offset: 4; c-file-style: "linux" -*- */
//
// Copyright (c) 2009-2011, Wei Mingzhi <whistler_wmz@users.sf.net>.
// Copyright (c) 2011-2024, SDLPAL development team.
// All rights reserved.
//
// This file is part of SDLPAL.
//
// SDLPAL is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 3
// as published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef THREADING_H
#define THREADING_H

#include "common.h"

PAL_C_LINKAGE_BEGIN

// Kill switch: always FALSE on SDL 1.x; on SDL2+ set TRUE to activate dual-threaded mode.
#if SDL_VERSION_ATLEAST(2,0,0)
extern BOOL          g_bThreadedMode;
#endif
// Cooperative shutdown flag: set TRUE to request all threads to exit.
extern volatile BOOL g_bThreadQuit;
// Set TRUE by PAL_GameMainThread just before it returns (logic thread finished).
extern volatile BOOL g_bLogicThreadDone;

#if SDL_VERSION_ATLEAST(2,0,0)

//
// SDL atomic type alias for SDL2/SDL3 compatibility.
//
#if SDL_VERSION_ATLEAST(3,0,0)
typedef SDL_AtomicInt THREADING_ATOMIC_INT;
#else
typedef SDL_atomic_t THREADING_ATOMIC_INT;
#endif

typedef struct tagFRAMEBUFFER_SLOT
{
   SDL_Surface            *pSurface;            // 8-bit indexed surface (320×200)
   SDL_Surface            *pDirectSurface;      // 32-bit ARGB8888 surface (320×200, AVI)
#if SDL_VERSION_ATLEAST(3,0,0)
   SDL_Palette            *pPalette;            // Palette for pSurface (SDL3 only)
#endif
   WORD                   wShakeTime;           // Remaining shake frames
   WORD                   wShakeLevel;          // Shake amplitude in pixels
   BOOL                   bDirectColor;         // TRUE = pDirectSurface (AVI), FALSE = indexed
} FRAMEBUFFER_SLOT;

typedef struct tagTRIPLE_BUFFER
{
   FRAMEBUFFER_SLOT       slots[3];             // Three frame slots
   THREADING_ATOMIC_INT   readyIndex;           // Index of latest completed frame (-1 = none)
   THREADING_ATOMIC_INT   readyGeneration;      // Monotonic counter, incremented on each publish
   THREADING_ATOMIC_INT   readIndex;            // Index currently held by render thread (-1 = none)
   INT                    writeIndex;           // Index currently used by logic thread (logic-thread-only)
   INT                    lastReadGeneration;   // Generation last consumed by render thread (render-thread-only)
} TRIPLE_BUFFER;

VOID
THREADING_Init(
   VOID
);

VOID
THREADING_Shutdown(
   VOID
);

FRAMEBUFFER_SLOT *
THREADING_AcquireWriteSlot(
   VOID
);

VOID
THREADING_PublishFrame(
   VOID
);

FRAMEBUFFER_SLOT *
THREADING_AcquireReadSlot(
   VOID
);

VOID
THREADING_ReleaseReadSlot(
   VOID
);

SDL_Thread *
THREADING_CreateLogicThread(
   SDL_ThreadFunction  fn,
   LPVOID              data
);

INT
THREADING_JoinLogicThread(
   SDL_Thread         *thread
);

#endif /* SDL_VERSION_ATLEAST(2,0,0) */

PAL_C_LINKAGE_END

#endif
