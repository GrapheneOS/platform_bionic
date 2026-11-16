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

#include "heap_tagging.h"
#include "malloc_common.h"
#include "malloc_tagged_pointers.h"

#include <bionic/pthread_internal.h>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <platform/bionic/malloc.h>
#include <platform/bionic/mte.h>
#include <sanitizer/hwasan_interface.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/auxv.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

extern "C" void h_malloc_disable_memory_tagging();

extern "C" void scudo_malloc_disable_memory_tagging();
extern "C" void scudo_malloc_set_track_allocation_stacks(int);

extern "C" const char* __scudo_get_stack_depot_addr();
extern "C" const char* __scudo_get_ring_buffer_addr();
extern "C" size_t __scudo_get_ring_buffer_size();
extern "C" size_t __scudo_get_stack_depot_size();

// Protected by `g_heap_tagging_lock`.
static HeapTaggingLevel heap_tagging_level = M_HEAP_TAGGING_LEVEL_NONE;

void SetDefaultHeapTaggingLevel() {
#if defined(__aarch64__)
#if !__has_feature(hwaddress_sanitizer)
  heap_tagging_level = __libc_shared_globals()->initial_heap_tagging_level;
#endif

  __libc_memtag_stack_abi = __libc_shared_globals()->initial_memtag_stack_abi;

  __libc_globals.mutate([](libc_globals* globals) {
    switch (heap_tagging_level) {
      case M_HEAP_TAGGING_LEVEL_TBI:
        // Arrange for us to set pointer tags to POINTER_TAG, check tags on
        // deallocation and untag when passing pointers to the allocator.
        globals->heap_pointer_tag = (reinterpret_cast<uintptr_t>(POINTER_TAG) << TAG_SHIFT) |
                                    (0xffull << CHECK_SHIFT) | (0xffull << UNTAG_SHIFT);
        break;
      case M_HEAP_TAGGING_LEVEL_SYNC:
      case M_HEAP_TAGGING_LEVEL_ASYNC:
        atomic_store(&globals->memtag, true);
        atomic_store(&__libc_memtag_stack, __libc_shared_globals()->initial_memtag_stack);
        break;
      default:
        break;
    };
  });


  switch (heap_tagging_level) {
    case M_HEAP_TAGGING_LEVEL_TBI:
    case M_HEAP_TAGGING_LEVEL_NONE:
#if defined(USE_SCUDO)
      scudo_malloc_disable_memory_tagging();
#endif
#if defined(USE_H_MALLOC)
      h_malloc_disable_memory_tagging();
#endif
      break;
    case M_HEAP_TAGGING_LEVEL_SYNC:
#if defined(USE_SCUDO)
      scudo_malloc_set_track_allocation_stacks(1);
#endif
      break;
    default:
      break;
  }

#endif  // aarch64
}

static bool set_tcf_on_all_threads(int tcf) {
  return android_run_on_all_threads(
      [](void* arg) {
        int tcf = *reinterpret_cast<int*>(arg);
        int tagged_addr_ctrl = prctl(PR_GET_TAGGED_ADDR_CTRL, 0, 0, 0, 0);
        if (tagged_addr_ctrl < 0) {
          return false;
        }

        tagged_addr_ctrl = (tagged_addr_ctrl & ~PR_MTE_TCF_MASK) | tcf;
        return prctl(PR_SET_TAGGED_ADDR_CTRL, tagged_addr_ctrl, 0, 0, 0) >= 0;
      },
      &tcf);
}

pthread_mutex_t g_heap_tagging_lock = PTHREAD_MUTEX_INITIALIZER;

static bool block_heap_tagging_level_downgrade;

#if defined(__aarch64__) && !__has_feature(hwaddress_sanitizer)
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define SECCOMP_ARG_LOW32(n) (offsetof(struct seccomp_data, args[n]) + sizeof(__u32))
#define SECCOMP_ARG_HIGH32(n) (offsetof(struct seccomp_data, args[n]))
#else
#define SECCOMP_ARG_LOW32(n) (offsetof(struct seccomp_data, args[n]))
#define SECCOMP_ARG_HIGH32(n) (offsetof(struct seccomp_data, args[n]) + sizeof(__u32))
#endif

