/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/scale.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>  // For getenv()

#include "libyuv/cpu_id.h"
#include "libyuv/planar_functions.h"  // For CopyARGB
#include "libyuv/row.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// ARGB scaling uses bilinear or point, but not box filter.
#if !defined(LIBYUV_DISABLE_NEON) && \
    (defined(__ARM_NEON__) || defined(LIBYUV_NEON))
#define HAS_SCALEARGBROWDOWNEVEN_NEON
#define HAS_SCALEARGBROWDOWN2_NEON
void ScaleARGBRowDownEven_NEON(const uint8* src_argb, int src_stride,
                               int src_stepx,
                               uint8* dst_argb, int dst_width);
void ScaleARGBRowDownEvenInt_NEON(const uint8* src_argb, int src_stride,
                                  int src_stepx,
                                  uint8* dst_argb, int dst_width);
void ScaleARGBRowDown2_NEON(const uint8* src_ptr, ptrdiff_t /* src_stride */,
                            uint8* dst, int dst_width);
void ScaleARGBRowDown2Int_NEON(const uint8* src_ptr, ptrdiff_t src_stride,
                               uint8* dst, int dst_width);
#endif

#if !defined(LIBYUV_DISABLE_X86) && defined(_M_IX86)
#define HAS_SCALEARGBROWDOWN2_SSE2
// Reads 8 pixels, throws half away and writes 4 even pixels (0, 2, 4, 6)
// Alignment requirement: src_argb 16 byte aligned, dst_argb 16 byte aligned.
__declspec(naked) __declspec(align(16))
static void ScaleARGBRowDown2_SSE2(const uint8* src_argb,
                                   ptrdiff_t /* src_stride */,
                                   uint8* dst_argb, int dst_width) {
  __asm {
    mov        eax, [esp + 4]        // src_argb
                                     // src_stride ignored
    mov        edx, [esp + 12]       // dst_argb
    mov        ecx, [esp + 16]       // dst_width

    align      16
  wloop:
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    lea        eax,  [eax + 32]
    shufps     xmm0, xmm1, 0x88
    sub        ecx, 4
    movdqa     [edx], xmm0
    lea        edx, [edx + 16]
    jg         wloop

    ret
  }
}

// Blends 8x2 rectangle to 4x1.
// Alignment requirement: src_argb 16 byte aligned, dst_argb 16 byte aligned.
__declspec(naked) __declspec(align(16))
static void ScaleARGBRowDown2Int_SSE2(const uint8* src_argb,
                                      ptrdiff_t src_stride,
                                      uint8* dst_argb, int dst_width) {
  __asm {
    push       esi
    mov        eax, [esp + 4 + 4]    // src_argb
    mov        esi, [esp + 4 + 8]    // src_stride
    mov        edx, [esp + 4 + 12]   // dst_argb
    mov        ecx, [esp + 4 + 16]   // dst_width

    align      16
  wloop:
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    movdqa     xmm2, [eax + esi]
    movdqa     xmm3, [eax + esi + 16]
    lea        eax,  [eax + 32]
    pavgb      xmm0, xmm2            // average rows
    pavgb      xmm1, xmm3
    movdqa     xmm2, xmm0            // average columns (8 to 4 pixels)
    shufps     xmm0, xmm1, 0x88      // even pixels
    shufps     xmm2, xmm1, 0xdd      // odd pixels
    pavgb      xmm0, xmm2
    sub        ecx, 4
    movdqa     [edx], xmm0
    lea        edx, [edx + 16]
    jg         wloop

    pop        esi
    ret
  }
}

#define HAS_SCALEARGBROWDOWNEVEN_SSE2
// Reads 4 pixels at a time.
// Alignment requirement: dst_argb 16 byte aligned.
__declspec(naked) __declspec(align(16))
void ScaleARGBRowDownEven_SSE2(const uint8* src_argb, ptrdiff_t src_stride,
                               int src_stepx,
                               uint8* dst_argb, int dst_width) {
  __asm {
    push       ebx
    push       edi
    mov        eax, [esp + 8 + 4]    // src_argb
                                     // src_stride ignored
    mov        ebx, [esp + 8 + 12]   // src_stepx
    mov        edx, [esp + 8 + 16]   // dst_argb
    mov        ecx, [esp + 8 + 20]   // dst_width
    lea        ebx, [ebx * 4]
    lea        edi, [ebx + ebx * 2]

    align      16
  wloop:
    movd       xmm0, [eax]
    movd       xmm1, [eax + ebx]
    punpckldq  xmm0, xmm1
    movd       xmm2, [eax + ebx * 2]
    movd       xmm3, [eax + edi]
    lea        eax,  [eax + ebx * 4]
    punpckldq  xmm2, xmm3
    punpcklqdq xmm0, xmm2
    sub        ecx, 4
    movdqa     [edx], xmm0
    lea        edx, [edx + 16]
    jg         wloop

    pop        edi
    pop        ebx
    ret
  }
}

