/**
 * @file
 *
 * @ingroup RTEMSBSPsAArch64Phytium
 *
 * @brief GRUB boot entry with 16 NOP instructions at the start
 */

/*
 * Copyright (C) 2025 Phytium Technology Co., Ltd.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <rtems/asm.h>
#include <bspopts.h>

  .globl _bsp_start
  .section ".bsp_pre_start_text", "ax"

/* ARM64 Image Header - 64 bytes fixed format for tyche compatibility */
_bsp_start:
  b _bsp_start_actual              /* 0x00: Skip Tyche hooks, jump to standard boot */
  .long 0x00000000                 /* 0x04: code0 (reserved) */
  .quad 0x0000000000100000         /* 0x08: text_offset (1MB from RAM base) */
  .quad 0x00000000000a0000         /* 0x10: image_size (640KB estimated) */
  .quad 0x000000000000000a         /* 0x18: flags (bit1=4K, bit3=BE support) */
  .quad 0x0000000000000000         /* 0x20: reserved */
  .quad 0x0000000000000000         /* 0x28: reserved */
  .quad 0x0000000000000000         /* 0x30: reserved */
  .long 0x644d5241                 /* 0x38: magic "ARMd" (little-endian bytes) */
  .long 0x00000000                 /* 0x3c: reserved */
  /* Total: 64 bytes, execution continues at _code_start */

