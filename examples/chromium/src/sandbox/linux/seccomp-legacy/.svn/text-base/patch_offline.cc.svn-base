// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "x86_decode.h"

// This tool patches ELF libraries and executables so that they can be
// used with elf_loader.cc.  It rewrites system call instructions so
// that they will work inside the sandbox.


static void CheckBounds(char *data, size_t data_size,
                        void *ptr, size_t inside_size) {
  assert(data <= (char *) ptr);
  assert((char *) ptr + inside_size <= data + data_size);
}

static int FixUpSection(bool is64bit, char *code, size_t code_size) {
  int patch_count = 0;
  char *pos = code;
  char *end = code + code_size;
  while (pos < end) {
    const char *ip = pos;
    playground::next_inst(&ip, is64bit);
    if (is64bit
        ? (pos[0] == '\x0f' && pos[1] == '\x05') /* syscall */
        : (pos[0] == '\xcd' && pos[1] == '\x80') /* int $0x80 */) {
      // Replace the instruction with "int $0".  This is the simplest
      // thing to do since it does not involve moving any instructions
      // around or extending the code segment.  However, executing
      // system calls via "int $0" is not very fast at run time.
      pos[0] = '\xcd';
      pos[1] = '\x00';
      patch_count++;
    }
    pos = (char *) ip;
  }
  return patch_count;
}

struct Elf32 {
  typedef Elf32_Ehdr Ehdr;
  typedef Elf32_Shdr Shdr;
};

struct Elf64 {
  typedef Elf64_Ehdr Ehdr;
  typedef Elf64_Shdr Shdr;
};

template <class Elf>
static void FixUpElf(char *data, size_t data_size) {
  typename Elf::Ehdr *header = (typename Elf::Ehdr *) data;
  CheckBounds(data, data_size, header, sizeof(*header));
  assert(memcmp(header->e_ident, ELFMAG, strlen(ELFMAG)) == 0);

  int patch_count = 0;
  for (int index = 0; index < header->e_shnum; index++) {
    typename Elf::Shdr *section =
      (typename Elf::Shdr *) (data + header->e_shoff +
                              header->e_shentsize * index);
    CheckBounds(data, data_size, section, sizeof(*section));

    if ((section->sh_flags & SHF_EXECINSTR) != 0) {
      CheckBounds(data, data_size,
                  data + section->sh_offset, section->sh_size);
      patch_count += FixUpSection(header->e_machine == EM_X86_64,
                                  data + section->sh_offset, section->sh_size);
    }
  }
  printf("patched %i syscall instructions\n", patch_count);
}

static void FixUpElfFile(const char *input_file, const char *output_file) {
  FILE *fp;
  size_t file_size;
  char *data;
  size_t got;
  size_t written;

  // Read whole ELF file and write it back with modifications.
  fp = fopen(input_file, "rb");
  if (fp == NULL) {
    fprintf(stderr, "Failed to open input file: %s\n", input_file);
    exit(1);
  }
  // Find the file size.
  fseek(fp, 0, SEEK_END);
  file_size = ftell(fp);
  data = (char *) malloc(file_size);
  assert(data != NULL);
  fseek(fp, 0, SEEK_SET);
  got = fread(data, 1, file_size, fp);
  assert(got == file_size);
  fclose(fp);

  switch (data[EI_CLASS]) {
    case ELFCLASS64:
      FixUpElf<Elf64>(data, file_size);
      break;
    case ELFCLASS32:
      FixUpElf<Elf32>(data, file_size);
      break;
    default:
      fprintf(stderr, "Unknown ELF class\n");
      exit(1);
  }

  fp = fopen(output_file, "wb");
  if (fp == NULL) {
    fprintf(stderr, "Failed to open output file: %s\n", output_file);
    exit(1);
  }
  written = fwrite(data, 1, file_size, fp);
  assert(written == file_size);
  fclose(fp);
  free(data);
}

int main(int argc, char **argv) {
  if (argc != 4 || strcmp(argv[2], "-o") != 0) {
    fprintf(stderr, "Usage: %s <input-file> -o <output-file>\n\n", argv[0]);
    fprintf(stderr,
            "This tool rewrites ELF objects to patch system call instructions\n"
            "to be redirected via the seccomp-sandbox's handler.\n");
    return 1;
  }
  FixUpElfFile(argv[1], argv[3]);
  return 0;
}