// Blends four 2x2 to 4x1.
// Alignment requirement: dst_argb 16 byte aligned.
__declspec(naked) __declspec(align(16))
static void ScaleARGBRowDownEvenInt_SSE2(const uint8* src_argb,
                                         ptrdiff_t src_stride,
                                         int src_stepx,
                                         uint8* dst_argb, int dst_width) {
  __asm {
    push       ebx
    push       esi
    push       edi
    mov        eax, [esp + 12 + 4]    // src_argb
    mov        esi, [esp + 12 + 8]    // src_stride
    mov        ebx, [esp + 12 + 12]   // src_stepx
    mov        edx, [esp + 12 + 16]   // dst_argb
    mov        ecx, [esp + 12 + 20]   // dst_width
    lea        esi, [eax + esi]      // row1 pointer
    lea        ebx, [ebx * 4]
    lea        edi, [ebx + ebx * 2]

    align      16
  wloop:
    movq       xmm0, qword ptr [eax] // row0 4 pairs
    movhps     xmm0, qword ptr [eax + ebx]
    movq       xmm1, qword ptr [eax + ebx * 2]
    movhps     xmm1, qword ptr [eax + edi]
    lea        eax,  [eax + ebx * 4]
    movq       xmm2, qword ptr [esi] // row1 4 pairs
    movhps     xmm2, qword ptr [esi + ebx]
    movq       xmm3, qword ptr [esi + ebx * 2]
    movhps     xmm3, qword ptr [esi + edi]
    lea        esi,  [esi + ebx * 4]
    pavgb      xmm0, xmm2            // average rows
    pavgb      xmm1, xmm3
    movdqa     xmm2, xmm0            // average columns (8 to 4 pixels)
    shufps     xmm0, xmm1, 0x88      // even pixels
    shufps     xmm2, xmm1, 0xdd      // odd pixels
    pavgb      xmm0, xmm2
    sub        ecx, 4
    movdqa     [edx], xmm0
    lea        edx, [edx + 16]
    jg         wloop

    pop        edi
    pop        esi
    pop        ebx
    ret
  }
}

// Column scaling unfiltered. SSSE3 version.
// TODO(fbarchard): Port to Neon

#define HAS_SCALEARGBCOLS_SSE2
__declspec(naked) __declspec(align(16))
static void ScaleARGBCols_SSE2(uint8* dst_argb, const uint8* src_argb,
                               int dst_width, int x, int dx) {
  __asm {
    push       esi
    push       edi
    mov        edi, [esp + 8 + 4]    // dst_argb
    mov        esi, [esp + 8 + 8]    // src_argb
    mov        ecx, [esp + 8 + 12]   // dst_width
    movd       xmm2, [esp + 8 + 16]  // x
    movd       xmm3, [esp + 8 + 20]  // dx
    pextrw     eax, xmm2, 1         // get x0 integer. preroll
    sub        ecx, 2
    jl         xloop29

    movdqa     xmm0, xmm2           // x1 = x0 + dx
    paddd      xmm0, xmm3
    punpckldq  xmm2, xmm0           // x0 x1
    punpckldq  xmm3, xmm3           // dx dx
    paddd      xmm3, xmm3           // dx * 2, dx * 2
    pextrw     edx, xmm2, 3         // get x1 integer. preroll

    // 2 Pixel loop.
    align      16
  xloop2:
    paddd      xmm2, xmm3           // x += dx
    movd       xmm0, qword ptr [esi + eax * 4]  // 1 source x0 pixels
    movd       xmm1, qword ptr [esi + edx * 4]  // 1 source x1 pixels
    punpckldq  xmm0, xmm1           // x0 x1
    pextrw     eax, xmm2, 1         // get x0 integer. next iteration.
    pextrw     edx, xmm2, 3         // get x1 integer. next iteration.
    movq       qword ptr [edi], xmm0
    lea        edi, [edi + 8]
    sub        ecx, 2               // 2 pixels
    jge        xloop2
 xloop29:

    add        ecx, 2 - 1
    jl         xloop99

    // 1 pixel remainder
    movd       xmm0, qword ptr [esi + eax * 4]  // 1 source x0 pixels
    movd       [edi], xmm0
 xloop99:

    pop        edi
    pop        esi
    ret
  }
}

// Bilinear row filtering combines 2x1 -> 1x1. SSSE3 version.
// TODO(fbarchard): Port to Neon

// Shuffle table for arranging 2 pixels into pairs for pmaddubsw
static const uvec8 kShuffleColARGB = {
  0u, 4u, 1u, 5u, 2u, 6u, 3u, 7u,  // bbggrraa 1st pixel
  8u, 12u, 9u, 13u, 10u, 14u, 11u, 15u  // bbggrraa 2nd pixel
};

// Shuffle table for duplicating 2 fractions into 8 bytes each
static const uvec8 kShuffleFractions = {
  0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u,
};

#define HAS_SCALEARGBFILTERCOLS_SSSE3
__declspec(naked) __declspec(align(16))
static void ScaleARGBFilterCols_SSSE3(uint8* dst_argb, const uint8* src_argb,
                                      int dst_width, int x, int dx) {
  __asm {
    push       esi
    push       edi
    mov        edi, [esp + 8 + 4]    // dst_argb
    mov        esi, [esp + 8 + 8]    // src_argb
    mov        ecx, [esp + 8 + 12]   // dst_width
    movd       xmm2, [esp + 8 + 16]  // x
    movd       xmm3, [esp + 8 + 20]  // dx
    movdqa     xmm4, kShuffleColARGB
    movdqa     xmm5, kShuffleFractions
    pcmpeqb    xmm6, xmm6           // generate 0x007f for inverting fraction.
    psrlw      xmm6, 9
    pextrw     eax, xmm2, 1         // get x0 integer. preroll
    sub        ecx, 2
    jl         xloop29

    movdqa     xmm0, xmm2           // x1 = x0 + dx
    paddd      xmm0, xmm3
    punpckldq  xmm2, xmm0           // x0 x1
    punpckldq  xmm3, xmm3           // dx dx
    paddd      xmm3, xmm3           // dx * 2, dx * 2
    pextrw     edx, xmm2, 3         // get x1 integer. preroll

    // 2 Pixel loop.
    align      16
  xloop2:
    movdqa     xmm1, xmm2           // x0, x1 fractions.
    paddd      xmm2, xmm3           // x += dx
    movq       xmm0, qword ptr [esi + eax * 4]  // 2 source x0 pixels
    psrlw      xmm1, 9              // 7 bit fractions.
    movhps     xmm0, qword ptr [esi + edx * 4]  // 2 source x1 pixels
    pshufb     xmm1, xmm5           // 0000000011111111
    pshufb     xmm0, xmm4           // arrange pixels into pairs
    pxor       xmm1, xmm6           // 0..7f and 7f..0
    pmaddubsw  xmm0, xmm1           // argb_argb 16 bit, 2 pixels.
    psrlw      xmm0, 7              // argb 8.7 fixed point to low 8 bits.
    pextrw     eax, xmm2, 1         // get x0 integer. next iteration.
    pextrw     edx, xmm2, 3         // get x1 integer. next iteration.
    packuswb   xmm0, xmm0           // argb_argb 8 bits, 2 pixels.
    movq       qword ptr [edi], xmm0
    lea        edi, [edi + 8]
    sub        ecx, 2               // 2 pixels
    jge        xloop2
 xloop29:

    add        ecx, 2 - 1
    jl         xloop99

    // 1 pixel remainder
    psrlw      xmm2, 9              // 7 bit fractions.
    movq       xmm0, qword ptr [esi + eax * 4]  // 2 source x0 pixels
    pshufb     xmm2, xmm5           // 00000000
    pshufb     xmm0, xmm4           // arrange pixels into pairs
    pxor       xmm2, xmm6           // 0..7f and 7f..0
    pmaddubsw  xmm0, xmm2           // argb 16 bit, 1 pixel.
    psrlw      xmm0, 7
    packuswb   xmm0, xmm0           // argb 8 bits, 1 pixel.
    movd       [edi], xmm0
 xloop99:

    pop        edi
    pop        esi
    ret
  }
}

