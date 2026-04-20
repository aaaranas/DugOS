/*
 * ============================================================
 *  dug_os.c  --  DugOS v1.0
 *
 *  A study OS built on top of MINIX 3.1.0 kernel/main.c
 *  concepts (Appendix B, line 07100).
 *
 *  Minimum requirements:
 *    [x] Boot sequence      (intr_init, process table, announce)
 *    [x] Welcome screen     (ASCII banner)
 *    [x] File operations    (read, write, edit, delete)
 *    [x] Directory ops      (create, change, delete, list)
 *    [x] Shutdown
 *
 *  Extra features:
 *    [x] help               (list all commands)
 *    [x] cls / clear        (clear screen)
 *    [x] whoami             (current user)
 *    [x] date               (system date/time)
 *    [x] rename             (rename a file)
 *    [x] copy               (copy a file)
 *    [x] pwd                (print working directory)
 *    [x] cat                (alias for fread)
 *
 *  Compile (Windows):   gcc -o dug_os.exe dug_os.c
 *  Run:                 .\dug_os.exe
 *  Compile (Linux/Mac): gcc -o dug_os dug_os.c
 *  Run:                 ./dug_os
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
  #include <direct.h>       /* _mkdir, _rmdir, _getcwd, _chdir */
  #include <windows.h>      /* Sleep(), system("cls")           */
  #define mkdir(p)    _mkdir(p)
  #define rmdir(p)    _rmdir(p)
  #define getcwd(b,s) _getcwd(b,s)
  #define chdir(p)    _chdir(p)
  #define CLEAR_CMD   "cls"
  #define SLEEP_MS(x) Sleep(x)
#else
  #include <unistd.h>       /* getcwd, chdir                    */
  #include <sys/stat.h>     /* mkdir                            */
  #define mkdir(p)    mkdir(p, 0755)
  #define CLEAR_CMD   "clear"
  #define SLEEP_MS(x) usleep((x)*1000)
#endif

/* ============================================================
 *  KERNEL CONSTANTS  (mirrors MINIX Appendix B)
 * ============================================================ */
#define NR_TASKS       4
#define NR_PROCS       64
#define NR_BOOT_PROCS  8
#define NR_SYS_PROCS   32
#define SLOT_FREE      0x01
#define NO_MAP         0x02
#define SYS_PROC       0x10
#define NONE           -1
#define HARDWARE       -4
#define OS_NAME        "DugOS"
#define OS_VERSION     "1.0"
#define OS_AUTHOR      "Built on MINIX 3.1.0 concepts"
#define P_NAME_LEN     16
#define CWD_LEN        512
#define CMD_LEN        256
#define ARG_LEN        128
#define MAX_ARGS       8
#define FILE_BUF       4096
#define USERNAME       "dug"

/* ============================================================
 *  KERNEL DATA STRUCTURES
 * ============================================================ */
