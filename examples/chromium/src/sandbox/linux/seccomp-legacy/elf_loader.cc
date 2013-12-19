// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <elf.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "sandbox_impl.h"
#include "syscall_entrypoint.h"
#include "tls_setup.h"


#if defined(__x86_64__)
# define ElfW(name) Elf64_##name
# define ElfW_ELFCLASS ELFCLASS64
# define ElfW_EXPECTED_MACHINE EM_X86_64
#elif defined(__i386__)
# define ElfW(name) Elf32_##name
# define ElfW_ELFCLASS ELFCLASS32
# define ElfW_EXPECTED_MACHINE EM_386
#else
# error Unsupported target platform
#endif


static uintptr_t PageSizeRoundDown(uintptr_t val) {
  return val & ~(getpagesize() - 1);
}

static uintptr_t PageSizeRoundUp(uintptr_t val) {
  return PageSizeRoundDown(val + getpagesize() - 1);
}

static ElfW(auxv_t) *FindAuxv(int argc, char **argv) {
  char **ptr = argv + argc + 1;
  // Skip over envp.
  while (*ptr != NULL) {
    ptr++;
  }
  ptr++;
  return (ElfW(auxv_t) *) ptr;
}

static void SetAuxvField(ElfW(auxv_t) *auxv, unsigned type, uintptr_t value) {
  for (; auxv->a_type != AT_NULL; auxv++) {
    if (auxv->a_type == type) {
      auxv->a_un.a_val = value;
      return;
    }
  }
}

static void JumpToElfEntryPoint(void *stack, void *entry_point,
                                void *atexit_func) {
#if defined(__x86_64__)
  asm("mov %0, %%rsp\n"
      "jmp *%1\n"
      // %edx is registered as an atexit handler if non-zero.
      : : "r"(stack), "r"(entry_point), "d"(atexit_func));
#elif defined(__i386__)
  asm("mov %0, %%esp\n"
      "jmp *%1\n"
      // %rdx is registered as an atexit handler if non-zero.
      : : "r"(stack), "r"(entry_point), "d"(atexit_func));
#else
# error Unsupported target platform
#endif
}