#elif !defined(LIBYUV_DISABLE_X86) && (defined(__x86_64__) || defined(__i386__))
// GCC versions of row functions are verbatim conversions from Visual C.
// Generated using gcc disassembly on Visual C object file:
// objdump -D yuvscaler.obj >yuvscaler.txt
#define HAS_SCALEARGBROWDOWN2_SSE2
static void ScaleARGBRowDown2_SSE2(const uint8* src_argb,
                                   ptrdiff_t /* src_stride */,
                                   uint8* dst_argb, int dst_width) {
  asm volatile (
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqa    (%0),%%xmm0                     \n"
    "movdqa    0x10(%0),%%xmm1                 \n"
    "lea       0x20(%0),%0                     \n"
    "shufps    $0x88,%%xmm1,%%xmm0             \n"
    "sub       $0x4,%2                         \n"
    "movdqa    %%xmm0,(%1)                     \n"
    "lea       0x10(%1),%1                     \n"
    "jg        1b                              \n"
  : "+r"(src_argb),   // %0
    "+r"(dst_argb),   // %1
    "+r"(dst_width)  // %2
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1"
#endif
  );
}

static void ScaleARGBRowDown2Int_SSE2(const uint8* src_argb,
                                      ptrdiff_t src_stride,
                                      uint8* dst_argb, int dst_width) {
  asm volatile (
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqa    (%0),%%xmm0                     \n"
    "movdqa    0x10(%0),%%xmm1                 \n"
    "movdqa    (%0,%3,1),%%xmm2                \n"
    "movdqa    0x10(%0,%3,1),%%xmm3            \n"
    "lea       0x20(%0),%0                     \n"
    "pavgb     %%xmm2,%%xmm0                   \n"
    "pavgb     %%xmm3,%%xmm1                   \n"
    "movdqa    %%xmm0,%%xmm2                   \n"
    "shufps    $0x88,%%xmm1,%%xmm0             \n"
    "shufps    $0xdd,%%xmm1,%%xmm2             \n"
    "pavgb     %%xmm2,%%xmm0                   \n"
    "sub       $0x4,%2                         \n"
    "movdqa    %%xmm0,(%1)                     \n"
    "lea       0x10(%1),%1                     \n"
    "jg        1b                              \n"
  : "+r"(src_argb),    // %0
    "+r"(dst_argb),    // %1
    "+r"(dst_width)   // %2
  : "r"(static_cast<intptr_t>(src_stride))   // %3
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3"
#endif
  );
}

#define HAS_SCALEARGBROWDOWNEVEN_SSE2
// Reads 4 pixels at a time.
// Alignment requirement: dst_argb 16 byte aligned.
void ScaleARGBRowDownEven_SSE2(const uint8* src_argb, ptrdiff_t src_stride,
                               int src_stepx,
                               uint8* dst_argb, int dst_width) {
  intptr_t src_stepx_x4 = static_cast<intptr_t>(src_stepx);
  intptr_t src_stepx_x12 = 0;
  asm volatile (
    "lea       0x0(,%1,4),%1                   \n"
    "lea       (%1,%1,2),%4                    \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movd      (%0),%%xmm0                     \n"
    "movd      (%0,%1,1),%%xmm1                \n"
    "punpckldq %%xmm1,%%xmm0                   \n"
    "movd      (%0,%1,2),%%xmm2                \n"
    "movd      (%0,%4,1),%%xmm3                \n"
    "lea       (%0,%1,4),%0                    \n"
    "punpckldq %%xmm3,%%xmm2                   \n"
    "punpcklqdq %%xmm2,%%xmm0                  \n"
    "sub       $0x4,%3                         \n"
    "movdqa    %%xmm0,(%2)                     \n"
    "lea       0x10(%2),%2                     \n"
    "jg        1b                              \n"
  : "+r"(src_argb),       // %0
    "+r"(src_stepx_x4),  // %1
    "+r"(dst_argb),       // %2
    "+r"(dst_width),     // %3
    "+r"(src_stepx_x12)  // %4
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3"
#endif
  );
}

