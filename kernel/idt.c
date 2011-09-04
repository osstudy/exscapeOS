#include <types.h>
#include <string.h>
#include <gdtidt.h>
#include <monitor.h>
#include <kernutil.h>

struct idt_entry {
	uint16 base_lo;
	uint16 sel;
	uint8 always0;
	uint8 flags;
	uint16 base_hi;
} __attribute__((packed));

struct idt_ptr {
	uint16 limit;
	uint32 base;
} __attribute__((packed));

extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();

/*
 * Declare an IDT of 256 entries. The first 32 are reserved for CPU exceptions,
 * the rest for other kinds of interrupts. Handling all interrupts
 * will also prevent unhandled interrupt exceptions.
 */
struct idt_entry idt[256];
struct idt_ptr idtp;

/* Implemented in kernel.s */
extern void idt_load(void);

/* Set an entry in the IDT */
void idt_set_gate(uint8 num, uint32 base, uint16 sel, uint8 flags) {
	idt[num].base_lo = (base & 0xffff);
	idt[num].base_hi = ((base >> 16) & 0xffff);
	idt[num].sel     = sel;
	idt[num].always0 = 0;
	idt[num].flags   = flags;
}

/* Installs the IDT; called from kmain() */
void idt_install(void) {
	/* Set the limit up - same theory here as with the GDT */
	idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
	idtp.base = (uint32)&idt;

	/* Zero the entire IDT; unused entries won't cause us any trouble */
	memset(&idt, 0, sizeof(struct idt_entry) * 256);

	/* Add ISRs */
	idt_set_gate(0, (uint32)isr0, 0x08, 0x8e);
	idt_set_gate(1, (uint32)isr1, 0x08, 0x8e);
	idt_set_gate(2, (uint32)isr2, 0x08, 0x8e);
	idt_set_gate(3, (uint32)isr3, 0x08, 0x8e);
	idt_set_gate(4, (uint32)isr4, 0x08, 0x8e);
	idt_set_gate(5, (uint32)isr5, 0x08, 0x8e);
	idt_set_gate(6, (uint32)isr6, 0x08, 0x8e);
	idt_set_gate(7, (uint32)isr7, 0x08, 0x8e);
	idt_set_gate(8, (uint32)isr8, 0x08, 0x8e);
	idt_set_gate(9, (uint32)isr9, 0x08, 0x8e);
	idt_set_gate(10, (uint32)isr10, 0x08, 0x8e);
	idt_set_gate(11, (uint32)isr11, 0x08, 0x8e);
	idt_set_gate(12, (uint32)isr12, 0x08, 0x8e);
	idt_set_gate(13, (uint32)isr13, 0x08, 0x8e);
	idt_set_gate(14, (uint32)isr14, 0x08, 0x8e);
	idt_set_gate(15, (uint32)isr15, 0x08, 0x8e);
	idt_set_gate(16, (uint32)isr16, 0x08, 0x8e);
	idt_set_gate(17, (uint32)isr17, 0x08, 0x8e);
	idt_set_gate(18, (uint32)isr18, 0x08, 0x8e);
	idt_set_gate(19, (uint32)isr19, 0x08, 0x8e);
	idt_set_gate(20, (uint32)isr20, 0x08, 0x8e);
	idt_set_gate(21, (uint32)isr21, 0x08, 0x8e);
	idt_set_gate(22, (uint32)isr22, 0x08, 0x8e);
	idt_set_gate(23, (uint32)isr23, 0x08, 0x8e);
	idt_set_gate(24, (uint32)isr24, 0x08, 0x8e);
	idt_set_gate(25, (uint32)isr25, 0x08, 0x8e);
	idt_set_gate(26, (uint32)isr26, 0x08, 0x8e);
	idt_set_gate(27, (uint32)isr27, 0x08, 0x8e);
	idt_set_gate(28, (uint32)isr28, 0x08, 0x8e);
	idt_set_gate(29, (uint32)isr29, 0x08, 0x8e);
	idt_set_gate(30, (uint32)isr30, 0x08, 0x8e);
	idt_set_gate(31, (uint32)isr31, 0x08, 0x8e);

	/* Load the new IDT */
	idt_load();
}

void isr_handler(registers_t regs) {
	printk("Received interrupt: %d\n", regs.int_no);
	panic("See above");
}