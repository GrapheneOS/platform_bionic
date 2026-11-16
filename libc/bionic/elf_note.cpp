/*
 * Copyright (C) 2024 The Android Open Source Project
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

#include "platform/bionic/macros.h"
#include "private/elf_note.h"

#include <string.h>

bool __get_elf_note(unsigned note_type, const char* note_name, const ElfW(Addr) note_addr,
                    const ElfW(Phdr)* phdr_note, const ElfW(Nhdr)** note_hdr,
                    const char** note_desc) {
  if (phdr_note->p_type != PT_NOTE || !note_name || !note_addr ||
      phdr_note->p_filesz > phdr_note->p_memsz) {
    return false;
  }

  size_t note_name_len = strlen(note_name) + 1;

  ElfW(Addr) p = note_addr;
  ElfW(Addr) note_end;
  if (__builtin_add_overflow(p, phdr_note->p_filesz, &note_end)) {
    return false;
  }
  while (p < note_end) {
    // Parse the note and check it's structurally valid.
    if (note_end - p < sizeof(ElfW(Nhdr))) {
      return false;
    }
    const ElfW(Nhdr)* note = reinterpret_cast<const ElfW(Nhdr)*>(p);
    p += sizeof(ElfW(Nhdr));

    size_t aligned_namesz = 0;
    if (__builtin_add_overflow(static_cast<size_t>(note->n_namesz), size_t{3}, &aligned_namesz)) {
      return false;
    }
    aligned_namesz &= ~size_t{3};
    if (note_end - p < aligned_namesz) {
      return false;
    }
    const char* name = reinterpret_cast<const char*>(p);
    p += aligned_namesz;

    size_t aligned_descsz = 0;
    if (__builtin_add_overflow(static_cast<size_t>(note->n_descsz), size_t{3}, &aligned_descsz)) {
      return false;
    }
    aligned_descsz &= ~size_t{3};
    if (note_end - p < aligned_descsz) {
      return false;
    }
    const char* desc = reinterpret_cast<const char*>(p);
    p += aligned_descsz;

    // Is this the note we're looking for?
    if (note->n_type == note_type &&
        note->n_namesz == note_name_len &&
        strncmp(note_name, name, note_name_len) == 0) {
      *note_hdr = note;
      *note_desc = desc;
      return true;
    }
  }
  return false;
}

bool __find_elf_note(unsigned int note_type, const char* note_name, const ElfW(Phdr)* phdr_start,
                     size_t phdr_ct, const ElfW(Nhdr)** note_hdr, const char** note_desc,
                     const ElfW(Addr) load_bias) {
  for (size_t i = 0; i < phdr_ct; ++i) {
    const ElfW(Phdr)* phdr = &phdr_start[i];
    if (phdr->p_type != PT_NOTE) {
      continue;
    }

    ElfW(Addr) note_addr;
    if (__builtin_add_overflow(load_bias, phdr->p_vaddr, &note_addr)) {
      continue;
    }
    if (__get_elf_note(note_type, note_name, note_addr, phdr, note_hdr, note_desc)) {
      return true;
    }
  }

  return false;
}
