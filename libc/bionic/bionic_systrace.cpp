/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bionic/pthread_internal.h"
#include "private/bionic_lock.h"
#include "private/bionic_systrace.h"
#include "private/bionic_tls.h"
#include "private/CachedProperty.h"

#include <async_safe/log.h>
#include <cutils/trace.h> // For ATRACE_TAG_BIONIC.

static Lock g_lock;
static CachedProperty g_debug_atrace_tags_enableflags("debug.atrace.tags.enableflags");
static uint64_t g_tags;
static int g_trace_marker_fd = -1;

static bool should_trace() {
  g_lock.lock();
  if (g_debug_atrace_tags_enableflags.DidChange()) {
    g_tags = strtoull(g_debug_atrace_tags_enableflags.Get(), nullptr, 0);
  }
  g_lock.unlock();
  return ((g_tags & ATRACE_TAG_BIONIC) != 0);
}

static int get_trace_marker_fd() {
  g_lock.lock();
  if (g_trace_marker_fd == -1) {
    g_trace_marker_fd = open("/sys/kernel/tracing/trace_marker", O_CLOEXEC | O_WRONLY);
    if (g_trace_marker_fd == -1) {
      g_trace_marker_fd = open("/sys/kernel/debug/tracing/trace_marker", O_CLOEXEC | O_WRONLY);
    }
  }
  g_lock.unlock();
  return g_trace_marker_fd;
}

static void trace_begin_internal(const char* message) {
  if (!should_trace()) {
    return;
  }

  int trace_marker_fd = get_trace_marker_fd();
  if (trace_marker_fd == -1) {
    return;
  }

  // Tracing may stop just after checking property and before writing the message.
  // So the write is acceptable to fail. See b/20666100.
  async_safe_format_fd(trace_marker_fd, "B|%d|%s", getpid(), message);
}

void bionic_trace_begin(const char* message) {
  // Some functions called by trace_begin_internal() can call
  // bionic_trace_begin(). Prevent infinite recursion and non-recursive mutex
  // deadlock by using a flag in the thread local storage.
  bionic_tls& tls = __get_bionic_tls();
  if (tls.bionic_systrace_disabled) {
    return;
  }
  tls.bionic_systrace_disabled = true;

  trace_begin_internal(message);

  tls.bionic_systrace_disabled = false;
}

static void trace_end_internal() {
  if (!should_trace()) {
    return;
  }

  int trace_marker_fd = get_trace_marker_fd();
  if (trace_marker_fd == -1) {
    return;
  }

  // This code is intentionally "sub-optimal"; do not optimize this by inlining
  // the E| string into the write.
  //
  // This is because if the const char* string passed to write(trace_marker) is not
  // in resident memory (e.g. the page of the .rodata section that contains it has
  // been paged out, or the anonymous page that contained a heap-based string is
  // swapped in zram), the ftrace code will NOT page it in and instead report
  // <faulted>.
  //
  // We "fix" this by putting the string on the stack, which is more unlikely
  // to be paged out and pass the pointer to that instead.
  //
  // See b/197620214 for more context on this.
  volatile char buf[2]{'E', '|'};
  TEMP_FAILURE_RETRY(write(trace_marker_fd, const_cast<const char*>(buf), 2));
}

void bionic_trace_end() {
  // Some functions called by trace_end_internal() can call
  // bionic_trace_begin(). Prevent infinite recursion and non-recursive mutex
  // deadlock by using a flag in the thread local storage.
  bionic_tls& tls = __get_bionic_tls();
  if (tls.bionic_systrace_disabled) {
    return;
  }
  tls.bionic_systrace_disabled = true;

  trace_end_internal();

  tls.bionic_systrace_disabled = false;
}

ScopedTrace::ScopedTrace(const char* message) : called_end_(false) {
  bionic_trace_begin(message);
}

ScopedTrace::~ScopedTrace() {
  End();
}

void ScopedTrace::End() {
  if (!called_end_) {
    bionic_trace_end();
    called_end_ = true;
  }
}