// Blends four 2x2 to 4x1.
// Alignment requirement: dst_argb 16 byte aligned.
static void ScaleARGBRowDownEvenInt_SSE2(const uint8* src_argb,
                                         ptrdiff_t src_stride, int src_stepx,
                                         uint8* dst_argb, int dst_width) {
  intptr_t src_stepx_x4 = static_cast<intptr_t>(src_stepx);
  intptr_t src_stepx_x12 = 0;
  intptr_t row1 = static_cast<intptr_t>(src_stride);
  asm volatile (
    "lea       0x0(,%1,4),%1                   \n"
    "lea       (%1,%1,2),%4                    \n"
    "lea       (%0,%5,1),%5                    \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movq      (%0),%%xmm0                     \n"
    "movhps    (%0,%1,1),%%xmm0                \n"
    "movq      (%0,%1,2),%%xmm1                \n"
    "movhps    (%0,%4,1),%%xmm1                \n"
    "lea       (%0,%1,4),%0                    \n"
    "movq      (%5),%%xmm2                     \n"
    "movhps    (%5,%1,1),%%xmm2                \n"
    "movq      (%5,%1,2),%%xmm3                \n"
    "movhps    (%5,%4,1),%%xmm3                \n"
    "lea       (%5,%1,4),%5                    \n"
    "pavgb     %%xmm2,%%xmm0                   \n"
    "pavgb     %%xmm3,%%xmm1                   \n"
    "movdqa    %%xmm0,%%xmm2                   \n"
    "shufps    $0x88,%%xmm1,%%xmm0             \n"
    "shufps    $0xdd,%%xmm1,%%xmm2             \n"
    "pavgb     %%xmm2,%%xmm0                   \n"
    "sub       $0x4,%3                         \n"
    "movdqa    %%xmm0,(%2)                     \n"
    "lea       0x10(%2),%2                     \n"
    "jg        1b                              \n"
  : "+r"(src_argb),        // %0
    "+r"(src_stepx_x4),   // %1
    "+r"(dst_argb),        // %2
    "+rm"(dst_width),     // %3
    "+r"(src_stepx_x12),  // %4
    "+r"(row1)            // %5
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3"
#endif
  );
}

#define HAS_SCALEARGBCOLS_SSE2
static void ScaleARGBCols_SSE2(uint8* dst_argb, const uint8* src_argb,
                               int dst_width, int x, int dx) {
  intptr_t x0 = 0, x1 = 0;
  asm volatile (
    "movd      %5,%%xmm2                       \n"
    "movd      %6,%%xmm3                       \n"
    "pextrw    $0x1,%%xmm2,%k3                 \n"
    "sub       $0x2,%2                         \n"
    "jl        29f                             \n"
    "movdqa    %%xmm2,%%xmm0                   \n"
    "paddd     %%xmm3,%%xmm0                   \n"
    "punpckldq %%xmm0,%%xmm2                   \n"
    "punpckldq %%xmm3,%%xmm3                   \n"
    "paddd     %%xmm3,%%xmm3                   \n"
    "pextrw    $0x3,%%xmm2,%k4                 \n"

    ".p2align  4                               \n"
  "2:                                          \n"
    "paddd     %%xmm3,%%xmm2                   \n"
    "movd      (%1,%3,4),%%xmm0                \n"
    "movd      (%1,%4,4),%%xmm1                \n"
    "punpckldq %%xmm1,%%xmm0                   \n"
    "pextrw    $0x1,%%xmm2,%k3                 \n"
    "pextrw    $0x3,%%xmm2,%k4                 \n"
    "movq      %%xmm0,(%0)                     \n"
    "lea       0x8(%0),%0                      \n"
    "sub       $0x2,%2                         \n"
    "jge       2b                              \n"

  "29:                                         \n"
    "add       $0x1,%2                         \n"
    "jl        99f                             \n"
    "movd      (%1,%3,4),%%xmm0                \n"
    "movd      %%xmm0,(%0)                     \n"
  "99:                                         \n"
  : "+r"(dst_argb),    // %0
    "+r"(src_argb),    // %1
    "+rm"(dst_width),  // %2
    "+r"(x0),          // %3
    "+r"(x1)           // %4
  : "rm"(x),           // %5
    "rm"(dx)           // %6
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3"
#endif
  );
}

#ifdef __APPLE__
#define CONST
#else
#define CONST static const
#endif

// Shuffle table for arranging 2 pixels into pairs for pmaddubsw
CONST uvec8 kShuffleColARGB = {
  0u, 4u, 1u, 5u, 2u, 6u, 3u, 7u,  // bbggrraa 1st pixel
  8u, 12u, 9u, 13u, 10u, 14u, 11u, 15u  // bbggrraa 2nd pixel
};

// Shuffle table for duplicating 2 fractions into 8 bytes each
CONST uvec8 kShuffleFractions = {
  0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u,
};

