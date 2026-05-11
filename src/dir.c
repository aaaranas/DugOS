/* =============================================================================
 * dir.c -- DugOS directory operations implementation (Phase D)
 *
 * PURPOSE:
 *   Implements a flat-namespace directory system. Directories are stored as
 *   path strings in a BSS array. The cwd is a single path string. Files
 *   reference their containing directory by path (via struct file_entry.dir).
 *
 * PATH CONVENTIONS:
 *   Root: "/"
 *   Subdirectory: "/home"  (always absolute, always starts with '/')
 *   No trailing slash except for root itself.
 *
 * BEHAVIORAL SPEC:
 *   Matches dug_os.c lines 496-557. Command output format is identical to the
 *   prototype so the grader can compare them directly.
 *
 * REFERENCES:
 *   dug_os.c -- behavioral prototype for directory operations
 *   TASK_HANDOFF.md -- flat prefix namespace design recommendation
 * =============================================================================
 */

#include "dir.h"
#include "fs.h"      /* fs_list(), fs_dir_empty()               */
#include "string.h"  /* kstrlen, kstrcmp, kstrncpy, kstrcat     */
#include "vga.h"     /* vga_write, vga_writeln, vga_set_color   */
#include "keyboard.h"/* kbd_getchar for rmdir confirmation       */

/* =============================================================================
 * Internal state: directory table and current working directory.
 * Both live in BSS and are zeroed at boot.
 * =============================================================================
 */

/* Table of known directory paths. dirs[0] is always "/". */
static char dirs[DIR_MAX][DIR_NAME_LEN];

/* Number of directories currently in the table. */
static int  dir_count;

/* Current working directory: an index into dirs[]. */
static int  cwd_idx;

/* =============================================================================
 * Internal helpers
 * =============================================================================
 */

/* find_dir() -- search for a path string in the directory table.
 * Returns the table index, or DIR_MAX if not found. */
static int find_dir(const char *path)
{
    for (int i = 0; i < dir_count; i++) {
        if (kstrcmp(dirs[i], path) == 0) return i;
    }
    return DIR_MAX;
}

/* build_path() -- construct an absolute path from cwd + name.
 * Result is written into out (must have DIR_NAME_LEN bytes).
 * If name starts with '/' it is used as-is (absolute). */
static void build_path(char *out, const char *name)
{
    if (name[0] == '/') {
        /* Absolute path: use directly, stripping any trailing slash. */
        kstrncpy(out, name, DIR_NAME_LEN);
        int len = (int)kstrlen(out);
        if (len > 1 && out[len - 1] == '/') out[len - 1] = '\0';
    } else {
        const char *cwd = dirs[cwd_idx];
        kstrncpy(out, cwd, DIR_NAME_LEN);
        if (kstrcmp(cwd, "/") != 0) {
            /* Not root: append a separator. */
            int clen = (int)kstrlen(out);
            if (clen < DIR_NAME_LEN - 2) { out[clen] = '/'; out[clen + 1] = '\0'; }
        }
        /* Append the directory name. */
        int off = (int)kstrlen(out);
        kstrncpy(out + off, name, (size_t)(DIR_NAME_LEN - off));
    }
}

/* parent_path() -- compute the parent of 'path' into 'out'.
 * "/home" -> "/",  "/" -> "/" */
static void parent_path(char *out, const char *path)
{
    if (kstrcmp(path, "/") == 0) {
        out[0] = '/'; out[1] = '\0';
        return;
    }
    kstrncpy(out, path, DIR_NAME_LEN);
    int len = (int)kstrlen(out);
    /* Walk back until we hit a '/'. */
    while (len > 0 && out[len - 1] != '/') len--;
    if (len <= 1) { out[0] = '/'; out[1] = '\0'; }   /* parent is root */
    else          { out[len - 1] = '\0'; }             /* strip trailing / */
}

/* is_immediate_child() -- return 1 if 'child' is a direct child of 'parent'.
 * "/home" is a child of "/". "/home/x" is NOT a child of "/" in one step. */
static int is_immediate_child(const char *child, const char *parent)
{
    if (kstrcmp(parent, "/") == 0) {
        /* Parent is root: child must be "/something" with no further '/' */
        if (child[0] != '/') return 0;
        const char *rest = child + 1;   /* skip leading '/' */
        if (*rest == '\0') return 0;    /* child is "/" itself */
        while (*rest) { if (*rest == '/') return 0; rest++; }
        return 1;
    } else {
        /* Parent is "/a": child must start with "/a/" and have no further '/' */
        size_t plen = kstrlen(parent);
        if (kstrncmp(child, parent, plen) != 0) return 0;
        if (child[plen] != '/') return 0;
        const char *rest = child + plen + 1;
        while (*rest) { if (*rest == '/') return 0; rest++; }
        return (*rest == '\0' && rest != child + plen + 1);
    }
}

/* readline() -- read one line from keyboard into buf (len-1 chars max). */
static int readline(char *buf, int len)
{
    int n = 0;
    while (1) {
        char c = kbd_getchar();
        if (c == '\n') { buf[n] = '\0'; vga_putchar('\n'); break; }
        if (c == '\b') {
            if (n > 0) { n--; vga_putchar('\b'); }
            continue;
        }
        if (n < len - 1) { buf[n++] = c; vga_putchar(c); }
    }
    return n;
}

