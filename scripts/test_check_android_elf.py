# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0

import contextlib
import io
import subprocess
import unittest
from unittest import mock

import check_android_elf


# Program headers from an ARM64 NDK r27 shared library with both 16 KB flags.
ALIGNED_HEADERS = '''
  Type           Offset   VirtAddr           PhysAddr           FileSiz  MemSiz   Flg Align
  LOAD           0x000000 0x0000000000000000 0x0000000000000000 0x000700 0x000700 R E 0x4000
  LOAD           0x000700 0x0000000000004700 0x0000000000004700 0x0001e8 0x003900 RW  0x4000
  LOAD           0x0008e8 0x00000000000088e8 0x00000000000088e8 0x000004 0x000004 RW  0x4000
  GNU_RELRO      0x000700 0x0000000000004700 0x0000000000004700 0x0001e8 0x003900 R   0x1
'''


class AndroidElfTests(unittest.TestCase):
    # Accept a RELRO segment whose start is unaligned but whose end is 16 KB aligned.
    def test_aligned_headers(self):
        self.assertEqual([], check_android_elf.check_program_headers(ALIGNED_HEADERS))

    # Catch the max-page-size-only regression even when every PT_LOAD is 16 KB aligned.
    def test_unaligned_relro_end(self):
        output = ALIGNED_HEADERS.replace('0x003900', '0x000900')
        errors = check_android_elf.check_program_headers(output)
        self.assertEqual(1, len(errors))
        self.assertIn('GNU_RELRO end 0x5000', errors[0])

    # Check MemSiz, not FileSiz: RELRO padding is included only in the memory size.
    def test_relro_uses_memory_size(self):
        output = ALIGNED_HEADERS.replace('0x0001e8 0x003900 R ', '0x003900 0x000900 R ')
        self.assertIn('GNU_RELRO end 0x5000', check_android_elf.check_program_headers(output)[0])

    # Reject any under-aligned or invalid PT_LOAD, including non-power-of-two alignments.
    def test_invalid_load_alignment(self):
        for alignment in ('0x1000', '0x0', '0x6000'):
            with self.subTest(alignment=alignment):
                output = ALIGNED_HEADERS.replace('R E 0x4000', 'R E ' + alignment)
                self.assertIn('PT_LOAD alignment', check_android_elf.check_program_headers(output)[0])

    # A large p_align cannot hide incompatible virtual-address and file-offset alignment.
    def test_load_offset_congruence(self):
        output = ALIGNED_HEADERS.replace('LOAD           0x000700', 'LOAD           0x001700')
        self.assertIn('differ modulo 0x4000', check_android_elf.check_program_headers(output)[0])

    # Accept load alignment larger than 16 KB and libraries without an optional RELRO segment.
    def test_larger_alignment_without_relro(self):
        output = 'LOAD 0x0 0x0 0x0 0x100 0x100 R E 0x10000'
        self.assertEqual([], check_android_elf.check_program_headers(output))

    # Fail closed on missing or malformed program headers instead of silently passing validation.
    def test_missing_or_malformed_headers(self):
        for output in ('', 'There are no program headers in this file.', 'LOAD malformed',
                       ALIGNED_HEADERS + 'GNU_RELRO malformed'):
            with self.subTest(output=output):
                self.assertTrue(check_android_elf.check_program_headers(output))

    # The command must return failure and identify the offending artifact among multiple files.
    def test_cli_rejects_unaligned_artifact(self):
        results = [subprocess.CompletedProcess([], 0, ALIGNED_HEADERS, ''),
                   subprocess.CompletedProcess([], 0, ALIGNED_HEADERS.replace('0x003900', '0x000900'), '')]
        stderr = io.StringIO()
        with mock.patch.object(check_android_elf.subprocess, 'run', side_effect=results) as run:
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(stderr):
                result = check_android_elf.main(['--readelf', 'llvm-readelf', 'good.so', 'bad.so'])
        self.assertEqual(1, result)
        self.assertIn('bad.so: GNU_RELRO end 0x5000', stderr.getvalue())
        self.assertEqual(['llvm-readelf', '--program-headers', '--wide', 'good.so'], run.call_args_list[0].args[0])

    # Missing tools and readelf errors must fail the build instead of bypassing the check.
    def test_cli_readelf_failure(self):
        for error in (FileNotFoundError('missing llvm-readelf'),
                      subprocess.CalledProcessError(1, 'llvm-readelf', stderr='invalid ELF')):
            with self.subTest(error=error), mock.patch.object(check_android_elf.subprocess, 'run', side_effect=error):
                with contextlib.redirect_stderr(io.StringIO()):
                    self.assertEqual(1, check_android_elf.main(['--readelf', 'llvm-readelf', 'bad.so']))

    # Valid artifacts must produce a successful exit status for the post-link build step.
    def test_cli_success(self):
        result = subprocess.CompletedProcess([], 0, ALIGNED_HEADERS, '')
        with mock.patch.object(check_android_elf.subprocess, 'run', return_value=result):
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(0, check_android_elf.main(['--readelf', 'llvm-readelf', 'good.so']))


if __name__ == '__main__':
    unittest.main()
