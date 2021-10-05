/*
 * Copyright (C) 2015 The Android Open Source Project
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
#include <stdint.h>
#include <sys/cdefs.h>

#include <private/bionic_defs.h>
#include <private/bionic_globals.h>

#include "pthread_internal.h"

class thread_local_dtor {
 public:
  uintptr_t func;
  void *arg;
  void *dso_handle; // unused...
  thread_local_dtor* next;
};

extern "C" int __cxa_thread_atexit_impl(void (*func) (void *), void *arg, void *dso_handle);
extern "C" void __loader_add_thread_local_dtor(void* dso_handle) __attribute__((weak));
extern "C" void __loader_remove_thread_local_dtor(void* dso_handle) __attribute__((weak));

__BIONIC_WEAK_FOR_NATIVE_BRIDGE
int __cxa_thread_atexit_impl(void (*func) (void *), void *arg, void *dso_handle) {
  thread_local_dtor* dtor = new thread_local_dtor();

  dtor->func = __libc_globals->dtor_cookie ^ reinterpret_cast<uintptr_t>(func);
  dtor->arg = arg;
  dtor->dso_handle = dso_handle;

  pthread_internal_t* thread = __get_thread();
  dtor->next = thread->thread_local_dtors;
  thread->thread_local_dtors = dtor;
  if (__loader_add_thread_local_dtor != nullptr) {
    __loader_add_thread_local_dtor(dso_handle);
  }
  return 0;
}

extern "C" __LIBC_HIDDEN__ void __cxa_thread_finalize() {
  pthread_internal_t* thread = __get_thread();
  while (thread->thread_local_dtors != nullptr) {
    thread_local_dtor* current = thread->thread_local_dtors;
    thread->thread_local_dtors = current->next;

    (reinterpret_cast<void (*)(void*)>(__libc_globals->dtor_cookie ^ current->func))(current->arg);
    if (__loader_remove_thread_local_dtor != nullptr) {
      __loader_remove_thread_local_dtor(current->dso_handle);
    }
    delete current;
  }
}