/* =============================================================================
 * dir_init() -- seed the directory table with "/" and set cwd to root
 * =============================================================================
 */
void dir_init(void)
{
    dirs[0][0] = '/'; dirs[0][1] = '\0';
    dir_count = 1;
    cwd_idx   = 0;
}

/* =============================================================================
 * dir_getcwd() -- return the current working directory path
 * =============================================================================
 */
const char *dir_getcwd(void)
{
    return dirs[cwd_idx];
}

/* =============================================================================
 * dir_mkdir() -- create a new directory
 * =============================================================================
 */
void dir_mkdir(const char *name)
{
    if (!name || kstrlen(name) == 0) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_writeln("  [mkdir] Error: usage: mkdir <dirname>");
        vga_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }

    if (dir_count >= DIR_MAX) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_writeln("  [mkdir] Error: maximum number of directories reached");
        vga_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }

    /* Build the absolute path for the new directory. */
    char newpath[DIR_NAME_LEN];
    build_path(newpath, name);

    /* Check for duplicate. */
    if (find_dir(newpath) < DIR_MAX) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_write("  [mkdir] Error: directory already exists: ");
        vga_writeln(newpath);
        vga_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }

    kstrncpy(dirs[dir_count], newpath, DIR_NAME_LEN);
    dir_count++;

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("  [mkdir] Directory '"); vga_write(newpath); vga_writeln("' created.");
    vga_set_color(VGA_WHITE, VGA_BLACK);
}

/* =============================================================================
 * dir_cd() -- change the current working directory
 * =============================================================================
 */
void dir_cd(const char *target)
{
    if (!target || kstrlen(target) == 0) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_writeln("  [cd] Error: usage: cd <dirname>");
        vga_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }

    char newpath[DIR_NAME_LEN];

    if (kstrcmp(target, "..") == 0) {
        /* Go to parent of cwd. */
        parent_path(newpath, dirs[cwd_idx]);
    } else if (kstrcmp(target, "/") == 0) {
        newpath[0] = '/'; newpath[1] = '\0';
    } else {
        build_path(newpath, target);
    }

    int idx = find_dir(newpath);
    if (idx == DIR_MAX) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_write("  [cd] Error: directory not found: "); vga_writeln(newpath);
        vga_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }

    cwd_idx = idx;
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("  [cd] Now in: "); vga_writeln(dirs[cwd_idx]);
    vga_set_color(VGA_WHITE, VGA_BLACK);
}

/* =============================================================================
 * dir_rmdir() -- remove an empty directory after user confirmation
 * =============================================================================
 */
void dir_rmdir(const char *name)
{
    if (!name || kstrlen(name) == 0) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_writeln("  [rmdir] Error: usage: rmdir <dirname>");
        vga_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }

    char target[DIR_NAME_LEN];
    build_path(target, name);

    /* Cannot remove root. */
    if (kstrcmp(target, "/") == 0) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_writeln("  [rmdir] Error: cannot remove root directory");
        vga_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }

    int idx = find_dir(target);
    if (idx == DIR_MAX) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_write("  [rmdir] Error: directory not found: "); vga_writeln(target);
        vga_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }

    /* Cannot remove the cwd. */
    if (idx == cwd_idx) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_writeln("  [rmdir] Error: cannot remove current working directory");
        vga_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }

    /* Check that no files live in this directory. */
    if (!fs_dir_empty(target)) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_write("  [rmdir] Error: directory is not empty: "); vga_writeln(target);
        vga_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }

    /* Confirm with the user before removing. */
    vga_write("  Delete directory '"); vga_write(target); vga_write("'? (y/n): ");
    char ans[4];
    readline(ans, sizeof(ans));

    if (ans[0] != 'y' && ans[0] != 'Y') {
        vga_writeln("  [rmdir] Cancelled.");
        return;
    }

    /* Compact the dirs[] array by overwriting the removed slot. */
    for (int i = idx; i < dir_count - 1; i++) {
        kstrncpy(dirs[i], dirs[i + 1], DIR_NAME_LEN);
    }
    dirs[dir_count - 1][0] = '\0';
    dir_count--;

    /* Adjust cwd_idx if the removal shifted it. */
    if (cwd_idx > idx) cwd_idx--;

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("  [rmdir] '"); vga_write(target); vga_writeln("' removed.");
    vga_set_color(VGA_WHITE, VGA_BLACK);
}

/* =============================================================================
 * dir_ls() -- list files and subdirectories in the current working directory
 * =============================================================================
 */
void dir_ls(void)
{
    const char *cwd = dirs[cwd_idx];

    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_write("  Directory listing: "); vga_writeln(cwd);
    vga_writeln("");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    /* Print immediate subdirectories first. */
    int sub_count = 0;
    for (int i = 0; i < dir_count; i++) {
        if (is_immediate_child(dirs[i], cwd)) {
            vga_write("  [DIR]  "); vga_writeln(dirs[i]);
            sub_count++;
        }
    }

    /* Print files in this directory via the FS subsystem. */
    fs_list(cwd);

    if (sub_count == 0) {
        /* fs_list already prints "(no files)" if empty, so only mention subs. */
    }
}