// Installs a thread-group-synchronized (TSYNC) seccomp-BPF filter that enforces
// the heap tagging downgrade lock in the kernel against raw prctl() syscalls.
// Uses SECCOMP_RET_KILL_PROCESS so SIGSYS handlers (including profiling SIGSYS
// overrides) cannot catch or suppress MTE downgrade attempts.
static bool InstallMteDowngradeLockSeccompFilter(HeapTaggingLevel current_level) {
  if (current_level == M_HEAP_TAGGING_LEVEL_NONE) {
    return true;
  }

  const bool require_mte = (current_level == M_HEAP_TAGGING_LEVEL_ASYNC ||
                            current_level == M_HEAP_TAGGING_LEVEL_SYNC);
  int cur_ctrl = prctl(PR_GET_TAGGED_ADDR_CTRL, 0, 0, 0, 0);

  uint32_t required_bits = PR_TAGGED_ADDR_ENABLE;
  uint32_t forbidden_bits = 0;
  uint32_t tcf_any_mask = PR_TAGGED_ADDR_ENABLE;

  if (require_mte) {
    uint32_t active_tag_mask =
        (cur_ctrl >= 0) ? (static_cast<uint32_t>(cur_ctrl) & PR_MTE_TAG_MASK) : 0;
    required_bits |= (active_tag_mask != 0) ? active_tag_mask
                                            : static_cast<uint32_t>(PR_MTE_TAG_SET_NONZERO);
    forbidden_bits |= PR_MTE_STORE_ONLY;
    tcf_any_mask = PR_MTE_TCF_MASK;
  }

  // Exact 18-instruction cBPF filter:
  //   - Non-prctl syscalls take 5 instructions -> ALLOW.
  //   - Unrelated prctl() options take 7 instructions -> ALLOW.
  //   - Matches low 32-bit `int option` == PR_SET_TAGGED_ADDR_CTRL (matching
  //     SYSCALL_DEFINE5(prctl, int, option, ...) 32-bit cast semantics) and
  //     kills on non-zero upper 32 bits of args[0] or args[1], PR_MTE_STORE_ONLY,
  //     clearing PR_MTE_TCF_SYNC|PR_MTE_TCF_ASYNC (PR_MTE_TCF_NONE), clearing
  //     PR_TAGGED_ADDR_ENABLE, or weakening the active PR_MTE_TAG_MASK.
  struct sock_filter filter[] = {
      // [0] Load seccomp_data.arch
      BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, arch)),
      // [1] If arch == AUDIT_ARCH_AARCH64 goto [3]
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_AARCH64, 1, 0),
      // [2] Allow compat AUDIT_ARCH_ARM to pass to base filter; kill unknown arch
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_ARM, 13, 14),
      // [3] Load seccomp_data.nr
      BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr)),
      // [4] If nr != __NR_prctl goto [16] ALLOW
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_prctl, 0, 11),
      // [5] Load low 32-bit word of args[0] (`int option`)
      BPF_STMT(BPF_LD | BPF_W | BPF_ABS, SECCOMP_ARG_LOW32(0)),
      // [6] If option != PR_SET_TAGGED_ADDR_CTRL goto [16] ALLOW
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, PR_SET_TAGGED_ADDR_CTRL, 0, 9),
      // [7] Reject non-zero high 32-bit word in args[0] (upper-bit smuggling)
      BPF_STMT(BPF_LD | BPF_W | BPF_ABS, SECCOMP_ARG_HIGH32(0)),
      // [8] If high32(args[0]) != 0 goto [17] KILL_PROCESS
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 0, 0, 8),
      // [9] Reject non-zero high 32-bit word in args[1] (`unsigned long arg2`)
      BPF_STMT(BPF_LD | BPF_W | BPF_ABS, SECCOMP_ARG_HIGH32(1)),
      // [10] If high32(args[1]) != 0 goto [17] KILL_PROCESS
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 0, 0, 6),
      // [11] Load low 32-bit word of args[1] (`arg2`)
      BPF_STMT(BPF_LD | BPF_W | BPF_ABS, SECCOMP_ARG_LOW32(1)),
      // [12] Reject forbidden flags (e.g. PR_MTE_STORE_ONLY) -> [17] KILL_PROCESS
      BPF_JUMP(BPF_JMP | BPF_JSET | BPF_K, forbidden_bits, 4, 0),
      // [13] Require PR_MTE_TCF_SYNC or PR_MTE_TCF_ASYNC (reject PR_MTE_TCF_NONE) -> [17]
      BPF_JUMP(BPF_JMP | BPF_JSET | BPF_K, tcf_any_mask, 0, 3),
      // [14] Verify PR_TAGGED_ADDR_ENABLE and active PR_MTE_TAG_MASK bits remain set
      BPF_STMT(BPF_ALU | BPF_AND | BPF_K, required_bits),
      // [15] If (arg2 & required_bits) == required_bits goto [16] ALLOW else [17] KILL
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, required_bits, 0, 1),
      // [16] ALLOW
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
      // [17] Uncatchable process-wide termination
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
  };

  struct sock_fprog prog = {
      .len = static_cast<unsigned short>( sizeof(filter) / sizeof(filter[0]) ),
      .filter = filter,
  };

  if (syscall(__NR_seccomp, SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_TSYNC, &prog) == 0) {
    return true;
  }

  warning_log("BlockHeapTaggingLevelDowngrade: seccomp TSYNC filter install failed: %m");
  return false;
}
#endif  // defined(__aarch64__) && !__has_feature(hwaddress_sanitizer)