struct proc {
    int  p_nr;
    int  p_rts_flags;
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

/* ============================================================
 *  GLOBAL KERNEL STATE
 * ============================================================ */
struct proc     proc_table[NR_TASKS + NR_PROCS];
struct priv     priv_table[NR_SYS_PROCS];
int             shutdown_started = 0;
int             os_running       = 1;
char            cwd[CWD_LEN];

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

/* ============================================================
 *  BOOT FUNCTIONS  (from MINIX main.c, line 07100)
 * ============================================================ */

/*
 * intr_init() -- Program the 8259A PIC.
 * minit=1 -> MINIX mode  |  minit=0 -> BIOS defaults (shutdown)
 */
static void intr_init(int minit)
{
    if (minit) {
        printf("  [kernel] intr_init: remapping 8259A PIC ... OK\n");
        printf("  [kernel] IRQs 0-7  -> vectors 0x20-0x27\n");
        printf("  [kernel] IRQs 8-15 -> vectors 0x28-0x2F\n");
    } else {
        printf("  [kernel] intr_init: restoring BIOS defaults ... OK\n");
    }
}

/*
 * boot_proc_table() -- Clear and initialize the process table.
 */
static void boot_proc_table(void)
{
    int i;
    struct proc *rp;
    for (rp = proc_table, i = -NR_TASKS;
         rp < proc_table + NR_TASKS + NR_PROCS;
         ++rp, ++i)
    {
        rp->p_rts_flags = SLOT_FREE;
        rp->p_nr        = i;
    }
    printf("  [kernel] proc_table: %d slots cleared ... OK\n",
           NR_TASKS + NR_PROCS);
}

/*
 * boot_priv_table() -- Clear and initialize the privilege table.
 */
static void boot_priv_table(void)
{
    int i;
    struct priv *sp;
    for (sp = priv_table, i = 0;
         sp < priv_table + NR_SYS_PROCS;
         ++sp, ++i)
    {
        sp->s_proc_nr = NONE;
        sp->s_id      = i;
    }
    printf("  [kernel] priv_table: %d slots cleared ... OK\n", NR_SYS_PROCS);
}

/*
 * boot_image_init() -- Load boot image processes into the process table.
 */
static void boot_image_init(void)
{
    int i;
    struct boot_image *ip;
    struct proc       *rp;

    for (i = 0; i < NR_BOOT_PROCS; i++) {
        ip = &image[i];
        rp = &proc_table[NR_TASKS + ip->proc_nr];
        rp->p_nr           = ip->proc_nr;
        rp->p_priority     = ip->priority;
        rp->p_quantum_size = ip->quantum;
        rp->p_ticks_left   = ip->quantum;
        strncpy(rp->p_name, ip->proc_name, P_NAME_LEN);
        rp->p_rts_flags    = (rp->p_nr == HARDWARE) ? NO_MAP : 0;
    }
    printf("  [kernel] boot_image: %d processes loaded  ... OK\n",
           NR_BOOT_PROCS);
}

/*
 * announce() -- Print the kernel startup line.
 */
static void announce(void)
{
    printf("  [kernel] %s v%s -- %s\n", OS_NAME, OS_VERSION, OS_AUTHOR);
    printf("  [kernel] Executing in 32-bit protected mode.\n");
}

/* ============================================================
 *  WELCOME SCREEN
 * ============================================================ */
static void welcome_screen(void)
{
    system(CLEAR_CMD);
    printf("\n");
    printf("  +=================================================================+\n");
    printf("  |                                                                 |\n");
    printf("  |    ____  _   _   ____   ___   ____                             |\n");
    printf("  |   |  _ \\| | | | / ___| / _ \\ / ___|                            |\n");
    printf("  |   | | | | | | || |  _ | | | |\\___ \\                            |\n");
    printf("  |   | |_| | |_| || |_| || |_| | ___) |                           |\n");
    printf("  |   |____/ \\___/  \\____| \\___/ |____/                             |\n");
    printf("  |                                                                 |\n");
    printf("  |          O P E R A T I N G     S Y S T E M                    |\n");
    printf("  |                      v e r s i o n  %-4s                       |\n", OS_VERSION);
    printf("  |                                                                 |\n");
    printf("  |   - - - - - - - - - - - - - - - - - - - - - - - - - - - - -   |\n");
    printf("  |      Built on MINIX 3.1.0 kernel  (Appendix B, line 07100)    |\n");
    printf("  |   - - - - - - - - - - - - - - - - - - - - - - - - - - - - -   |\n");
    printf("  |                                                                 |\n");
    printf("  |                  [ Type  'help'  to begin ]                    |\n");
    printf("  |                                                                 |\n");
    printf("  +=================================================================+\n");
    printf("\n");
    printf("  >> Welcome, %s. DugOS is ready.\n\n", USERNAME);
}

/* ============================================================
 *  UTILITY HELPERS
 * ============================================================ */

/* Refresh the global cwd buffer. */
static void refresh_cwd(void)
{
    if (!getcwd(cwd, CWD_LEN))
        strncpy(cwd, "?", CWD_LEN);
}

/* Trim leading/trailing whitespace in-place. */
static char *trim(char *s)
{
    char *end;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '\0') return s;
    end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' ||
                        *end == '\n' || *end == '\r'))
        end--;
    *(end + 1) = '\0';
    return s;
}

/*
 * parse_cmd() -- Split input into argv[].
 * Returns argc. argv[] pointers point into the original buffer.
 */
static int parse_cmd(char *input, char *argv[], int max_args)
{
    int argc = 0;
    char *tok = strtok(input, " \t\n");
    while (tok && argc < max_args) {
        argv[argc++] = tok;
        tok = strtok(NULL, " \t\n");
    }
    return argc;
}

/* Print a consistent error message. */
static void err(const char *cmd, const char *msg)
{
    printf("  [%s] Error: %s\n", cmd, msg);
}

/* ============================================================
 *  COMMAND IMPLEMENTATIONS
 * ============================================================ */