// Bilinear row filtering combines 4x2 -> 4x1. SSSE3 version
#define HAS_SCALEARGBFILTERCOLS_SSSE3
static void ScaleARGBFilterCols_SSSE3(uint8* dst_argb, const uint8* src_argb,
                                      int dst_width, int x, int dx) {
  intptr_t x0 = 0, x1 = 0;
  asm volatile (
    "movdqa    %0,%%xmm4                       \n"
    "movdqa    %1,%%xmm5                       \n"
  :
  : "m"(kShuffleColARGB),  // %0
    "m"(kShuffleFractions)  // %1
  );

  asm volatile (
    "movd      %5,%%xmm2                       \n"
    "movd      %6,%%xmm3                       \n"
    "pcmpeqb   %%xmm6,%%xmm6                   \n"
    "psrlw     $0x9,%%xmm6                     \n"
    "pextrw    $0x1,%%xmm2,%k3                 \n"
    "sub       $0x2,%2                         \n"
    "jl        29f                             \n"
    "movdqa    %%xmm2,%%xmm0                   \n"
    "paddd     %%xmm3,%%xmm0                   \n"
    "punpckldq %%xmm0,%%xmm2                   \n"
    "punpckldq %%xmm3,%%xmm3                   \n"
    "paddd     %%xmm3,%%xmm3                   \n"
    "pextrw    $0x3,%%xmm2,%k4                 \n"

    ".p2align  4                               \n"
  "2:                                          \n"
    "movdqa    %%xmm2,%%xmm1                   \n"
    "paddd     %%xmm3,%%xmm2                   \n"
    "movq      (%1,%3,4),%%xmm0                \n"
    "psrlw     $0x9,%%xmm1                     \n"
    "movhps    (%1,%4,4),%%xmm0                \n"
    "pshufb    %%xmm5,%%xmm1                   \n"
    "pshufb    %%xmm4,%%xmm0                   \n"
    "pxor      %%xmm6,%%xmm1                   \n"
    "pmaddubsw %%xmm1,%%xmm0                   \n"
    "psrlw     $0x7,%%xmm0                     \n"
    "pextrw    $0x1,%%xmm2,%k3                 \n"
    "pextrw    $0x3,%%xmm2,%k4                 \n"
    "packuswb  %%xmm0,%%xmm0                   \n"
    "movq      %%xmm0,(%0)                     \n"
    "lea       0x8(%0),%0                      \n"
    "sub       $0x2,%2                         \n"
    "jge       2b                              \n"

  "29:                                         \n"
    "add       $0x1,%2                         \n"
    "jl        99f                             \n"
    "psrlw     $0x9,%%xmm2                     \n"
    "movq      (%1,%3,4),%%xmm0                \n"
    "pshufb    %%xmm5,%%xmm2                   \n"
    "pshufb    %%xmm4,%%xmm0                   \n"
    "pxor      %%xmm6,%%xmm2                   \n"
    "pmaddubsw %%xmm2,%%xmm0                   \n"
    "psrlw     $0x7,%%xmm0                     \n"
    "packuswb  %%xmm0,%%xmm0                   \n"
    "movd      %%xmm0,(%0)                     \n"
  "99:                                         \n"
  : "+r"(dst_argb),    // %0
    "+r"(src_argb),    // %1
    "+rm"(dst_width),  // %2
    "+r"(x0),          // %3
    "+r"(x1)           // %4
  : "rm"(x),           // %5
    "rm"(dx)           // %6
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6"
#endif
  );
}
#endif  // defined(__x86_64__) || defined(__i386__)

static void ScaleARGBRowDown2_C(const uint8* src_argb,
                                ptrdiff_t /* src_stride */,
                                uint8* dst_argb, int dst_width) {
  const uint32* src = reinterpret_cast<const uint32*>(src_argb);
  uint32* dst = reinterpret_cast<uint32*>(dst_argb);

  for (int x = 0; x < dst_width - 1; x += 2) {
    dst[0] = src[0];
    dst[1] = src[2];
    src += 4;
    dst += 2;
  }
  if (dst_width & 1) {
    dst[0] = src[0];
  }
}

static void ScaleARGBRowDown2Int_C(const uint8* src_argb, ptrdiff_t src_stride,
                                   uint8* dst_argb, int dst_width) {
  for (int x = 0; x < dst_width; ++x) {
    dst_argb[0] = (src_argb[0] + src_argb[4] +
                  src_argb[src_stride] + src_argb[src_stride + 4] + 2) >> 2;
    dst_argb[1] = (src_argb[1] + src_argb[5] +
                  src_argb[src_stride + 1] + src_argb[src_stride + 5] + 2) >> 2;
    dst_argb[2] = (src_argb[2] + src_argb[6] +
                  src_argb[src_stride + 2] + src_argb[src_stride + 6] + 2) >> 2;
    dst_argb[3] = (src_argb[3] + src_argb[7] +
                  src_argb[src_stride + 3] + src_argb[src_stride + 7] + 2) >> 2;
    src_argb += 8;
    dst_argb += 4;
  }
}

void ScaleARGBRowDownEven_C(const uint8* src_argb, ptrdiff_t /* src_stride */,
                            int src_stepx,
                            uint8* dst_argb, int dst_width) {
  const uint32* src = reinterpret_cast<const uint32*>(src_argb);
  uint32* dst = reinterpret_cast<uint32*>(dst_argb);

  for (int x = 0; x < dst_width - 1; x += 2) {
    dst[0] = src[0];
    dst[1] = src[src_stepx];
    src += src_stepx * 2;
    dst += 2;
  }
  if (dst_width & 1) {
    dst[0] = src[0];
  }
}

static void ScaleARGBRowDownEvenInt_C(const uint8* src_argb,
                                      ptrdiff_t src_stride,
                                      int src_stepx,
                                      uint8* dst_argb, int dst_width) {
  for (int x = 0; x < dst_width; ++x) {
    dst_argb[0] = (src_argb[0] + src_argb[4] +
                  src_argb[src_stride] + src_argb[src_stride + 4] + 2) >> 2;
    dst_argb[1] = (src_argb[1] + src_argb[5] +
                  src_argb[src_stride + 1] + src_argb[src_stride + 5] + 2) >> 2;
    dst_argb[2] = (src_argb[2] + src_argb[6] +
                  src_argb[src_stride + 2] + src_argb[src_stride + 6] + 2) >> 2;
    dst_argb[3] = (src_argb[3] + src_argb[7] +
                  src_argb[src_stride + 3] + src_argb[src_stride + 7] + 2) >> 2;
    src_argb += src_stepx * 4;
    dst_argb += 4;
  }
}

// Mimics SSSE3 blender
#define BLENDER1(a, b, f) ((a) * (0x7f ^ f) + (b) * f) >> 7
#define BLENDERC(a, b, f, s) static_cast<uint32>( \
    BLENDER1(((a) >> s) & 255, ((b) >> s) & 255, f) << s)
#define BLENDER(a, b, f) \
    BLENDERC(a, b, f, 24) | BLENDERC(a, b, f, 16) | \
    BLENDERC(a, b, f, 8) | BLENDERC(a, b, f, 0)

