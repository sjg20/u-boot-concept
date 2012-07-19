#ifndef __COMPAT_H__
#define __COMPAT_H__

/* linux compatibility defines */
#define DEFINE_SPINLOCK(a)	int a
#define spin_lock_irqsave(a,b)	do { b = *a; } while (0)
#define spin_unlock_irqrestore(a,b)	do { *a = b; } while (0)
#define IO_TO_VIRT(a)	(a)
#define WARN_ON(a)

#define readl(addr) (*(volatile unsigned int *)(addr))
#define writel(b, addr) ((*(volatile unsigned int *) (addr)) = (b))

#define TEGRA_APB_MISC_BASE		0x70000000
#define TEGRA_PMC_BASE			0x7000E400
#define TEGRA_CLK_RESET_BASE	0x60006000

#endif