_code_start:
  /* 
   * FORCE JUMP TO GRUB BOOT PATH
   * In case the bootloader jumps to +0x40 (skipping header) instead of 0x00
   */
  b _bsp_start_actual

  /* 
   * CRITICAL: tyche has called grub_efi_finish_boot_services before jumping here
   * EFI Boot Services are now terminated. Hardware state is unpredictable.
   * We must reinitialize the hardware environment before any other operations.64 ~ 290
   */
  
  /* ==== START HOOK: Print "START_HOOK\r\n" with UART init ==== */
  mov x10, #0x0000
  movk x10, #0x2800, lsl #16  /* UART base = 0x28000000 (ttyAMA1 legacy console) */
  
  /* Initialize PL011 UART */
  /* 1. Disable UART (clear UARTEN in CR) */
  mov w11, #0
  str w11, [x10, #0x30]       /* UARTCR = 0 */
  
  /* 2. Wait for current transmission to complete (check BUSY bit) */
1:
  ldr w11, [x10, #0x18]       /* Read UARTFR (Flag Register) */
  tbnz w11, #3, 1b            /* Loop if BUSY bit is set */
  
  /* 3. Flush FIFO */
  ldr w11, [x10, #0x2C]       /* Read UARTLCR_H */
  bic w11, w11, #0x10         /* Clear FEN (FIFO enable) */
  str w11, [x10, #0x2C]       /* Write back */
  
  /* 4. Set baud rate to 115200 (trying 48MHz UARTCLK) */
  /* IBRD = 48000000 / (16 * 115200) = 26.04 */
  mov w11, #26
  str w11, [x10, #0x24]       /* UARTIBRD = 26 */
  mov w11, #3                 /* FBRD = 0.04 * 64 = 3 */
  str w11, [x10, #0x28]       /* UARTFBRD = 3 */

  /* 5. Set line control: 8N1, enable FIFO */
  mov w11, #0x70              /* 8 bits, no parity, 1 stop, FIFO enabled */
  str w11, [x10, #0x2C]       /* UARTLCR_H */
  
  /* 6. Enable UART, TX, and RX */
  mov w11, #0x301             /* UARTEN | TXE | RXE */
  str w11, [x10, #0x30]       /* UARTCR */
  
  /* Ensure all writes complete */
  dsb sy
  
  /* Now send "START_HOOK\r\n" */
  adr x12, string_data        /* Load address of string */
  b print_loop_start          /* Jump to loop start */
  
  .align 3                    /* Align string to 8-byte boundary */
string_data:
  .byte 'S', 'T', 'A', 'R', 'T', '_', 'H', 'O', 'O', 'K', 13, 10, 0
  .align 2

print_loop_start:
print_loop:
  ldrb w11, [x12], #1         /* Load byte and increment */
  cbz w11, print_done         /* If null terminator, done */
  
  /* Wait for TX FIFO not full */
wait_tx:
  ldr w13, [x10, #0x18]       /* Read UARTFR */
  tbnz w13, #5, wait_tx       /* Loop if TXFF (bit 5) is set */
  
  str w11, [x10]              /* Write to UARTDR */
  b print_loop                /* Next character */

print_done:
  /* ==== END START HOOK ==== */
  
  /* 1. Disable all interrupts */
  msr daifset, #0xf
  
  /* 2. Check current Exception Level */
  mrs x0, CurrentEL
  lsr x0, x0, #2           /* Extract EL[3:2] bits */
  cmp x0, #1
  b.eq at_el1
  
  /* If at EL2, drop to EL1 */
  mov x0, #0x3c9           /* EL1h (use SP_EL1), D,A,I,F masked */
  msr spsr_el2, x0
  adr x0, at_el1
  msr elr_el2, x0
  eret
  
at_el1:
  /* Install a minimal exception vector table for early diagnostics */
  adr x0, _early_vector_table
  msr vbar_el1, x0
  isb

  /* 3. Disable MMU and caches if enabled (critical for direct UART access) */
  mrs x0, sctlr_el1
  bic x0, x0, #(1 << 0)    /* Disable MMU (M bit) */
  bic x0, x0, #(1 << 2)    /* Disable D-cache (C bit) */
  bic x0, x0, #(1 << 12)   /* Disable I-cache (I bit) */
  msr sctlr_el1, x0
  isb
  
  /* 4. Invalidate instruction cache */
  ic iallu
  dsb sy
  isb
  
  /* 5. Now UART physical address should be accessible */
  /* CRITICAL: Output marker IMMEDIATELY to verify code execution */
  mov x10, #0x2800
  movk x10, #0x0000, lsl #16
  mov x11, #0x40           /* '@' marker (signals post-EFI init success) */
  str w11, [x10]
  str w11, [x10]
  str w11, [x10]
  mov x11, #0x0A           /* '\n' */
  str w11, [x10]
  mov x11, #0x0D           /* '\r' */
  str w11, [x10]

/* Minimal early exception vectors and handler */
  .align 11
_early_vector_table:
  b _early_sync_current_sp0
  b _early_irq_current_sp0
  b _early_fiq_current_sp0
  b _early_serr_current_sp0

  b _early_sync_current_spx
  b _early_irq_current_spx
  b _early_fiq_current_spx
  b _early_serr_current_spx

  b _early_sync_lower_a64
  b _early_irq_lower_a64
  b _early_fiq_lower_a64
  b _early_serr_lower_a64

  b _early_sync_lower_a32
  b _early_irq_lower_a32
  b _early_fiq_lower_a32
  b _early_serr_lower_a32

_early_sync_current_sp0:
  b _early_sync_common
_early_irq_current_sp0:
  b _early_hang
_early_fiq_current_sp0:
  b _early_hang
_early_serr_current_sp0:
  b _early_hang

_early_sync_current_spx:
  b _early_sync_common
_early_irq_current_spx:
  b _early_hang
_early_fiq_current_spx:
  b _early_hang
_early_serr_current_spx:
  b _early_hang

_early_sync_lower_a64:
  b _early_sync_common
_early_irq_lower_a64:
  b _early_hang
_early_fiq_lower_a64:
  b _early_hang
_early_serr_lower_a64:
  b _early_hang

_early_sync_lower_a32:
  b _early_sync_common
_early_irq_lower_a32:
  b _early_hang
_early_fiq_lower_a32:
  b _early_hang
_early_serr_lower_a32:
  b _early_hang

_early_sync_common:
  mov x0, #0x2800
  movk x0, #0x0000, lsl #16

  mov x1, #0x45  /* 'E' */
  str w1, [x0]
  mov x1, #0x53  /* 'S' */
  str w1, [x0]
  mov x1, #0x52  /* 'R' */
  str w1, [x0]
  mov x1, #0x3d  /* '=' */
  str w1, [x0]

  mrs x2, esr_el1
  bl _early_uart_hex64

  mov x1, #0x20  /* ' ' */
  str w1, [x0]
  mov x1, #0x46  /* 'F' */
  str w1, [x0]
  mov x1, #0x41  /* 'A' */
  str w1, [x0]
  mov x1, #0x52  /* 'R' */
  str w1, [x0]
  mov x1, #0x3d  /* '=' */
  str w1, [x0]

  mrs x2, far_el1
  bl _early_uart_hex64

  mov x1, #0x0a  /* '\n' */
  str w1, [x0]
  mov x1, #0x0d  /* '\r' */
  str w1, [x0]

_early_hang:
  b _early_hang

_early_uart_hex64:
  mov x3, #60
_early_hex_loop:
  lsr x4, x2, x3
  and x4, x4, #0xf
  add x4, x4, #0x30
  cmp x4, #0x39
  b.ls _early_hex_out
  add x4, x4, #0x7
_early_hex_out:
  str w4, [x0]
  subs x3, x3, #4
  b.ge _early_hex_loop
  ret

_bsp_start_actual:

  /* FIX: Early gate for secondary cores to prevent log spam */
  mrs x9, MPIDR_EL1
  and x9, x9, #0xff
  cbz x9, .Lprimary_core_check
.Lsecondary_core_gate_bsp:
  wfe
  b .Lsecondary_core_gate_bsp
.Lprimary_core_check:
  
  /* Early debug output - use existing UART from GRUB (skip reinitialization) */
  ldr x0, =0x28000000      /* UART0 base address */
  
  /* UART reinitialization DISABLED for GRUB boot - use existing config */
  /* mov x1, #0x00            // Disable UART */
  /* str w1, [x0, #0x30]      // UARTCR */
  
  /* mov x1, #0x0D            // IBRD = 13 for 115200 @ 24MHz */
  /* str w1, [x0, #0x24]      // UARTIBRD */
  
  /* mov x1, #0x01            // FBRD = 1 */
  /* str w1, [x0, #0x28]      // UARTFBRD */
  
  /* mov x1, #0x70            // 8N1, FIFO enabled */
  /* str w1, [x0, #0x2C]      // UARTLCR_H */
  
  /* mov x1, #0x301           // Enable UART, TX, RX */
  /* str w1, [x0, #0x30]      // UARTCR */
  
  /* Output debug string "RTEMS-D2K\n\r" */
  mov x1, #0x47            /* 'G' */
  str w1, [x0]
  mov x1, #0x52            /* 'R' */
  str w1, [x0]
  mov x1, #0x55            /* 'U' */
  str w1, [x0]
  mov x1, #0x42            /* 'B' */
  str w1, [x0]
  mov x1, #0x2D            /* '-' */
  str w1, [x0]
  mov x1, #0x42            /* 'B' */
  str w1, [x0]
  mov x1, #0x4F            /* 'O' */
  str w1, [x0]
  mov x1, #0x4F            /* 'O' */
  str w1, [x0]
  mov x1, #0x54            /* 'T' */
  str w1, [x0]
  mov x1, #0x0A            /* '\n' */
  str w1, [x0]
  mov x1, #0x0D            /* '\r' */
  str w1, [x0]

  /* Cache flush disabled for GRUB boot - GRUB handles cache setup */
  /* #ifdef AARCH64_FLUSH_CACHE_BOOT_UP */
  /*
   * We do dcache flush only for the primary core startup
  */
  /* #ifdef RTEMS_SMP */
  /* bl _AArch64_Get_current_processor_for_system_start */
  /* cmp x0, #0 */
  /* bne _skip_flush_start */
  /* #endif */ /* RTEMS_SMP */
  /* bl rtems_cache_flush_entire_data */
  /* _skip_flush_start: */
  /* #endif */ /* AARCH64_FLUSH_CACHE_BOOT_UP */

  /* Debug: Before _start */
  ldr x0, =0x28000000
  mov x1, #0x53            /* 'S' */
  str w1, [x0]
  mov x1, #0x54            /* 'T' */
  str w1, [x0]
  mov x1, #0x41            /* 'A' */
  str w1, [x0]
  mov x1, #0x52            /* 'R' */
  str w1, [x0]
  mov x1, #0x54            /* 'T' */
  str w1, [x0]
  mov x1, #0x0A            /* '\n' */
  str w1, [x0]
  mov x1, #0x0D            /* '\r' */
  str w1, [x0]

  /* No EL2 configuration - we expect to be in EL1 from GRUB/UEFI */
  
  bl _start

#include "/home/lyj/Phytium-RTEMS/phytium-rtems-sdk/rtems/rtems/bsps/aarch64/shared/start/start.S"