// Requires `g_heap_tagging_lock` to be held.
bool BlockHeapTaggingLevelDowngrade() {
  if (block_heap_tagging_level_downgrade) {
    return false;
  }
  block_heap_tagging_level_downgrade = true;
#if defined(__aarch64__) && !__has_feature(hwaddress_sanitizer)
  InstallMteDowngradeLockSeccompFilter(heap_tagging_level);
#endif
  return true;
}

// Requires `g_heap_tagging_lock` to be held.
bool SetHeapTaggingLevel(HeapTaggingLevel tag_level) {
  if (tag_level == heap_tagging_level) {
    return true;
  }

  if (block_heap_tagging_level_downgrade) {
    // allow switching between SYNC and ASYNC, but don't allow disabling memory tagging
    if (tag_level < heap_tagging_level && tag_level != M_HEAP_TAGGING_LEVEL_ASYNC) {
      error_log("SetHeapTaggingLevel: blocked downgrade of tag level from %i to %i", heap_tagging_level, tag_level);
      return false;
    }
  }

  switch (tag_level) {
    case M_HEAP_TAGGING_LEVEL_NONE:
      __libc_globals.mutate([](libc_globals* globals) {
        if (heap_tagging_level == M_HEAP_TAGGING_LEVEL_TBI) {
          // Preserve the untag mask (we still want to untag pointers when passing them to the
          // allocator), but clear the fixed tag and the check mask, so that pointers are no longer
          // tagged and checks no longer happen.
          globals->heap_pointer_tag = static_cast<uintptr_t>(0xffull << UNTAG_SHIFT);
        }
        atomic_store(&__libc_memtag_stack, false);
        atomic_store(&globals->memtag, false);
        atomic_store(&__libc_shared_globals()->memtag_currently_on, false);
      });

      if (heap_tagging_level != M_HEAP_TAGGING_LEVEL_TBI) {
        if (!set_tcf_on_all_threads(PR_MTE_TCF_NONE)) {
          error_log("SetHeapTaggingLevel: set_tcf_on_all_threads failed");
          return false;
        }
      }
#if defined(USE_SCUDO) && !__has_feature(hwaddress_sanitizer)
      scudo_malloc_disable_memory_tagging();
#endif
#if defined(USE_H_MALLOC)
      h_malloc_disable_memory_tagging();
#endif
      break;
    case M_HEAP_TAGGING_LEVEL_TBI:
    case M_HEAP_TAGGING_LEVEL_ASYNC:
    case M_HEAP_TAGGING_LEVEL_SYNC:
      if (heap_tagging_level == M_HEAP_TAGGING_LEVEL_NONE) {
#if !__has_feature(hwaddress_sanitizer)
        // Suppress the error message in HWASan builds. Apps can try to enable TBI (or even MTE
        // modes) being unaware of HWASan, fail them silently.
        error_log(
            "SetHeapTaggingLevel: re-enabling tagging after it was disabled is not supported");
#endif
        return false;
      } else if (tag_level == M_HEAP_TAGGING_LEVEL_TBI ||
                 heap_tagging_level == M_HEAP_TAGGING_LEVEL_TBI) {
        error_log("SetHeapTaggingLevel: switching between TBI and ASYNC/SYNC is not supported");
        return false;
      }

      if (tag_level == M_HEAP_TAGGING_LEVEL_ASYNC) {
        // When entering ASYNC mode, specify that we want to allow upgrading to SYNC by OR'ing in
        // the SYNC flag. But if the kernel doesn't support specifying multiple TCF modes, fall back
        // to specifying a single mode.
        if (!set_tcf_on_all_threads(PR_MTE_TCF_ASYNC | PR_MTE_TCF_SYNC)) {
          set_tcf_on_all_threads(PR_MTE_TCF_ASYNC);
        }
#if defined(USE_SCUDO) && !__has_feature(hwaddress_sanitizer)
        scudo_malloc_set_track_allocation_stacks(0);
#endif
      } else if (tag_level == M_HEAP_TAGGING_LEVEL_SYNC) {
        set_tcf_on_all_threads(PR_MTE_TCF_SYNC);
#if defined(USE_SCUDO) && !__has_feature(hwaddress_sanitizer)
        scudo_malloc_set_track_allocation_stacks(1);
        __libc_shared_globals()->scudo_ring_buffer = __scudo_get_ring_buffer_addr();
        __libc_shared_globals()->scudo_ring_buffer_size = __scudo_get_ring_buffer_size();
        __libc_shared_globals()->scudo_stack_depot = __scudo_get_stack_depot_addr();
        __libc_shared_globals()->scudo_stack_depot_size = __scudo_get_stack_depot_size();
#endif
      }
      break;
    default:
      error_log("SetHeapTaggingLevel: unknown tagging level");
      return false;
  }

  heap_tagging_level = tag_level;
  info_log("SetHeapTaggingLevel: tag level set to %d", tag_level);

  return true;
}

