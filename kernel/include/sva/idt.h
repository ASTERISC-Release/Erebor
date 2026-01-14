#ifndef _IDT_H
#define _IDT_H

#include <asm/desc.h>
#include <linux/types.h>
#include <sva/stack.h>

#include <asm/idtentry.h>

extern void sva_load_idt(void);
extern void sva_idt_setup_from_table(const struct idt_data*, int, bool);

#endif // _IDT_H