static void ScaleARGBFilterCols_C(uint8* dst_argb, const uint8* src_argb,
                                  int dst_width, int x, int dx) {
  const uint32* src = reinterpret_cast<const uint32*>(src_argb);
  uint32* dst = reinterpret_cast<uint32*>(dst_argb);
  for (int j = 0; j < dst_width - 1; j += 2) {
    int xi = x >> 16;
    int xf = (x >> 9) & 0x7f;
    uint32 a = src[xi];
    uint32 b = src[xi + 1];
    dst[0] = BLENDER(a, b, xf);
    x += dx;
    xi = x >> 16;
    xf = (x >> 9) & 0x7f;
    a = src[xi];
    b = src[xi + 1];
    dst[1] = BLENDER(a, b, xf);
    x += dx;
    dst += 2;
  }
  if (dst_width & 1) {
    int xi = x >> 16;
    int xf = (x >> 9) & 0x7f;
    uint32 a = src[xi];
    uint32 b = src[xi + 1];
    dst[0] = BLENDER(a, b, xf);
  }
}

// ScaleARGB ARGB, 1/2
// This is an optimized version for scaling down a ARGB to 1/2 of
// its original size.

static void ScaleARGBDown2(int /* src_width */, int /* src_height */,
                           int dst_width, int dst_height,
                           int src_stride, int dst_stride,
                           const uint8* src_argb, uint8* dst_argb,
                           FilterMode filtering) {
  void (*ScaleARGBRowDown2)(const uint8* src_argb, ptrdiff_t src_stride,
                            uint8* dst_argb, int dst_width) =
      filtering ? ScaleARGBRowDown2Int_C : ScaleARGBRowDown2_C;
#if defined(HAS_SCALEARGBROWDOWN2_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) && IS_ALIGNED(dst_width, 4) &&
      IS_ALIGNED(src_argb, 16) && IS_ALIGNED(src_stride, 16) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride, 16)) {
    ScaleARGBRowDown2 = filtering ? ScaleARGBRowDown2Int_SSE2 :
        ScaleARGBRowDown2_SSE2;
  }
#elif defined(HAS_SCALEARGBROWDOWN2_NEON)
  if (TestCpuFlag(kCpuHasNEON) && IS_ALIGNED(dst_width, 8) &&
      IS_ALIGNED(src_argb, 4) && IS_ALIGNED(src_stride, 4)) {
    ScaleARGBRowDown2 = filtering ? ScaleARGBRowDown2Int_NEON :
        ScaleARGBRowDown2_NEON;
  }
#endif

  // TODO(fbarchard): Loop through source height to allow odd height.
  for (int y = 0; y < dst_height; ++y) {
    ScaleARGBRowDown2(src_argb, src_stride, dst_argb, dst_width);
    src_argb += (src_stride << 1);
    dst_argb += dst_stride;
  }
}

// ScaleARGB ARGB Even
// This is an optimized version for scaling down a ARGB to even
// multiple of its original size.

static void ScaleARGBDownEven(int src_width, int src_height,
                              int dst_width, int dst_height,
                              int src_stride, int dst_stride,
                              const uint8* src_argb, uint8* dst_argb,
                              FilterMode filtering) {
  assert(IS_ALIGNED(src_width, 2));
  assert(IS_ALIGNED(src_height, 2));
  void (*ScaleARGBRowDownEven)(const uint8* src_argb, ptrdiff_t src_stride,
                               int src_step, uint8* dst_argb, int dst_width) =
      filtering ? ScaleARGBRowDownEvenInt_C : ScaleARGBRowDownEven_C;
#if defined(HAS_SCALEARGBROWDOWNEVEN_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) && IS_ALIGNED(dst_width, 4) &&
      IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride, 16)) {
    ScaleARGBRowDownEven = filtering ? ScaleARGBRowDownEvenInt_SSE2 :
        ScaleARGBRowDownEven_SSE2;
  }
#elif defined(HAS_SCALEARGBROWDOWNEVEN_NEON)
  if (TestCpuFlag(kCpuHasNEON) && IS_ALIGNED(dst_width, 4) &&
      IS_ALIGNED(src_argb, 4)) {
    ScaleARGBRowDownEven = filtering ? ScaleARGBRowDownEvenInt_NEON :
        ScaleARGBRowDownEven_NEON;
  }
#endif
  int src_step = src_width / dst_width;
  // Adjust to point to center of box.
  int row_step = src_height / dst_height;
  int row_stride = row_step * src_stride;
  src_argb += ((row_step >> 1) - 1) * src_stride + ((src_step >> 1) - 1) * 4;
  for (int y = 0; y < dst_height; ++y) {
    ScaleARGBRowDownEven(src_argb, src_stride, src_step, dst_argb, dst_width);
    src_argb += row_stride;
    dst_argb += dst_stride;
  }
}

// ScaleARGB ARGB to/from any dimensions, with bilinear
// interpolation.
static void ScaleARGBBilinearDown(int src_width, int src_height,
                                  int dst_width, int dst_height,
                                  int src_stride, int dst_stride,
                                  const uint8* src_argb, uint8* dst_argb) {
  assert(dst_width > 0);
  assert(dst_height > 0);
  assert(src_width * 4 <= kMaxStride);
  SIMD_ALIGNED(uint8 row[kMaxStride + 16]);
  void (*ScaleARGBFilterRows)(uint8* dst_argb, const uint8* src_argb,
      ptrdiff_t src_stride, int dst_width, int source_y_fraction) =
      ARGBInterpolateRow_C;
#if defined(HAS_ARGBINTERPOLATEROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) && src_width >= 4) {
    ScaleARGBFilterRows = ARGBInterpolateRow_Any_SSE2;
    if (IS_ALIGNED(src_width, 4)) {
      ScaleARGBFilterRows = ARGBInterpolateRow_Unaligned_SSE2;
      if (IS_ALIGNED(src_argb, 16) && IS_ALIGNED(src_stride, 16)) {
        ScaleARGBFilterRows = ARGBInterpolateRow_SSE2;
      }
    }
  }
#endif
#if defined(HAS_ARGBINTERPOLATEROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) && src_width >= 4) {
    ScaleARGBFilterRows = ARGBInterpolateRow_Any_SSSE3;
    if (IS_ALIGNED(src_width, 4)) {
      ScaleARGBFilterRows = ARGBInterpolateRow_Unaligned_SSSE3;
      if (IS_ALIGNED(src_argb, 16) && IS_ALIGNED(src_stride, 16)) {
        ScaleARGBFilterRows = ARGBInterpolateRow_SSSE3;
      }
    }
  }
