/* =============================================================================
 * fs.c -- DugOS in-memory FAT file system implementation (Phase C)
 *
 * PURPOSE:
 *   Implements an in-memory file system with FAT-style linked block allocation
 *   as required by the project spec (Req 5). All data lives in kernel BSS;
 *   contents are lost on reboot, which is acceptable for this course project.
 *
 * FAT MECHANICS:
 *   fat[i] = FAT_FREE (0x0000) : block i is not owned by any file
 *   fat[i] = FAT_END  (0xFFFF) : block i is the LAST block in a file's chain
 *   fat[i] = j (other)         : block i's next block in the chain is j
 *
 *   To read a file: start at files[n].first_block, follow the FAT chain until
 *   FAT_END, reading FS_BLOCK_SIZE bytes from each block (last block may be
 *   partial -- use files[n].size to know exactly how many bytes to read).
 *
 *   To allocate space: scan fat[] for FAT_FREE entries. Link them into a chain
 *   and store the head index in files[n].first_block.
 *
 *   To free a file: walk the chain and set each fat[i] = FAT_FREE.
 *
 * BEHAVIORAL SPEC:
 *   Command names, prompt strings, and error messages match dug_os.c (lines
 *   354-493) so the kernel shell is consistent with the prototype.
 *
 * REFERENCES:
 *   dug_os.c -- behavioral reference for file operations
 *   Project spec Req 5 -- FAT allocation requirement
 * =============================================================================
 */

#include "fs.h"
#include "vga.h"       /* vga_write, vga_writeln, vga_write_dec, vga_set_color */
#include "string.h"    /* kstrlen, kstrcmp, kstrncpy, kmemset, kmemcpy         */
#include "keyboard.h"  /* kbd_getchar for interactive input                    */

/* =============================================================================
 * Storage: these three arrays live in BSS and are zeroed at boot by GRUB.
 * =============================================================================
 */

/* The File Allocation Table: fat[i] is the next block in the chain, or a
 * sentinel (FAT_FREE / FAT_END). */
static uint16_t fat[FS_N_BLOCKS];

/* Raw data blocks: each is FS_BLOCK_SIZE bytes. File data is stored here,
 * linked through fat[]. This array is the bulk of the FS's memory footprint. */
static uint8_t blocks[FS_N_BLOCKS][FS_BLOCK_SIZE];

/* Flat directory: one slot per possible file. 'used' flag distinguishes
 * live entries from free slots. */
static struct file_entry files[FS_MAX_FILES];

/* =============================================================================
 * Internal helpers
 * =============================================================================
 */

/* alloc_block() -- find the first free FAT block and return its index.
 * Returns FS_N_BLOCKS (out-of-range) if no block is free. */
static uint16_t alloc_block(void)
{
    for (uint16_t i = 0; i < FS_N_BLOCKS; i++) {
        if (fat[i] == FAT_FREE) return i;
    }
    return FS_N_BLOCKS;   /* sentinel: no free block available */
}

/* free_chain() -- walk a FAT chain starting at 'start' and mark all
 * blocks in the chain as FAT_FREE. */
static void free_chain(uint16_t start)
{
    while (start < FS_N_BLOCKS && start != FAT_END && start != FAT_FREE) {
        uint16_t next = fat[start];
        fat[start] = FAT_FREE;
        start = next;
    }
}

/* find_file() -- search the directory for a file by name and directory.
 * Returns the index into files[] or FS_MAX_FILES if not found. */
static int find_file(const char *name, const char *dir)
{
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (files[i].used &&
            kstrcmp(files[i].name, name) == 0 &&
            kstrcmp(files[i].dir,  dir)  == 0)
        {
            return i;
        }
    }
    return FS_MAX_FILES;   /* not found */
}

/* find_free_slot() -- return the first unused file entry index, or
 * FS_MAX_FILES if the directory is full. */
static int find_free_slot(void)
{
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (!files[i].used) return i;
    }
    return FS_MAX_FILES;
}

/* readline() -- read a line of user input into buf (max len-1 chars + '\0').
 * Echoes characters to VGA. Handles backspace. Stops on Enter ('\n').
 * Returns the number of characters read (not counting '\0'). */
static int readline(char *buf, int len)
{
    int n = 0;
    while (1) {
        char c = kbd_getchar();
        if (c == '\n') {
            buf[n] = '\0';
            vga_putchar('\n');
            break;
        }
        if (c == '\b') {
            if (n > 0) {
                n--;
                vga_putchar('\b');   /* vga_putchar handles backspace erasing */
            }
            continue;
        }
        if (n < len - 1) {
            buf[n++] = c;
            vga_putchar(c);
        }
    }
    return n;
}