/* ---- help ---- */
static void cmd_help(void)
{
    printf("\n");
    printf("  +------------------------------------------------------------+\n");
    printf("  |              DugOS v%s  --  Command Reference            |\n", OS_VERSION);
    printf("  +------------------------------------------------------------+\n");
    printf("\n");
    printf("  [ General ]\n");
    printf("  %-22s  %s\n", "  help",              "Show this help screen");
    printf("  %-22s  %s\n", "  cls / clear",        "Clear the terminal screen");
    printf("  %-22s  %s\n", "  pwd",                "Print current working directory");
    printf("  %-22s  %s\n", "  date",               "Show current date and time");
    printf("  %-22s  %s\n", "  whoami",             "Show current user");
    printf("\n");
    printf("  [ File Operations ]\n");
    printf("  %-22s  %s\n", "  fwrite <file>",      "Create or overwrite a file");
    printf("  %-22s  %s\n", "  fread  <file>",      "Read and display a file");
    printf("  %-22s  %s\n", "  cat    <file>",      "Same as fread");
    printf("  %-22s  %s\n", "  fedit  <file>",      "Append content to a file");
    printf("  %-22s  %s\n", "  fdel   <file>",      "Delete a file");
    printf("  %-22s  %s\n", "  rename <old> <new>", "Rename a file");
    printf("  %-22s  %s\n", "  copy   <src> <dst>", "Copy a file");
    printf("\n");
    printf("  [ Directory Operations ]\n");
    printf("  %-22s  %s\n", "  mkdir  <dir>",       "Create a new directory");
    printf("  %-22s  %s\n", "  cd     <dir>",       "Change current directory");
    printf("  %-22s  %s\n", "  rmdir  <dir>",       "Delete an empty directory");
    printf("  %-22s  %s\n", "  ls / dir",           "List files in current directory");
    printf("\n");
    printf("  [ System ]\n");
    printf("  %-22s  %s\n", "  shutdown",           "Shut down DugOS");
    printf("\n");
    printf("  +------------------------------------------------------------+\n");
    printf("\n");
}

/* ---- cls / clear ---- */
static void cmd_clear(void)
{
    system(CLEAR_CMD);
}

/* ---- pwd ---- */
static void cmd_pwd(void)
{
    refresh_cwd();
    printf("  %s\n", cwd);
}

/* ---- date ---- */
static void cmd_date(void)
{
    time_t now = time(NULL);
    char buf[64];
    strftime(buf, sizeof(buf), "%A, %B %d %Y  %H:%M:%S", localtime(&now));
    printf("  %s\n", buf);
}

/* ---- whoami ---- */
static void cmd_whoami(void)
{
    printf("  %s\n", USERNAME);
}

/* ---- fwrite <file> ---- */
static void cmd_fwrite(const char *filename)
{
    if (!filename || strlen(filename) == 0) {
        err("fwrite", "usage: fwrite <filename>");
        return;
    }

    FILE *fp = fopen(filename, "w");
    if (!fp) { err("fwrite", "cannot create file"); return; }

    printf("  Enter content. Type a single dot '.' on a new line to finish:\n");
    printf("  > ");

    char line[256];
    while (fgets(line, sizeof(line), stdin)) {
        char *t = trim(line);
        if (strcmp(t, ".") == 0) break;
        fputs(line, fp);
        printf("  > ");
    }
    fclose(fp);
    printf("  [fwrite] '%s' written successfully.\n", filename);
}

/* ---- fread <file> / cat <file> ---- */
static void cmd_fread(const char *filename)
{
    if (!filename || strlen(filename) == 0) {
        err("fread", "usage: fread <filename>");
        return;
    }

    FILE *fp = fopen(filename, "r");
    if (!fp) { err("fread", "file not found"); return; }

    printf("  ---- %s ----\n", filename);
    char buf[FILE_BUF];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf) - 1, fp)) > 0) {
        buf[n] = '\0';
        /* Indent each line for consistent shell appearance */
        char *line = strtok(buf, "\n");
        while (line) {
            printf("  %s\n", line);
            line = strtok(NULL, "\n");
        }
    }
    printf("  ---- EOF ----\n");
    fclose(fp);
}

