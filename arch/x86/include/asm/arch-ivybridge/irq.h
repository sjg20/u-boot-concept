/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2018, Bin Meng <bmeng.cn@gmail.com>
 *
 * IvyBridge specific IRQ converting logic
 */

#ifndef _IVYBRIDGE_IRQ_H_
#define _IVYBRIDGE_IRQ_H_

/**
 * pirq_reg_to_linkno() - Convert a PIRQ routing register offset to link number
 *
 * @reg:	PIRQ routing register offset from the base address
 * @base:	PIRQ routing register block base address
 * @return:	PIRQ link number (0 for PIRQA, 1 for PIRQB, etc)
 */
static inline int pirq_reg_to_linkno(int reg, int base)
{
	int linkno = reg - base;

	if (linkno > PIRQH)
		linkno -= 4;

	return linkno;
}

/**
 * pirq_linkno_to_reg() - Convert a PIRQ link number to routing register offset
 *
 * @linkno:	PIRQ link number (0 for PIRQA, 1 for PIRQB, etc)
 * @base:	PIRQ routing register block base address
 * @return:	PIRQ routing register offset from the base address
 */
static inline int pirq_linkno_to_reg(int linkno, int base)
{
	int reg = linkno + base;

	if (linkno > PIRQD)
		reg += 4;

	return reg;
}

#endif /* _IVYBRIDGE_IRQ_H_ */