#endif
#if defined(HAS_ARGBINTERPOLATEROW_NEON)
  if (TestCpuFlag(kCpuHasNEON) && src_width >= 4) {
    ScaleARGBFilterRows = ARGBInterpolateRow_Any_NEON;
    if (IS_ALIGNED(src_width, 4)) {
      ScaleARGBFilterRows = ARGBInterpolateRow_NEON;
    }
  }
#endif
  void (*ScaleARGBFilterCols)(uint8* dst_argb, const uint8* src_argb,
      int dst_width, int x, int dx) = ScaleARGBFilterCols_C;
#if defined(HAS_SCALEARGBFILTERCOLS_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    ScaleARGBFilterCols = ScaleARGBFilterCols_SSSE3;
  }
#endif
  int dx = 0;
  int dy = 0;
  int x = 0;
  int y = 0;
  if (dst_width <= src_width) {
    dx = (src_width << 16) / dst_width;
    x = (dx >> 1) - 32768;
  } else if (dst_width > 1) {
    dx = ((src_width - 1) << 16) / (dst_width - 1);
  }
  if (dst_height <= src_height) {
    dy = (src_height << 16) / dst_height;
    y = (dy >> 1) - 32768;
  } else if (dst_height > 1) {
    dy = ((src_height - 1) << 16) / (dst_height - 1);
  }
  int maxy = (src_height > 1) ? ((src_height - 1) << 16) - 1 : 0;
  for (int j = 0; j < dst_height; ++j) {
    if (y > maxy) {
      y = maxy;
    }
    int yi = y >> 16;
    int yf = (y >> 8) & 255;
    const uint8* src = src_argb + yi * src_stride;
    ScaleARGBFilterRows(row, src, src_stride, src_width, yf);
    ScaleARGBFilterCols(dst_argb, row, dst_width, x, dx);
    dst_argb += dst_stride;
    y += dy;
  }
}

// ScaleARGB ARGB to/from any dimensions, with bilinear
// interpolation.
static void ScaleARGBBilinearUp(int src_width, int src_height,
                                int dst_width, int dst_height,
                                int src_stride, int dst_stride,
                                const uint8* src_argb, uint8* dst_argb) {
  assert(dst_width > 0);
  assert(dst_height > 0);
  assert(dst_width * 4 <= kMaxStride);
  void (*ScaleARGBFilterRows)(uint8* dst_argb, const uint8* src_argb,
      ptrdiff_t src_stride, int dst_width, int source_y_fraction) =
      ARGBInterpolateRow_C;
#if defined(HAS_ARGBINTERPOLATEROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) && dst_width >= 4) {
    ScaleARGBFilterRows = ARGBInterpolateRow_Any_SSE2;
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleARGBFilterRows = ARGBInterpolateRow_Unaligned_SSE2;
      if (IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride, 16)) {
        ScaleARGBFilterRows = ARGBInterpolateRow_SSE2;
      }
    }
  }
#endif
#if defined(HAS_ARGBINTERPOLATEROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) && dst_width >= 4) {
    ScaleARGBFilterRows = ARGBInterpolateRow_Any_SSSE3;
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleARGBFilterRows = ARGBInterpolateRow_Unaligned_SSSE3;
      if (IS_ALIGNED(dst_argb, 16) && IS_ALIGNED(dst_stride, 16)) {
        ScaleARGBFilterRows = ARGBInterpolateRow_SSSE3;
      }
    }
  }
#endif
#if defined(HAS_ARGBINTERPOLATEROW_NEON)
  if (TestCpuFlag(kCpuHasNEON) && dst_width >= 4) {
    ScaleARGBFilterRows = ARGBInterpolateRow_Any_NEON;
    if (IS_ALIGNED(dst_width, 4)) {
      ScaleARGBFilterRows = ARGBInterpolateRow_NEON;
    }
  }
#endif
  void (*ScaleARGBFilterCols)(uint8* dst_argb, const uint8* src_argb,
      int dst_width, int x, int dx) = ScaleARGBFilterCols_C;
#if defined(HAS_SCALEARGBFILTERCOLS_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3)) {
    ScaleARGBFilterCols = ScaleARGBFilterCols_SSSE3;
  }
#endif
  int dx = 0;
  int dy = 0;
  int x = 0;
  int y = 0;
  if (dst_width <= src_width) {
    dx = (src_width << 16) / dst_width;
    x = (dx >> 1) - 32768;
  } else if (dst_width > 1) {
    dx = ((src_width - 1) << 16) / (dst_width - 1);
  }
  if (dst_height <= src_height) {
    dy = (src_height << 16) / dst_height;
    y = (dy >> 1) - 32768;
  } else if (dst_height > 1) {
    dy = ((src_height - 1) << 16) / (dst_height - 1);
  }
  int maxy = (src_height > 1) ? ((src_height - 1) << 16) - 1 : 0;
  if (y > maxy) {
    y = maxy;
  }
  int yi = y >> 16;
  int yf = (y >> 8) & 255;
  const uint8* src = src_argb + yi * src_stride;
  SIMD_ALIGNED(uint8 row[2 * kMaxStride]);
  uint8* rowptr = row;
  int rowstride = kMaxStride;
  int lasty = 0;

  ScaleARGBFilterCols(rowptr, src, dst_width, x, dx);
  if (src_height > 1) {
    src += src_stride;
  }
  ScaleARGBFilterCols(rowptr + rowstride, src, dst_width, x, dx);
  src += src_stride;

  for (int j = 0; j < dst_height; ++j) {
    yi = y >> 16;
    yf = (y >> 8) & 255;
    if (yi != lasty) {
      if (y <= maxy) {
        y = maxy;
        yi = y >> 16;
        yf = (y >> 8) & 255;
      } else {
        ScaleARGBFilterCols(rowptr, src, dst_width, x, dx);
        rowptr += rowstride;
        rowstride = -rowstride;
        lasty = yi;
        src += src_stride;
      }
    }
    ScaleARGBFilterRows(dst_argb, rowptr, rowstride, dst_width, yf);
    dst_argb += dst_stride;
    y += dy;
  }
}

