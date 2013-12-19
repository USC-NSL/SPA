# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'seccomp_intermediate_dir': '<(INTERMEDIATE_DIR)/seccomp-sandbox',
  },
  'targets': [
    {
      'target_name': 'seccomp_sandbox',
      'type': 'static_library',
      'sources': [
        'access.cc',
        'allocator.cc',
        'allocator.h',
        'clone.cc',
        'exit.cc',
        'debug.cc',
        'fault_handler_asm.S',
        'getpid.cc',
        'gettid.cc',
        'ioctl.cc',
        'ipc.cc',
        'library.cc',
        'library.h',
        'linux_syscall_support.h',
        'madvise.cc',
        'maps.cc',
        'maps.h',
        'mmap.cc',
        'mprotect.cc',
        'munmap.cc',
        'mutex.h',
        'open.cc',
        'prctl.cc',
        'sandbox.cc',
        'sandbox.h',
        'sandbox_impl.h',
        'securemem.cc',
        'securemem.h',
        'sigaction.cc',
        'sigprocmask.cc',
        'socketcall.cc',
        'stat.cc',
        'syscall_entrypoint.cc',
        'syscall_entrypoint.h',
        'system_call_table.cc',
        'system_call_table.h',
        'tls.h',
        'tls_setup.cc',
        'tls_setup_helper.S',
        'trusted_process.cc',
        'trusted_thread.cc',
        'trusted_thread_asm.S',
        'x86_decode.cc',
        'x86_decode.h',
      ],
    },
    {
      'target_name': 'seccomp_tests',
      'type': 'executable',
      'sources': [
        'reference_trusted_thread.cc',
        'tests/clone_test_helper.S',
        'tests/syscall_via_int0.S',
        'tests/test_runner.cc',
        'tests/test_patching.cc',
        'tests/test_patching_input.S',
        'tests/test_syscalls.cc',
      ],
      'include_dirs': [
         '.',
         '<(seccomp_intermediate_dir)',
      ],
      'dependencies': [
        'seccomp_sandbox',
      ],
      'libraries': [
        '-lpthread',
        '-lutil', # For openpty()
      ],
    },
    {
      'target_name': 'timestats',
      'type': 'executable',
      'sources': [
        'timestats.cc',
      ],
    },
  ],
}
