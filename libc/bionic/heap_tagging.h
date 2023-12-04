/*
 * Copyright (C) 2019 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include <bionic/pthread_internal.h>
#include <platform/bionic/malloc.h>
#include <stddef.h>

// Expected to be called in a single-threaded context during libc init, so no
// synchronization required.
void SetDefaultHeapTaggingLevel();

// Lock for the heap tagging level. You may find ScopedPthreadMutexLocker
// useful for RAII on this lock.
extern pthread_mutex_t g_heap_tagging_lock;

bool BlockHeapTaggingLevelDowngrade();

// This function can be called in a multithreaded context, and thus should
// only be called when holding the `g_heap_tagging_lock`.
bool SetHeapTaggingLevel(HeapTaggingLevel level);

// This is static because libc_nomalloc uses this but does not need to link the
// cpp file.
__attribute__((unused)) static inline const char* DescribeTaggingLevel(
    HeapTaggingLevel level) {
  switch (level) {
    case M_HEAP_TAGGING_LEVEL_NONE:
      return "none";
    case M_HEAP_TAGGING_LEVEL_TBI:
      return "tbi";
    case M_HEAP_TAGGING_LEVEL_ASYNC:
      return "async";
    case M_HEAP_TAGGING_LEVEL_SYNC:
      return "sync";
  }
}