/* ---- fedit <file> ---- */
static void cmd_fedit(const char *filename)
{
    if (!filename || strlen(filename) == 0) {
        err("fedit", "usage: fedit <filename>");
        return;
    }

    /* Check if file exists first */
    FILE *check = fopen(filename, "r");
    if (!check) { err("fedit", "file not found -- use fwrite to create it first"); return; }
    fclose(check);

    FILE *fp = fopen(filename, "a");
    if (!fp) { err("fedit", "cannot open file for editing"); return; }

    printf("  Appending to '%s'. Type '.' on a new line to finish:\n", filename);
    printf("  > ");

    char line[256];
    while (fgets(line, sizeof(line), stdin)) {
        char *t = trim(line);
        if (strcmp(t, ".") == 0) break;
        fputs(line, fp);
        printf("  > ");
    }
    fclose(fp);
    printf("  [fedit] '%s' updated successfully.\n", filename);
}

/* ---- fdel <file> ---- */
static void cmd_fdel(const char *filename)
{
    if (!filename || strlen(filename) == 0) {
        err("fdel", "usage: fdel <filename>");
        return;
    }

    char confirm[8];
    printf("  Delete '%s'? (y/n): ", filename);
    if (!fgets(confirm, sizeof(confirm), stdin)) return;
    if (trim(confirm)[0] != 'y' && trim(confirm)[0] != 'Y') {
        printf("  [fdel] Cancelled.\n");
        return;
    }

    if (remove(filename) == 0)
        printf("  [fdel] '%s' deleted.\n", filename);
    else
        err("fdel", "could not delete file (does it exist?)");
}

/* ---- rename <old> <new> ---- */
static void cmd_rename(const char *oldname, const char *newname)
{
    if (!oldname || !newname ||
        strlen(oldname) == 0 || strlen(newname) == 0) {
        err("rename", "usage: rename <old> <new>");
        return;
    }
    if (rename(oldname, newname) == 0)
        printf("  [rename] '%s' -> '%s'\n", oldname, newname);
    else
        err("rename", "could not rename file");
}

/* ---- copy <src> <dst> ---- */
static void cmd_copy(const char *src, const char *dst)
{
    if (!src || !dst || strlen(src) == 0 || strlen(dst) == 0) {
        err("copy", "usage: copy <source> <destination>");
        return;
    }

    FILE *in  = fopen(src, "rb");
    if (!in)  { err("copy", "source file not found"); return; }

    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); err("copy", "cannot create destination file"); return; }

    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        fwrite(buf, 1, n, out);

    fclose(in);
    fclose(out);
    printf("  [copy] '%s' -> '%s'\n", src, dst);
}

/* ---- mkdir <dir> ---- */
static void cmd_mkdir(const char *dirname)
{
    if (!dirname || strlen(dirname) == 0) {
        err("mkdir", "usage: mkdir <dirname>");
        return;
    }
    if (mkdir(dirname) == 0)
        printf("  [mkdir] Directory '%s' created.\n", dirname);
    else
        err("mkdir", "could not create directory (already exists?)");
}

/* ---- cd <dir> ---- */
static void cmd_cd(const char *dirname)
{
    if (!dirname || strlen(dirname) == 0) {
        err("cd", "usage: cd <dirname>");
        return;
    }
    if (chdir(dirname) == 0) {
        refresh_cwd();
        printf("  [cd] Now in: %s\n", cwd);
    } else {
        err("cd", "directory not found");
    }
}

/* ---- rmdir <dir> ---- */
static void cmd_rmdir(const char *dirname)
{
    if (!dirname || strlen(dirname) == 0) {
        err("rmdir", "usage: rmdir <dirname>");
        return;
    }

    char confirm[8];
    printf("  Delete directory '%s'? (y/n): ", dirname);
    if (!fgets(confirm, sizeof(confirm), stdin)) return;
    if (trim(confirm)[0] != 'y' && trim(confirm)[0] != 'Y') {
        printf("  [rmdir] Cancelled.\n");
        return;
    }

    if (rmdir(dirname) == 0)
        printf("  [rmdir] '%s' removed.\n", dirname);
    else
        err("rmdir", "could not remove (is it empty and does it exist?)");
}

/* ---- ls / dir ---- */
static void cmd_ls(void)
{
    refresh_cwd();
    printf("  Directory listing: %s\n\n", cwd);

#ifdef _WIN32
    system("dir /B");
#else
    system("ls -lah --color=never");
#endif
    printf("\n");
}