static void *LoadElfObject(int fd, ElfW(auxv_t) *auxv) {
  // Load and check headers.
  ElfW(Ehdr) elf_header;
  if (pread(fd, &elf_header, sizeof(elf_header), 0) != sizeof(elf_header)) {
    Sandbox::die("Failed to read ELF header");
  }
  if (memcmp(elf_header.e_ident, ELFMAG, SELFMAG) != 0) {
    Sandbox::die("Not an ELF file");
  }
  if (elf_header.e_ident[EI_CLASS] != ElfW_ELFCLASS) {
    Sandbox::die("Unexpected ELFCLASS");
  }
  if (elf_header.e_machine != ElfW_EXPECTED_MACHINE) {
    Sandbox::die("Unexpected ELF machine type");
  }
  if (elf_header.e_phentsize != sizeof(ElfW(Phdr))) {
    Sandbox::die("Unexpected ELF program header entry size");
  }
  if (elf_header.e_phnum >= 20) {
    // We impose an arbitrary limit as a sanity check and to avoid
    // overflowing the stack.
    Sandbox::die("Too many ELF program headers");
  }
  ElfW(Phdr) phdrs[elf_header.e_phnum];
  if (pread(fd, phdrs, sizeof(phdrs), elf_header.e_phoff)
      != (ssize_t) sizeof(phdrs)) {
    Sandbox::die("Failed to read ELF program headers");
  }

  // Scan program headers to find the overall size of the ELF object.
  // Find the first and last PT_LOAD segments.  ELF requires that
  // PT_LOAD segments be in ascending order of p_vaddr, so we can use
  // the last one to calculate the whole address span of the image.
  size_t index = 0;
  while (index < elf_header.e_phnum && phdrs[index].p_type != PT_LOAD) {
    index++;
  }
  if (index == elf_header.e_phnum) {
    Sandbox::die("ELF object contains no PT_LOAD headers");
  }
  ElfW(Phdr) *first_segment = &phdrs[index];
  ElfW(Phdr) *last_segment = &phdrs[elf_header.e_phnum - 1];
  while (last_segment > first_segment && last_segment->p_type != PT_LOAD) {
    last_segment--;
  }
  uintptr_t overall_start = PageSizeRoundDown(first_segment->p_vaddr);
  uintptr_t overall_end = PageSizeRoundUp(last_segment->p_vaddr
                                          + last_segment->p_memsz);
  uintptr_t overall_size = overall_end - overall_start;

  // Reserve address space.
  // Executables that must be loaded at a fixed address have an e_type
  // of ET_EXEC.  For these, we could use MAP_FIXED, but if the
  // address range is already occupied then that will clobber the
  // existing mappings without warning, which is bad.  Instead, use an
  // address hint and check that we got the expected address.
  // Executables that can be loaded at any address have an e_type of
  // ET_DYN.
  char *required_start =
    elf_header.e_type == ET_EXEC ? (char *) overall_start : NULL;
  char *base_addr = (char *) mmap(required_start, overall_size, PROT_NONE,
                                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (base_addr == MAP_FAILED) {
    Sandbox::die("Failed to reserve address space");
  }
  if (elf_header.e_type == ET_EXEC && base_addr != required_start) {
    Sandbox::die("Failed to reserve address space at fixed address");
  }

  char *load_offset = (char *) (base_addr - required_start);
  char *entry_point = load_offset + elf_header.e_entry;
  SetAuxvField(auxv, AT_ENTRY, (uintptr_t) entry_point);
  SetAuxvField(auxv, AT_BASE, (uintptr_t) load_offset);
  SetAuxvField(auxv, AT_PHNUM, elf_header.e_phnum);
  SetAuxvField(auxv, AT_PHENT, elf_header.e_phentsize);
  // Note that this assumes that the program headers are included in a
  // PT_LOAD segment for which the file offsets matches the mapping
  // offset, but Linux assumes this too when setting AT_PHDR.
  SetAuxvField(auxv, AT_PHDR, (uintptr_t) base_addr + elf_header.e_phoff);

  for (ElfW(Phdr) *segment = first_segment;
       segment <= last_segment;
       segment++) {
    if (segment->p_type == PT_LOAD) {
      uintptr_t segment_start = PageSizeRoundDown(segment->p_vaddr);
      uintptr_t segment_end = PageSizeRoundUp(segment->p_vaddr
                                              + segment->p_memsz);
      int prot = 0;
      if ((segment->p_flags & PF_R) != 0)
        prot |= PROT_READ;
      if ((segment->p_flags & PF_W) != 0)
        prot |= PROT_WRITE;
      if ((segment->p_flags & PF_X) != 0)
        prot |= PROT_EXEC;
      void *result = mmap(load_offset + segment_start,
                          segment_end - segment_start,
                          prot, MAP_PRIVATE | MAP_FIXED, fd,
                          PageSizeRoundDown(segment->p_offset));
      if (result == MAP_FAILED) {
        Sandbox::die("Failed to map ELF segment");
      }
      // TODO(mseaborn): Support a BSS that goes beyond the file's extent.
      if ((segment->p_flags & PF_W) != 0) {
        // Zero the BSS to the end of the page.  ld.so and other
        // programs use the rest of this page as part of the brk()
        // heap and assume that it has been zeroed.
        uintptr_t bss_start = segment->p_vaddr + segment->p_filesz;
        memset(load_offset + bss_start, 0, segment_end - bss_start);
      }
    }
  }
  if (close(fd) != 0) {
    Sandbox::die("close() failed");
  }
  return entry_point;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s executable args...\n", argv[0]);
    return 1;
  }

  const char *executable_filename = argv[1];
  int executable_fd = open(executable_filename, O_RDONLY);
  if (executable_fd < 0) {
    fprintf(stderr, "Failed to open executable %s: %s\n",
            executable_filename, strerror(errno));
    return 1;
  }

  playground::g_policy.allow_file_namespace = true;
  playground::AddTlsSetupSyscall();
  StartSeccompSandbox();

  ElfW(auxv_t) *auxv = FindAuxv(argc, argv);
  SetAuxvField(auxv, AT_SYSINFO, (uintptr_t) syscallEntryPointNoFrame);
  char **stack = argv;
  *(long *) stack = argc - 1;
  void *entry_point = LoadElfObject(executable_fd, auxv);
  JumpToElfEntryPoint(stack, entry_point, 0);
}
