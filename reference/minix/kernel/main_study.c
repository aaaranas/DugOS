#include <stdio.h>
#include <string.h>

/* ============================================================
 * main_study.c — Simplified study version of MINIX kernel/main.c
 * Based on MINIX 3.1.0 (Appendix B, line 07100)
 *
 * HOW TO USE:
 *   Phase 1: Run as-is. Only intr_init() and announce() are live.
 *   Phase 2: Uncomment the process table loop block.
 *   Phase 3: Uncomment the privilege table loop block.
 *   Phase 4: Uncomment the boot image loop block.
 *   Phase 5: Uncomment prepare_shutdown() at the bottom.
 *
 * Compile:  gcc -o main_study main_study.c
 * Run:      ./main_study
 * ============================================================ */


/* ---- Simulated constants (real MINIX pulls these from kernel headers) ---- */
#define NR_TASKS        4
#define NR_PROCS        64
#define NR_BOOT_PROCS   8
#define NR_SYS_PROCS    32
#define SLOT_FREE       0x01
#define NO_MAP          0x02
#define SYS_PROC        0x10
#define NONE            -1
#define HARDWARE        -4      /* proc_nr of the HARDWARE pseudo-task */
#define OS_RELEASE      "3"
#define OS_VERSION      "1.0"
#define P_NAME_LEN      16


/* ---- Simulated data structures ---- */

struct proc {
    int  p_nr;
    int  p_rts_flags;
    int  p_max_priority;
    int  p_priority;
    int  p_quantum_size;
    int  p_ticks_left;
    char p_name[P_NAME_LEN];
};

struct priv {
    int s_proc_nr;
    int s_id;
    int s_flags;
};

struct boot_image {
    int  proc_nr;
    int  priority;
    int  quantum;
    int  flags;
    char proc_name[P_NAME_LEN];
};


/* ---- Global tables ---- */
struct proc     proc_table[NR_TASKS + NR_PROCS];
struct priv     priv_table[NR_SYS_PROCS];
int             shutdown_started = 0;

/* Simulated boot image — mirrors what the bootloader loaded */
struct boot_image image[NR_BOOT_PROCS] = {
    { -4, 0, 0,  SYS_PROC, "HARDWARE" },
    { -3, 1, 10, SYS_PROC, "IDLE"     },
    { -2, 2, 10, SYS_PROC, "CLOCK"    },
    { -1, 2, 10, SYS_PROC, "SYSTEM"   },
    {  0, 3, 10, SYS_PROC, "PM"       },
    {  1, 3, 10, SYS_PROC, "FS"       },
    {  2, 3, 10, SYS_PROC, "TTY"      },
    {  3, 4, 10, 0,        "INIT"     },
};


/* ==============================================================
 * PHASE 1 — intr_init()
 *
 * Real MINIX: Programs the Intel 8259A Programmable Interrupt
 * Controller chip via outb() port I/O. Remaps hardware IRQs so
 * the OS owns all interrupts (not the BIOS).
 * minit=1 -> set up for MINIX.  minit=0 -> restore BIOS defaults.
 * ============================================================== */
void intr_init(int minit)
{
    printf("[intr_init] called with minit=%d\n", minit);
    if (minit == 1) {
        printf("[intr_init] Remapping 8259A PIC: IRQs 0-7  -> vectors 0x20-0x27\n");
        printf("[intr_init] Remapping 8259A PIC: IRQs 8-15 -> vectors 0x28-0x2F\n");
        printf("[intr_init] Hardware interrupts now under OS control.\n\n");
    } else {
        printf("[intr_init] Restoring BIOS interrupt defaults (shutdown path).\n\n");
    }
}


/* ==============================================================
 * PHASE 1 — announce()
 *
 * Real MINIX: Prints the startup banner using kprintf() — the
 * kernel's own printf that bypasses the C library entirely.
 * First human-visible sign that MINIX booted successfully.
 * ============================================================== */
void announce(void)
{
    printf("[announce] MINIX %s.%s  "
           "Copyright 2006, Vrije Universiteit, Amsterdam, The Netherlands\n",
           OS_RELEASE, OS_VERSION);
    printf("[announce] Executing in 32-bit protected mode.\n\n");
}


/* ==============================================================
 * PHASE 5 — prepare_shutdown()
 *
 * Real MINIX: Sends SIGKSTOP to all living system processes,
 * sets a 1-second watchdog timer, then calls shutdown() to
 * reinitialize the PIC and return to the boot monitor or reset.
 * ============================================================== */
void prepare_shutdown(int how)
{
    int i;
    printf("[prepare_shutdown] how=%d\n", how);
    printf("[prepare_shutdown] Sending SIGKSTOP to system processes...\n");
    for (i = 0; i < NR_TASKS + NR_PROCS; i++) {
        if (proc_table[i].p_rts_flags != SLOT_FREE &&
            proc_table[i].p_nr != HARDWARE) {
            printf("[prepare_shutdown]   -> signaling proc %d (%s)\n",
                   proc_table[i].p_nr, proc_table[i].p_name);
        }
    }
    shutdown_started = 1;
    printf("[prepare_shutdown] Done. MINIX shutting down.\n\n");
}


