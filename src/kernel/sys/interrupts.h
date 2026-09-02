/**
 * CardboardOS - Interrupt Handler Header
 */

#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>

// Interrupt handler function type
typedef void (*interrupt_handler_t)(void);

// Initialize interrupt system
void init_interrupts(void);

// Register an interrupt handler
void register_interrupt_handler(uint8_t vector, interrupt_handler_t handler);

// Enable/disable interrupts
void enable_interrupts(void);
void disable_interrupts(void);

// Interrupt service routines
void isr_handler(void);
void irq_handler(void);

// Exception handlers
void exception_divide_error(void);
void exception_debug(void);
void exception_nmi(void);
void exception_breakpoint(void);
void exception_overflow(void);
void exception_bound_range(void);
void exception_invalid_opcode(void);
void exception_device_not_available(void);
void exception_double_fault(void);
void exception_coprocessor_segment(void);
void exception_invalid_tss(void);
void exception_segment_not_present(void);
void exception_stack_fault(void);
void exception_general_protection(void);
void exception_page_fault(void);
void exception_reserved(void);
void exception_math_fault(void);
void exception_alignment_check(void);
void exception_machine_check(void);
void exception_simd_fault(void);

#endif // INTERRUPTS_H