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

#include "main.h"
#include "threading.h"

// g_bThreadedMode and g_bThreadQuit are always present so that all callers
// can reference them unconditionally regardless of SDL version.
#if SDL_VERSION_ATLEAST(2,0,0)
BOOL g_bThreadedMode = FALSE;
#endif
volatile BOOL g_bThreadQuit = FALSE;
volatile BOOL g_bLogicThreadDone = FALSE;

#if SDL_VERSION_ATLEAST(2,0,0)

#if SDL_VERSION_ATLEAST(3,0,0)
#define THREADING_ATOMIC_GET SDL_GetAtomicInt
#define THREADING_ATOMIC_SET SDL_SetAtomicInt
#define THREADING_ATOMIC_CAS SDL_CompareAndSwapAtomicInt
#else
#define THREADING_ATOMIC_GET SDL_AtomicGet
#define THREADING_ATOMIC_SET SDL_AtomicSet
#define THREADING_ATOMIC_CAS SDL_AtomicCAS
#endif

static TRIPLE_BUFFER *gpTripleBuffer = NULL;

static INT
THREADING_FindWritableSlot(
   INT iStartIndex
)
{
   INT i;
   INT iReadyIndex;
   INT iReadIndex;

   if (gpTripleBuffer == NULL)
   {
      return -1;
   }

   iReadyIndex = THREADING_ATOMIC_GET(&gpTripleBuffer->readyIndex);
   iReadIndex = THREADING_ATOMIC_GET(&gpTripleBuffer->readIndex);

   for (i = 0; i < 3; i++)
   {
      INT iCandidate = (iStartIndex + i) % 3;

      if (iCandidate != iReadyIndex && iCandidate != iReadIndex)
      {
         return iCandidate;
      }
   }

   return -1;
}