/* ==============================================================
 * main() — Kernel entry point  (real: called from mpx386.s)
 *
 * Sequence in real MINIX:
 *   1. intr_init(1)         — reprogram the PIC
 *   2. clear proc table     — mark all slots SLOT_FREE
 *   3. clear priv table     — mark all privilege slots NONE
 *   4. boot image loop      — fill proc entries, enqueue to run
 *   5. announce()           — print startup banner
 *   6. restart()            — jump into scheduler (never returns)
 * ============================================================== */
int main(void)
{
    struct boot_image *ip;
    struct proc       *rp;
    struct priv       *sp;
    int i;

    printf("=== MINIX kernel main() starting ===\n\n");

    /* ----------------------------------------------------------
     * PHASE 1: Initialize the interrupt controller.
     * The CPU must not receive hardware IRQs until the PIC is
     * reprogrammed. This is always the very first call in main().
     * ---------------------------------------------------------- */
    intr_init(1);


    /* ----------------------------------------------------------
     * PHASE 2: Clear the process table.
     *
     * Every slot is marked SLOT_FREE so the scheduler ignores it.
     * p_nr stores the process number (-NR_TASKS .. NR_PROCS-1)
     * so the proc_addr() / proc_nr() macros work correctly.
     *
     * TODO: Uncomment this block for Phase 2.
     * ---------------------------------------------------------- */
    /*
    printf("[main] Phase 2: clearing process table (%d slots)...\n",
           NR_TASKS + NR_PROCS);
    for (rp = proc_table, i = -NR_TASKS;
         rp < proc_table + NR_TASKS + NR_PROCS;
         ++rp, ++i)
    {
        rp->p_rts_flags = SLOT_FREE;
        rp->p_nr        = i;
        printf("[proc_table] slot %-3d  p_nr=%-3d  SLOT_FREE\n",
               (int)(rp - proc_table), i);
    }
    printf("\n");
    */


    /* ----------------------------------------------------------
     * PHASE 3: Clear the privilege table.
     *
     * Each priv struct is assigned an index and marked as owned
     * by no process (NONE). System processes claim one later via
     * get_priv() inside the boot image loop.
     *
     * TODO: Uncomment this block for Phase 3.
     * ---------------------------------------------------------- */
    /*
    printf("[main] Phase 3: clearing privilege table (%d slots)...\n",
           NR_SYS_PROCS);
    for (sp = priv_table, i = 0;
         sp < priv_table + NR_SYS_PROCS;
         ++sp, ++i)
    {
        sp->s_proc_nr = NONE;
        sp->s_id      = i;
        printf("[priv_table] slot %d  s_proc_nr=NONE  s_id=%d\n", i, i);
    }
    printf("\n");
    */


    /* ----------------------------------------------------------
     * PHASE 4: Set up boot image processes.
     *
     * The bootloader placed each kernel process into memory and
     * built an a.out header array. Here main() iterates over
     * image[], fills scheduling fields, and marks each process
     * runnable (p_rts_flags = 0) so the scheduler picks it up.
     * HARDWARE is the exception — it is never scheduled.
     *
     * TODO: Uncomment this block for Phase 4.
     * ---------------------------------------------------------- */
    /*
    printf("[main] Phase 4: initializing %d boot image processes...\n\n",
           NR_BOOT_PROCS);
    for (i = 0; i < NR_BOOT_PROCS; i++) {
        ip = &image[i];
        rp = &proc_table[NR_TASKS + ip->proc_nr];

        rp->p_nr           = ip->proc_nr;
        rp->p_max_priority = ip->priority;
        rp->p_priority     = ip->priority;
        rp->p_quantum_size = ip->quantum;
        rp->p_ticks_left   = ip->quantum;
        strncpy(rp->p_name, ip->proc_name, P_NAME_LEN);

        if (rp->p_nr == HARDWARE) {
            rp->p_rts_flags = NO_MAP;
            printf("[boot_image] %-10s  proc_nr=%2d  -> NOT runnable (HARDWARE)\n",
                   rp->p_name, rp->p_nr);
        } else {
            rp->p_rts_flags = 0;
            printf("[boot_image] %-10s  proc_nr=%2d  priority=%d  quantum=%d"
                   "  -> RUNNABLE\n",
                   rp->p_name, rp->p_nr, rp->p_priority, rp->p_quantum_size);
        }
    }
    printf("\n");
    */


    /* Definitely not shutting down. */
    shutdown_started = 0;

    /* ----------------------------------------------------------
     * PHASE 1 (continued): Print the MINIX startup banner.
     * In real MINIX, restart() follows and never returns.
     * ---------------------------------------------------------- */
    announce();

    /* ----------------------------------------------------------
     * PHASE 5 (optional): Simulate shutdown.
     *
     * In real MINIX, shutdown is triggered externally (e.g. by
     * the PM on a reboot syscall). Uncomment to trace it here.
     *
     * TODO: Uncomment this block for Phase 5.
     * ---------------------------------------------------------- */
    /*
    printf("[main] Simulating shutdown sequence...\n\n");
    prepare_shutdown(0);
    */

    printf("=== Boot sequence complete (study mode -- restart() skipped) ===\n");
    return 0;
}