/* trim_line() -- strip trailing whitespace (\r \n \t space) in place. */
static void trim_line(char *s)
{
    int i = (int)kstrlen(s) - 1;
    while (i >= 0 && (s[i] == ' ' || s[i] == '\t' ||
                      s[i] == '\r' || s[i] == '\n')) {
        s[i--] = '\0';
    }
}

/* write_to_blocks() -- write 'len' bytes from 'data' into a new FAT chain.
 * Returns the index of the first block in the chain, or FAT_END if len==0,
 * or FS_N_BLOCKS on allocation failure. */
static uint16_t write_to_blocks(const uint8_t *data, uint32_t len)
{
    if (len == 0) return (uint16_t)FAT_END;

    uint16_t first = (uint16_t)FAT_END;
    uint16_t prev  = (uint16_t)FAT_END;
    uint32_t written = 0;

    while (written < len) {
        uint16_t blk = alloc_block();
        if (blk == FS_N_BLOCKS) return FS_N_BLOCKS;   /* disk full */

        fat[blk] = (uint16_t)FAT_END;   /* initially the last block */
        if (prev != (uint16_t)FAT_END) fat[prev] = blk;  /* link prev -> blk */
        if (first == (uint16_t)FAT_END) first = blk;     /* remember chain head */

        uint32_t chunk = len - written;
        if (chunk > FS_BLOCK_SIZE) chunk = FS_BLOCK_SIZE;
        kmemcpy(blocks[blk], data + written, chunk);

        prev    = blk;
        written += chunk;
    }
    return first;
}

/* =============================================================================
 * fs_init() -- clear FAT and file directory (called once at boot)
 * =============================================================================
 */
void fs_init(void)
{
    /* Mark all FAT blocks as free. */
    for (int i = 0; i < FS_N_BLOCKS; i++) fat[i] = FAT_FREE;

    /* Clear all file directory slots. */
    for (int i = 0; i < FS_MAX_FILES; i++) {
        files[i].used = 0;
        files[i].size = 0;
        files[i].first_block = (uint16_t)FAT_END;
    }
}

/* =============================================================================
 * fs_write() -- create or overwrite a file with interactive keyboard input
 * =============================================================================
 */
void fs_write(const char *name, const char *dir)
{
    if (!name || kstrlen(name) == 0) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_writeln("  [fwrite] Error: usage: fwrite <filename>");
        vga_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }

    /* If the file already exists, free its blocks so they can be reused. */
    int idx = find_file(name, dir);
    if (idx < FS_MAX_FILES) {
        free_chain(files[idx].first_block);
        files[idx].first_block = (uint16_t)FAT_END;
        files[idx].size        = 0;
    } else {
        /* New file: find a free directory slot. */
        idx = find_free_slot();
        if (idx == FS_MAX_FILES) {
            vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
            vga_writeln("  [fwrite] Error: directory is full (max 32 files)");
            vga_set_color(VGA_WHITE, VGA_BLACK);
            return;
        }
        kstrncpy(files[idx].name, name, FS_NAME_LEN);
        kstrncpy(files[idx].dir,  dir,  FS_DIR_LEN);
        files[idx].used = 1;
    }

    /* Collect user input into a temporary buffer, then write it to blocks. */
    static uint8_t tmp[FS_N_BLOCKS * FS_BLOCK_SIZE];  /* static: not on stack */
    uint32_t total = 0;
    char line[256];

    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_writeln("  Enter content. Type a single '.' on a new line to finish:");

    while (1) {
        vga_write("  > ");
        int n = readline(line, sizeof(line));
        (void)n;
        trim_line(line);

        if (kstrcmp(line, ".") == 0) break;   /* terminator line */

        /* Append the line and a newline to the accumulation buffer. */
        uint32_t line_len = (uint32_t)kstrlen(line);
        if (total + line_len + 1 <= sizeof(tmp)) {
            kmemcpy(tmp + total, line, line_len);
            total += line_len;
            tmp[total++] = '\n';
        }
    }

    /* Write accumulated data into FAT blocks. */
    uint16_t first = (total == 0) ? (uint16_t)FAT_END : write_to_blocks(tmp, total);
    if (first == FS_N_BLOCKS) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_writeln("  [fwrite] Error: disk full -- not enough free blocks");
        vga_set_color(VGA_WHITE, VGA_BLACK);
        files[idx].used = 0;   /* abandon the slot */
        return;
    }

    files[idx].first_block = first;
    files[idx].size        = total;

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("  [fwrite] '");
    vga_write(name);
    vga_writeln("' written successfully.");
    vga_set_color(VGA_WHITE, VGA_BLACK);
}

