#include <linux/types.h>
#include <sva/stack.h>
#include <asm/desc.h>
#include <sva/idt.h>

#include <asm/idtentry.h>

static gate_desc sva_idt_table[IDT_ENTRIES] __page_aligned_bss;

#define IDT_TABLE_SIZE		(IDT_ENTRIES * sizeof(gate_desc))

static struct desc_ptr sva_idt_descr __ro_after_init = {
	.size		= IDT_TABLE_SIZE - 1,
	.address	= (unsigned long) sva_idt_table,
};

SECURE_WRAPPER(void,
sva_load_idt, void)
{
	asm volatile("lidt %0"::"m" (sva_idt_descr));
}

SECURE_WRAPPER(void,
sva_idt_setup_from_table, const struct idt_data *t, int size, bool sys)
{
	gate_desc desc;

	for (; size > 0; t++, size--) {
		idt_init_desc(&desc, t);
		write_idt_entry(sva_idt_table, t->vector, &desc);
		if (sys)
			set_bit(t->vector, system_vectors);
	}
}
