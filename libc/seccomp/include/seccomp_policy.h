/*
 * Copyright (C) 2016 The Android Open Source Project
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

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <linux/filter.h>

bool set_app_seccomp_filter();
bool set_app_zygote_seccomp_filter();
bool set_system_seccomp_filter();

// Installs a filter that limits setresuid/setresgid to a range of
// [uid_gid_min..uid_gid_max] (for the real-, effective- and super-ids).
bool install_setuidgid_seccomp_filter(uint32_t uid_gid_min, uint32_t uid_gid_max);

// Installs a thread-group-synchronized (TSYNC) BPF filter that prevents
// disabling MTE tag checks via raw prctl(PR_SET_TAGGED_ADDR_CTRL, ...).
// When lock_sync_only is true, downgrading PR_MTE_TCF_SYNC to PR_MTE_TCF_ASYNC
// is also rejected. Uses SECCOMP_RET_KILL_PROCESS when kill_process is true.
bool install_mte_tcf_lock_seccomp_filter(bool lock_sync_only = false, bool kill_process = true);
