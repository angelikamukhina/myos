#ifndef __CONTROLLER_H__
#define __CONTROLLER_H__

struct contr {
    void (*config)(unsigned);
    void (*mask)(unsigned);
    void (*unmask)(unsigned);
    void (*eoi)(unsigned);
};

extern const struct contr i8259a;

static inline void con_config(const struct contr *chip, unsigned offset)
{ if (chip->config) chip->config(offset); }

static inline void con_mask(const struct contr *chip, unsigned irq)
{ if (chip->mask) chip->mask(irq); }

static inline void con_unmask(const struct contr *chip, unsigned irq)
{ if (chip->mask) chip->unmask(irq); }

static inline void con_eoi(const struct contr *chip, unsigned irq)
{ if (chip->eoi) chip->eoi(irq); }

#endif