VOID
THREADING_Init(
   VOID
)
/*++
  Purpose:

    Initialize the triple buffer state and thread control flags.

  Parameters:

    None.

  Return value:

    None.

--*/
{
   if (gpTripleBuffer != NULL)
   {
      THREADING_Shutdown();
   }

   gpTripleBuffer = (TRIPLE_BUFFER *)UTIL_calloc(1, sizeof(TRIPLE_BUFFER));
   if (gpTripleBuffer == NULL)
   {
      return;
   }

   //
   // Create SDL_Surfaces for each triple-buffer slot so that the render
   // thread can blit directly from the slot without an intermediate copy.
   //
   {
      INT i;
      for (i = 0; i < 3; i++)
      {
         FRAMEBUFFER_SLOT *pSlot = &gpTripleBuffer->slots[i];

         pSlot->pSurface = SDL_CreateRGBSurface(SDL_SWSURFACE, 320, 200, 8,
            0, 0, 0, 0);
         pSlot->pDirectSurface = SDL_CreateRGBSurface(SDL_SWSURFACE, 320, 200, 32,
            0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
#if SDL_VERSION_ATLEAST(3,0,0)
         if (pSlot->pSurface != NULL)
         {
            pSlot->pPalette = SDL_CreateSurfacePalette(pSlot->pSurface);
         }
#endif

         if (pSlot->pSurface == NULL || pSlot->pDirectSurface == NULL)
         {
            THREADING_Shutdown();
            return;
         }
      }
   }

   THREADING_ATOMIC_SET(&gpTripleBuffer->readyIndex, -1);
   THREADING_ATOMIC_SET(&gpTripleBuffer->readyGeneration, 0);
   THREADING_ATOMIC_SET(&gpTripleBuffer->readIndex, -1);
   gpTripleBuffer->writeIndex = 0;
   gpTripleBuffer->lastReadGeneration = 0;

#if SDL_VERSION_ATLEAST(2,0,0)
   g_bThreadedMode = FALSE;
#endif
   g_bThreadQuit = FALSE;
}

VOID
THREADING_Shutdown(
   VOID
)
/*++
  Purpose:

    Release the triple buffer state.

  Parameters:

    None.

  Return value:

    None.

--*/
{
   if (gpTripleBuffer == NULL)
   {
      return;
   }

   THREADING_ATOMIC_SET(&gpTripleBuffer->readIndex, -1);
   THREADING_ATOMIC_SET(&gpTripleBuffer->readyIndex, -1);
   THREADING_ATOMIC_SET(&gpTripleBuffer->readyGeneration, 0);
   gpTripleBuffer->writeIndex = 0;
   gpTripleBuffer->lastReadGeneration = 0;

   {
      INT i;
      for (i = 0; i < 3; i++)
      {
         FRAMEBUFFER_SLOT *pSlot = &gpTripleBuffer->slots[i];
         if (pSlot->pSurface != NULL)
         {
            SDL_FreeSurface(pSlot->pSurface);
            pSlot->pSurface = NULL;
         }
         if (pSlot->pDirectSurface != NULL)
         {
            SDL_FreeSurface(pSlot->pDirectSurface);
            pSlot->pDirectSurface = NULL;
         }
      }
   }

   free(gpTripleBuffer);
   gpTripleBuffer = NULL;
}

FRAMEBUFFER_SLOT *
THREADING_AcquireWriteSlot(
   VOID
)
/*++
  Purpose:

    Acquire a writable frame slot for the logic thread.

  Parameters:

    None.

  Return value:

    Pointer to a writable slot, or NULL if no slot is available.

--*/
{
   INT iPreferredSlot;
   INT iWritableSlot;

   if (gpTripleBuffer == NULL)
   {
      return NULL;
   }

   iPreferredSlot = THREADING_ATOMIC_GET(&gpTripleBuffer->readyIndex);
   if (iPreferredSlot < 0)
   {
      iPreferredSlot = gpTripleBuffer->writeIndex;
   }
   else
   {
      iPreferredSlot = (iPreferredSlot + 1) % 3;
   }

   iWritableSlot = THREADING_FindWritableSlot(iPreferredSlot);
   if (iWritableSlot < 0)
   {
      return NULL;
   }

   gpTripleBuffer->writeIndex = iWritableSlot;
   return &gpTripleBuffer->slots[iWritableSlot];
}

VOID
THREADING_PublishFrame(
   VOID
)
/*++
  Purpose:

    Publish the latest completed frame for the render thread.

  Parameters:

    None.

  Return value:

    None.

--*/
{
   INT iNextWriteSlot;
   INT iGeneration;

   if (gpTripleBuffer == NULL || gpTripleBuffer->writeIndex < 0 || gpTripleBuffer->writeIndex >= 3)
   {
      return;
   }

   THREADING_ATOMIC_SET(&gpTripleBuffer->readyIndex, gpTripleBuffer->writeIndex);

   iGeneration = THREADING_ATOMIC_GET(&gpTripleBuffer->readyGeneration);
   THREADING_ATOMIC_SET(&gpTripleBuffer->readyGeneration, iGeneration + 1);

   iNextWriteSlot = THREADING_FindWritableSlot((gpTripleBuffer->writeIndex + 1) % 3);
   gpTripleBuffer->writeIndex = iNextWriteSlot;
}

FRAMEBUFFER_SLOT *
THREADING_AcquireReadSlot(
   VOID
)
/*++
  Purpose:

    Acquire the latest published frame slot for the render thread.

  Parameters:

    None.

  Return value:

    Pointer to the latest readable slot, or NULL if no new frame exists.

--*/
{
   INT iCurrentReadSlot;
   INT iGeneration;
   INT iReadySlot;

   if (gpTripleBuffer == NULL)
   {
      return NULL;
   }

   iCurrentReadSlot = THREADING_ATOMIC_GET(&gpTripleBuffer->readIndex);
   if (iCurrentReadSlot >= 0 && iCurrentReadSlot < 3)
   {
      return &gpTripleBuffer->slots[iCurrentReadSlot];
   }

   iGeneration = THREADING_ATOMIC_GET(&gpTripleBuffer->readyGeneration);
   if (iGeneration == gpTripleBuffer->lastReadGeneration)
   {
      return NULL;
   }

   iReadySlot = THREADING_ATOMIC_GET(&gpTripleBuffer->readyIndex);
   if (iReadySlot < 0 || iReadySlot >= 3)
   {
      return NULL;
   }

   if (!THREADING_ATOMIC_CAS(&gpTripleBuffer->readIndex, -1, iReadySlot))
   {
      iCurrentReadSlot = THREADING_ATOMIC_GET(&gpTripleBuffer->readIndex);
      if (iCurrentReadSlot >= 0 && iCurrentReadSlot < 3)
      {
         return &gpTripleBuffer->slots[iCurrentReadSlot];
      }

      return NULL;
   }

   gpTripleBuffer->lastReadGeneration = iGeneration;
   return &gpTripleBuffer->slots[iReadySlot];
}

VOID
THREADING_ReleaseReadSlot(
   VOID
)
/*++
  Purpose:

    Release the slot currently held by the render thread.

  Parameters:

    None.

  Return value:

    None.

--*/
{
   if (gpTripleBuffer == NULL)
   {
      return;
   }

   THREADING_ATOMIC_SET(&gpTripleBuffer->readIndex, -1);
}

SDL_Thread *
THREADING_CreateLogicThread(
   SDL_ThreadFunction fn,
   LPVOID             data
)
/*++
  Purpose:

    Create the logic thread.

  Parameters:

    [IN]  fn - Thread entry point.
    [IN]  data - User-supplied thread data.

  Return value:

    Pointer to the created thread, or NULL on failure.

--*/
{
   return SDL_CreateThread(fn, "LogicThread", data);
}

INT
THREADING_JoinLogicThread(
   SDL_Thread        *thread
)
/*++
  Purpose:

    Wait for the logic thread to exit.

  Parameters:

    [IN]  thread - Thread handle returned by THREADING_CreateLogicThread().

  Return value:

    The thread exit status.

--*/
{
   INT iThreadStatus = 0;

   if (thread != NULL)
   {
      SDL_WaitThread(thread, &iThreadStatus);
   }

   return iThreadStatus;
}

#endif /* SDL_VERSION_ATLEAST(2,0,0) */