// Scales a single row of pixels using point sampling.
// Code is adapted from libyuv bilinear yuv scaling, but with bilinear
//     interpolation off, and argb pixels instead of yuv.
static void ScaleARGBCols_C(uint8* dst_argb, const uint8* src_argb,
                            int dst_width, int x, int dx) {
  const uint32* src = reinterpret_cast<const uint32*>(src_argb);
  uint32* dst = reinterpret_cast<uint32*>(dst_argb);
  for (int j = 0; j < dst_width - 1; j += 2) {
    dst[0] = src[x >> 16];
    x += dx;
    dst[1] = src[x >> 16];
    x += dx;
    dst += 2;
  }
  if (dst_width & 1) {
    dst[0] = src[x >> 16];
  }
}

// ScaleARGB ARGB to/from any dimensions, without interpolation.
// Fixed point math is used for performance: The upper 16 bits
// of x and dx is the integer part of the source position and
// the lower 16 bits are the fixed decimal part.

static void ScaleARGBSimple(int src_width, int src_height,
                            int dst_width, int dst_height,
                            int src_stride, int dst_stride,
                            const uint8* src_argb, uint8* dst_argb) {
  void (*ScaleARGBCols)(uint8* dst_argb, const uint8* src_argb,
      int dst_width, int x, int dx) = ScaleARGBCols_C;
#if defined(HAS_SCALEARGBCOLS_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    ScaleARGBCols = ScaleARGBCols_SSE2;
  }
#endif
  int dx = (src_width << 16) / dst_width;
  int dy = (src_height << 16) / dst_height;
  int x = (dx >= 65536) ? ((dx >> 1) - 32768) : (dx >> 1);
  int y = (dy >= 65536) ? ((dy >> 1) - 32768) : (dy >> 1);
  for (int i = 0; i < dst_height; ++i) {
    ScaleARGBCols(dst_argb, src_argb + (y >> 16) * src_stride,
                  dst_width, x, dx);
    dst_argb += dst_stride;
    y += dy;
  }
}

// ScaleARGB ARGB to/from any dimensions.

static void ScaleARGBAnySize(int src_width, int src_height,
                             int dst_width, int dst_height,
                             int src_stride, int dst_stride,
                             const uint8* src_argb, uint8* dst_argb,
                             FilterMode filtering) {
  if (!filtering ||
      (src_width * 4 > kMaxStride && dst_width * 4 > kMaxStride)) {
    ScaleARGBSimple(src_width, src_height, dst_width, dst_height,
                    src_stride, dst_stride, src_argb, dst_argb);
    return;
  }
  if (dst_height <= src_height || dst_width * 4 > kMaxStride) {
    ScaleARGBBilinearDown(src_width, src_height, dst_width, dst_height,
                          src_stride, dst_stride, src_argb, dst_argb);
  } else {
    ScaleARGBBilinearUp(src_width, src_height, dst_width, dst_height,
                        src_stride, dst_stride, src_argb, dst_argb);
  }
}

// ScaleARGB a ARGB.
//
// This function in turn calls a scaling function
// suitable for handling the desired resolutions.

static void ScaleARGB(const uint8* src, int src_stride,
                      int src_width, int src_height,
                      uint8* dst, int dst_stride,
                      int dst_width, int dst_height,
                      FilterMode filtering) {
#ifdef CPU_X86
  // environment variable overrides for testing.
  char *filter_override = getenv("LIBYUV_FILTER");
  if (filter_override) {
    filtering = (FilterMode)atoi(filter_override);  // NOLINT
  }
#endif
  if (dst_width == src_width && dst_height == src_height) {
    // Straight copy.
    ARGBCopy(src, src_stride, dst, dst_stride, dst_width, dst_height);
    return;
  }
  if (2 * dst_width == src_width && 2 * dst_height == src_height) {
    // Optimized 1/2.
    ScaleARGBDown2(src_width, src_height, dst_width, dst_height,
                   src_stride, dst_stride, src, dst, filtering);
    return;
  }
  int scale_down_x = src_width / dst_width;
  int scale_down_y = src_height / dst_height;
  if (dst_width * scale_down_x == src_width &&
      dst_height * scale_down_y == src_height) {
    if (!(scale_down_x & 1) && !(scale_down_y & 1)) {
      // Optimized even scale down. ie 4, 6, 8, 10x
      ScaleARGBDownEven(src_width, src_height, dst_width, dst_height,
                        src_stride, dst_stride, src, dst, filtering);
      return;
    }
    if ((scale_down_x & 1) && (scale_down_y & 1)) {
      filtering = kFilterNone;
    }
  }
  // Arbitrary scale up and/or down.
  ScaleARGBAnySize(src_width, src_height, dst_width, dst_height,
                   src_stride, dst_stride, src, dst, filtering);
}

// ScaleARGB an ARGB image.
LIBYUV_API
int ARGBScale(const uint8* src_argb, int src_stride_argb,
             int src_width, int src_height,
             uint8* dst_argb, int dst_stride_argb,
             int dst_width, int dst_height,
             FilterMode filtering) {
  if (!src_argb || src_width <= 0 || src_height == 0 ||
      !dst_argb || dst_width <= 0 || dst_height <= 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (src_height < 0) {
    src_height = -src_height;
    src_argb = src_argb + (src_height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }
  ScaleARGB(src_argb, src_stride_argb, src_width, src_height,
            dst_argb, dst_stride_argb, dst_width, dst_height,
            filtering);
  return 0;
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
