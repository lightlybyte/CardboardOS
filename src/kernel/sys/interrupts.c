/**
 * CardboardOS - Interrupt Handler Implementation
 */

#include "interrupts.h"
#include "../core/panic.h"
#include "../core/kmain.h"

// IDT entry structure
struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t flags;
    uint16_t base_mid;
    uint32_t base_high;
    uint32_t reserved;
} __attribute__((packed));

// IDT pointer structure
struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

// IRQ handlers
static interrupt_handler_t irq_handlers[16] = {0};

// IDT table
static struct idt_entry idt[256];
static struct idt_ptr idtp;

// External assembly functions
extern void load_idt(uint64_t idt_ptr);
extern void isr_wrapper(void);
extern void irq_wrapper(void);

void init_interrupts(void) {
    // Set up IDT entries
    // Real implementation would set up all 256 interrupt vectors
    
    // Load IDT
    idtp.limit = sizeof(idt) - 1;
    idtp.base = (uint64_t)&idt;
    load_idt((uint64_t)&idtp);
    
    // Enable interrupts
    enable_interrupts();
}

void register_interrupt_handler(uint8_t vector, interrupt_handler_t handler) {
    if (vector < 16) {
        irq_handlers[vector] = handler;
    }
}

void enable_interrupts(void) {
    __asm__ volatile("sti");
}

void disable_interrupts(void) {
    __asm__ volatile("cli");
}

void isr_handler(void) {
    // Handle CPU exceptions
    panic("CPU Exception occurred!");
}

void irq_handler(void) {
    // Handle IRQs
    // Acknowledge PIC
    // Call registered handlers
}

// Exception handlers
void exception_divide_error(void) { panic("Divide by zero error"); }
void exception_debug(void) { panic("Debug exception"); }
void exception_nmi(void) { panic("NMI interrupt"); }
void exception_breakpoint(void) { panic("Breakpoint"); }
void exception_overflow(void) { panic("Overflow"); }
void exception_bound_range(void) { panic("Bound range exceeded"); }
void exception_invalid_opcode(void) { panic("Invalid opcode"); }
void exception_device_not_available(void) { panic("Device not available"); }
void exception_double_fault(void) { panic("Double fault"); }
void exception_coprocessor_segment(void) { panic("Coprocessor segment overrun"); }
void exception_invalid_tss(void) { panic("Invalid TSS"); }
void exception_segment_not_present(void) { panic("Segment not present"); }
void exception_stack_fault(void) { panic("Stack fault"); }
void exception_general_protection(void) { panic("General protection fault"); }
void exception_page_fault(void) { panic("Page fault"); }
void exception_reserved(void) { panic("Reserved exception"); }
void exception_math_fault(void) { panic("Math fault"); }
void exception_alignment_check(void) { panic("Alignment check"); }
void exception_machine_check(void) { panic("Machine check"); }
void exception_simd_fault(void) { panic("SIMD fault"); }