/* =============================================================================
 * fs_read() -- display the content of a file
 * =============================================================================
 */
void fs_read(const char *name, const char *dir)
{
    if (!name || kstrlen(name) == 0) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_writeln("  [fread] Error: usage: fread <filename>");
        vga_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }

    int idx = find_file(name, dir);
    if (idx == FS_MAX_FILES) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_write("  [fread] Error: file not found: ");
        vga_writeln(name);
        vga_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }

    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_write("  ---- "); vga_write(name); vga_writeln(" ----");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    /* Walk the FAT chain, printing block data byte by byte. */
    uint16_t blk   = files[idx].first_block;
    uint32_t left  = files[idx].size;

    if (blk == (uint16_t)FAT_END || left == 0) {
        vga_writeln("  (empty file)");
    } else {
        /* Print two-space indent, then each character. */
        vga_write("  ");
        while (blk < FS_N_BLOCKS && left > 0) {
            uint32_t chunk = left < FS_BLOCK_SIZE ? left : FS_BLOCK_SIZE;
            for (uint32_t i = 0; i < chunk; i++) {
                char c = (char)blocks[blk][i];
                vga_putchar(c);
                if (c == '\n') vga_write("  ");   /* re-indent after newline */
            }
            left -= chunk;
            blk   = fat[blk];
        }
        vga_putchar('\n');
    }

    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_writeln("  ---- EOF ----");
    vga_set_color(VGA_WHITE, VGA_BLACK);
}

/* =============================================================================
 * fs_edit() -- append content to an existing file interactively
 * =============================================================================
 */
void fs_edit(const char *name, const char *dir)
{
    if (!name || kstrlen(name) == 0) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_writeln("  [fedit] Error: usage: fedit <filename>");
        vga_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }

    int idx = find_file(name, dir);
    if (idx == FS_MAX_FILES) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_write("  [fedit] Error: file not found: ");
        vga_writeln(name);
        vga_writeln("  Use fwrite to create it first.");
        vga_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }

    /* Read existing content into a temporary buffer. */
    static uint8_t tmp[FS_N_BLOCKS * FS_BLOCK_SIZE];
    uint32_t total = files[idx].size;

    /* Copy existing data into tmp. */
    {
        uint16_t blk  = files[idx].first_block;
        uint32_t left = files[idx].size;
        uint32_t off  = 0;
        while (blk < FS_N_BLOCKS && left > 0) {
            uint32_t chunk = left < FS_BLOCK_SIZE ? left : FS_BLOCK_SIZE;
            kmemcpy(tmp + off, blocks[blk], chunk);
            off  += chunk;
            left -= chunk;
            blk   = fat[blk];
        }
    }

    /* Collect appended input. */
    char line[256];
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_write("  Appending to '"); vga_write(name); vga_writeln("'. Type '.' to finish:");

    while (1) {
        vga_write("  > ");
        int n = readline(line, sizeof(line));
        (void)n;
        trim_line(line);

        if (kstrcmp(line, ".") == 0) break;

        uint32_t line_len = (uint32_t)kstrlen(line);
        if (total + line_len + 1 <= sizeof(tmp)) {
            kmemcpy(tmp + total, line, line_len);
            total += line_len;
            tmp[total++] = '\n';
        }
    }

    /* Free old blocks and write the combined content back. */
    free_chain(files[idx].first_block);
    uint16_t first = (total == 0) ? (uint16_t)FAT_END : write_to_blocks(tmp, total);
    if (first == FS_N_BLOCKS) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_writeln("  [fedit] Error: disk full");
        vga_set_color(VGA_WHITE, VGA_BLACK);
        files[idx].first_block = (uint16_t)FAT_END;
        files[idx].size        = 0;
        return;
    }
    files[idx].first_block = first;
    files[idx].size        = total;

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("  [fedit] '"); vga_write(name); vga_writeln("' updated successfully.");
    vga_set_color(VGA_WHITE, VGA_BLACK);
}

/* =============================================================================
 * fs_delete() -- delete a file after user confirmation
 * =============================================================================
 */
