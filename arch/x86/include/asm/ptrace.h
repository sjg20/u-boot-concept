#ifndef _I386_PTRACE_H
#define _I386_PTRACE_H

#include <asm/types.h>

#define REBX 0
#define RECX 1
#define REDX 2
#define RESI 3
#define REDI 4
#define REBP 5
#define REAX 6
#define RDS 7
#define RES 8
#define RFS 9
#define RGS 10
#define RORIG_EAX 11
#define REIP 12
#define RCS  13
#define REFL 14
#define RUESP 15
#define RSS   16
#define RFRAME_SIZE 17

/* this struct defines the way the registers are stored on the
   stack during a system call. */

struct pt_regs {
	long ebx;
	long ecx;
	long edx;
	long esi;
	long edi;
	long ebp;
	long eax;
	int  xds;
	int  xes;
	int  xfs;
	int  xgs;
	long orig_eax;
	long eip;
	int  xcs;
	long eflags;
	long esp;
	int  xss;
}  __attribute__ ((packed));

struct irq_regs {
	/* Pushed by irq_common_entry */
	long ebx;
	long ecx;
	long edx;
	long esi;
	long edi;
	long ebp;
	long esp;
	long eax;
	long xds;
	long xes;
	long xfs;
	long xgs;
	long xss;
	/* Pushed by vector handler (irq_<num>) */
	long irq_id;
	/* Pushed by cpu in response to interrupt */
	long eip;
	long xcs;
	long eflags;
}  __attribute__ ((packed));

/* Arbitrarily choose the same ptrace numbers as used by the Sparc code. */
#define PTRACE_GETREGS            12
#define PTRACE_SETREGS            13
#define PTRACE_GETFPREGS          14
#define PTRACE_SETFPREGS          15
#define PTRACE_GETFPXREGS         18
#define PTRACE_SETFPXREGS         19

#define PTRACE_SETOPTIONS         21

/* options set using PTRACE_SETOPTIONS */
#define PTRACE_O_TRACESYSGOOD     0x00000001

#ifdef __KERNEL__
#define user_mode(regs) ((VM_MASK & (regs)->eflags) || (3 & (regs)->xcs))
#define instruction_pointer(regs) ((regs)->eip)
extern void show_regs(struct pt_regs *);
#endif

#endif