/* ---- shutdown ---- */
static void cmd_shutdown(void)
{
    printf("\n");
    printf("  +----------------------------------------------------------+\n");
    printf("  |                  DugOS Shutdown                       |\n");
    printf("  +----------------------------------------------------------+\n");
    printf("\n");
    printf("  [kernel] Sending SIGKSTOP to all system processes...\n");

    int i;
    for (i = 0; i < NR_BOOT_PROCS; i++) {
        if (image[i].proc_nr != HARDWARE)
            printf("  [kernel]   -> stopping %-10s (proc %d)\n",
                   image[i].proc_name, image[i].proc_nr);
    }

    printf("  [kernel] Flushing file system buffers ... OK\n");
    printf("  [kernel] Stopping clock task          ... OK\n");
    printf("  [kernel] Masking all hardware IRQs    ... OK\n");
    intr_init(0);
    shutdown_started = 1;
    os_running       = 0;

    printf("\n  DugOS has shut down. Goodbye, %s.\n\n", USERNAME);
}

/* ============================================================
 *  SHELL -- command dispatcher
 * ============================================================ */
static void shell_dispatch(char *input)
{
    char *argv[MAX_ARGS];
    int   argc;
    char  buf[CMD_LEN];

    strncpy(buf, input, CMD_LEN - 1);
    buf[CMD_LEN - 1] = '\0';

    argc = parse_cmd(buf, argv, MAX_ARGS);
    if (argc == 0) return;

    char *cmd = argv[0];

    /* ---- System ---- */
    if      (strcmp(cmd, "help")     == 0) cmd_help();
    else if (strcmp(cmd, "cls")      == 0 ||
             strcmp(cmd, "clear")    == 0) cmd_clear();
    else if (strcmp(cmd, "pwd")      == 0) cmd_pwd();
    else if (strcmp(cmd, "date")     == 0) cmd_date();
    else if (strcmp(cmd, "whoami")   == 0) cmd_whoami();
    else if (strcmp(cmd, "shutdown") == 0) cmd_shutdown();

    /* ---- File ops ---- */
    else if (strcmp(cmd, "fwrite")   == 0) cmd_fwrite  (argc > 1 ? argv[1] : NULL);
    else if (strcmp(cmd, "fread")    == 0 ||
             strcmp(cmd, "cat")      == 0) cmd_fread   (argc > 1 ? argv[1] : NULL);
    else if (strcmp(cmd, "fedit")    == 0) cmd_fedit   (argc > 1 ? argv[1] : NULL);
    else if (strcmp(cmd, "fdel")     == 0) cmd_fdel    (argc > 1 ? argv[1] : NULL);
    else if (strcmp(cmd, "rename")   == 0) cmd_rename  (argc > 1 ? argv[1] : NULL,
                                                         argc > 2 ? argv[2] : NULL);
    else if (strcmp(cmd, "copy")     == 0) cmd_copy    (argc > 1 ? argv[1] : NULL,
                                                         argc > 2 ? argv[2] : NULL);

    /* ---- Directory ops ---- */
    else if (strcmp(cmd, "mkdir")    == 0) cmd_mkdir   (argc > 1 ? argv[1] : NULL);
    else if (strcmp(cmd, "cd")       == 0) cmd_cd      (argc > 1 ? argv[1] : NULL);
    else if (strcmp(cmd, "rmdir")    == 0) cmd_rmdir   (argc > 1 ? argv[1] : NULL);
    else if (strcmp(cmd, "ls")       == 0 ||
             strcmp(cmd, "dir")      == 0) cmd_ls();

    else
        printf("  Unknown command: '%s'  (type 'help' for a list)\n", cmd);
}

/* ============================================================
 *  SHELL LOOP
 * ============================================================ */
static void shell_run(void)
{
    char input[CMD_LEN];

    while (os_running) {
        refresh_cwd();
        printf("  dug@os:%s$ ", cwd);
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) break;

        char *trimmed = trim(input);
        if (strlen(trimmed) == 0) continue;

        shell_dispatch(trimmed);
        printf("\n");
    }
}

/* ============================================================
 *  KERNEL MAIN  (mirrors MINIX kernel/main.c, line 07100)
 * ============================================================ */
int main(void)
{
    /* ---- Phase 1: Boot sequence ---- */
    system(CLEAR_CMD);
    printf("\n  Booting %s v%s ...\n\n", OS_NAME, OS_VERSION);

    intr_init(1);
    boot_proc_table();
    boot_priv_table();
    boot_image_init();
    announce();

    printf("\n  Boot complete.\n");
    SLEEP_MS(1200);

    /* ---- Phase 2: Welcome screen ---- */
    welcome_screen();

    /* ---- Phase 3: Shell ---- */
    shell_run();

    return 0;
}