#ifdef __aarch64__
static inline __attribute__((no_sanitize("memtag"))) void untag_memory(void* from, void* to) {
  if (from == to) {
    return;
  }
  __asm__ __volatile__(
      ".arch_extension mte\n"
      "1:\n"
      "stg %[Ptr], [%[Ptr]], #16\n"
      "cmp %[Ptr], %[End]\n"
      "b.lt 1b\n"
      : [Ptr] "+&r"(from)
      : [End] "r"(to)
      : "memory");
}
#endif

#ifdef __aarch64__
// 128Mb of stack should be enough for anybody.
static constexpr size_t kUntagLimit = 128 * 1024 * 1024;
#endif  // __aarch64__

extern "C" __LIBC_HIDDEN__ __attribute__((no_sanitize("memtag"))) void memtag_handle_longjmp(
    void* sp_dst __unused, void* sp_src __unused) {
  // A usual longjmp looks like this, where sp_dst was the LR in the call to setlongjmp (i.e.
  // the SP of the frame calling setlongjmp).
  // ┌─────────────────────┐                  │
  // │                     │                  │
  // ├─────────────────────┤◄──────── sp_dst  │ stack
  // │         ...         │                  │ grows
  // ├─────────────────────┤                  │ to lower
  // │         ...         │                  │ addresses
  // ├─────────────────────┤◄──────── sp_src  │
  // │siglongjmp           │                  │
  // ├─────────────────────┤                  │
  // │memtag_handle_longjmp│                  │
  // └─────────────────────┘                  ▼
#ifdef __aarch64__
  if (atomic_load(&__libc_memtag_stack)) {
    size_t distance = reinterpret_cast<uintptr_t>(sp_dst) - reinterpret_cast<uintptr_t>(sp_src);
    if (distance > kUntagLimit) {
      async_safe_fatal(
          "memtag_handle_longjmp: stack adjustment too large! %p -> %p, distance %zx > %zx\n",
          sp_src, sp_dst, distance, kUntagLimit);
    } else {
      untag_memory(sp_src, sp_dst);
    }
  }
#endif  // __aarch64__

  // We can use __has_feature here rather than __hwasan_handle_longjmp as a
  // weak symbol because this is part of libc which is always sanitized for a
  // hwasan enabled process.
#if __has_feature(hwaddress_sanitizer)
  __hwasan_handle_longjmp(sp_dst);
#endif  // __has_feature(hwaddress_sanitizer)
}
