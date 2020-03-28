#ifndef _TYPE_BEDBUG_H
#define _TYPE_BEDBUG_H

struct cmd_tbl;

/* Supporting routines */
int bedbug_puts (const char *);
void bedbug_init (void);
void bedbug860_init (void);
void do_bedbug_breakpoint (struct pt_regs *);
void bedbug_main_loop (unsigned long, struct pt_regs *);


typedef struct {
	int hw_debug_enabled;
	int stopped;
	int current_bp;
	struct pt_regs *regs;

	void (*do_break) (struct cmd_tbl *, int, int, char * const []);
	void (*break_isr) (struct pt_regs *);
	int (*find_empty) (void);
	int (*set) (int, unsigned long);
	int (*clear) (int);
} CPU_DEBUG_CTX;


#endif /* _TYPE_BEDBUG_H  */