void fs_delete(const char *name, const char *dir)
{
    if (!name || kstrlen(name) == 0) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_writeln("  [fdel] Error: usage: fdel <filename>");
        vga_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }

    int idx = find_file(name, dir);
    if (idx == FS_MAX_FILES) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_write("  [fdel] Error: file not found: ");
        vga_writeln(name);
        vga_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }

    /* Prompt for confirmation before destructive delete. */
    vga_write("  Delete '"); vga_write(name); vga_write("'? (y/n): ");

    char ans[4];
    readline(ans, sizeof(ans));

    if (ans[0] != 'y' && ans[0] != 'Y') {
        vga_writeln("  [fdel] Cancelled.");
        return;
    }

    free_chain(files[idx].first_block);
    files[idx].used        = 0;
    files[idx].size        = 0;
    files[idx].first_block = (uint16_t)FAT_END;
    files[idx].name[0]     = '\0';

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("  [fdel] '"); vga_write(name); vga_writeln("' deleted.");
    vga_set_color(VGA_WHITE, VGA_BLACK);
}

/* =============================================================================
 * fs_rename() -- rename a file
 * =============================================================================
 */
void fs_rename(const char *oldname, const char *newname, const char *dir)
{
    if (!oldname || !newname ||
        kstrlen(oldname) == 0 || kstrlen(newname) == 0) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_writeln("  [rename] Error: usage: rename <old> <new>");
        vga_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }

    int idx = find_file(oldname, dir);
    if (idx == FS_MAX_FILES) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_write("  [rename] Error: file not found: ");
        vga_writeln(oldname);
        vga_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }

    /* Check that the new name is not already taken. */
    if (find_file(newname, dir) < FS_MAX_FILES) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_write("  [rename] Error: '");
        vga_write(newname);
        vga_writeln("' already exists");
        vga_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }

    kstrncpy(files[idx].name, newname, FS_NAME_LEN);

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("  [rename] '"); vga_write(oldname);
    vga_write("' -> '");       vga_write(newname); vga_writeln("'");
    vga_set_color(VGA_WHITE, VGA_BLACK);
}

/* =============================================================================
 * fs_copy() -- copy a file to a new name in the same directory
 * =============================================================================
 */
void fs_copy(const char *src, const char *dst, const char *dir)
{
    if (!src || !dst || kstrlen(src) == 0 || kstrlen(dst) == 0) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_writeln("  [copy] Error: usage: copy <source> <destination>");
        vga_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }

    int sidx = find_file(src, dir);
    if (sidx == FS_MAX_FILES) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_write("  [copy] Error: source not found: "); vga_writeln(src);
        vga_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }

    if (find_file(dst, dir) < FS_MAX_FILES) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_write("  [copy] Error: destination already exists: "); vga_writeln(dst);
        vga_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }

    int didx = find_free_slot();
    if (didx == FS_MAX_FILES) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_writeln("  [copy] Error: directory full");
        vga_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }

    /* Read source content into a temporary buffer, then write to new blocks. */
    static uint8_t tmp[FS_N_BLOCKS * FS_BLOCK_SIZE];
    uint32_t total = files[sidx].size;

    {
        uint16_t blk  = files[sidx].first_block;
        uint32_t left = files[sidx].size;
        uint32_t off  = 0;
        while (blk < FS_N_BLOCKS && left > 0) {
            uint32_t chunk = left < FS_BLOCK_SIZE ? left : FS_BLOCK_SIZE;
            kmemcpy(tmp + off, blocks[blk], chunk);
            off  += chunk;
            left -= chunk;
            blk   = fat[blk];
        }
    }

    uint16_t first = (total == 0) ? (uint16_t)FAT_END : write_to_blocks(tmp, total);
    if (first == FS_N_BLOCKS) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_writeln("  [copy] Error: disk full");
        vga_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }

    kstrncpy(files[didx].name, dst,  FS_NAME_LEN);
    kstrncpy(files[didx].dir,  dir,  FS_DIR_LEN);
    files[didx].first_block = first;
    files[didx].size        = total;
    files[didx].used        = 1;

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("  [copy] '"); vga_write(src);
    vga_write("' -> '");     vga_write(dst); vga_writeln("'");
    vga_set_color(VGA_WHITE, VGA_BLACK);
}

/* =============================================================================
 * fs_list() -- list all files in a given directory
 * =============================================================================
 */
void fs_list(const char *dir)
{
    int count = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (!files[i].used) continue;
        if (kstrcmp(files[i].dir, dir) != 0) continue;

        vga_write("  ");
        vga_write(files[i].name);
        vga_write("  (");
        vga_write_dec(files[i].size);
        vga_writeln(" bytes)");
        count++;
    }
    if (count == 0) {
        vga_writeln("  (no files in this directory)");
    }
}

/* =============================================================================
 * fs_dir_empty() -- return 1 if no files exist in the given directory
 * =============================================================================
 */
int fs_dir_empty(const char *dir)
{
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (files[i].used && kstrcmp(files[i].dir, dir) == 0) return 0;
    }
    return 1;
}
