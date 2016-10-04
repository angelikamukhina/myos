#include "controller.h"
#include "ioport.h"

#define CMD_MASTER_PORT      0x20
#define DAT_MASTER_PORT      0x21
#define CMD_SLAVE_PORT       0xA0
#define DAT_SLAVE_PORT       0xA1

static unsigned pic_irq_mask = 0xFFFFu;
static void contr_config(unsigned offset)
{
    out8(CMD_MASTER_PORT, (1 << 0) | (1 << 4));
    out8(CMD_SLAVE_PORT, (1 << 0) | (1 << 4));
    out8(DAT_MASTER_PORT, offset);
    out8(DAT_SLAVE_PORT, offset+8);
    out8(DAT_MASTER_PORT, (1 << 2));
    out8(DAT_SLAVE_PORT, (1 << 6));
    out8(DAT_MASTER_PORT, (1 << 0));
    out8(DAT_SLAVE_PORT, (1 << 0));
    
    	out8(DAT_MASTER_PORT, pic_irq_mask & 0xFFU);
	out8(DAT_SLAVE_PORT, (pic_irq_mask >> 8) & 0xFFU);
}


static void contr_mask(unsigned irq)
{

    static unsigned pic_irq_mask = 0xFFFFu;
    	pic_irq_mask |= 1u << irq;
	if (irq < 8)
		out8(DAT_MASTER_PORT, pic_irq_mask & 0xFFU);
	else
		out8(DAT_SLAVE_PORT, (pic_irq_mask >> 8) & 0xFFU);
}

static void contr_unmask(unsigned irq)
{
	pic_irq_mask &= ~(1u << irq);
	if (irq < 8)
		out8(DAT_MASTER_PORT, pic_irq_mask & 0xFFU);
	else
		out8(DAT_SLAVE_PORT, (pic_irq_mask >> 8) & 0xFFU);
}

static void contr_eoi(unsigned irq)
{
    if (irq >= 8)
    {
        out8(CMD_SLAVE_PORT, (1 << 5));
        out8(CMD_MASTER_PORT, (1 << 5) + (1 << 2));
    } else {
        out8(CMD_MASTER_PORT, (1 << 5));
    }
    
}

const struct contr i8259a = {
    .config = &contr_config,
    .mask = &contr_mask,
    .unmask = &contr_unmask,
    .eoi = &contr_eoi
};

