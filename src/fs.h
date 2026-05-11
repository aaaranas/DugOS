/* =============================================================================
 * fs.h -- DugOS in-memory FAT file system interface (Phase C)
 *
 * PURPOSE:
 *   Declares the interface for DugOS's in-memory file system. Files are stored
 *   entirely in kernel BSS (RAM only -- contents are lost on reboot, which is
 *   acceptable for a course OS). Storage uses a FAT-style linked allocation
 *   scheme as required by the project spec (Req 5).
 *
 * DESIGN DECISIONS (resolved per TASK_HANDOFF -- do not change unilaterally):
 *
 *   Allocation method: LINKED (FAT-style)
 *     fat[i] holds the index of the next block in the chain, or FAT_END if this
 *     is the last block, or FAT_FREE if this block is unused. This exactly
 *     mirrors a simplified FAT12/16 File Allocation Table.
 *
 *   Block size: 32 KB (FS_BLOCK_SIZE = 32768 bytes)
 *     Each file uses one or more 32 KB blocks linked through the FAT.
 *
 *   Number of blocks: FS_N_BLOCKS = 8 (total storage: 256 KB in BSS)
 *
 *   Root directory: flat array of up to FS_MAX_FILES file_entry structs.
 *     Each entry holds the file name, the directory it lives in, the index
 *     of its first FAT block, its size in bytes, and a 'used' flag.
 *
 * DATA LAYOUT:
 *   fat[0..N-1]          -- uint16_t FAT table (next block or sentinel)
 *   blocks[0..N-1][...]  -- raw 32 KB data blocks in BSS
 *   files[0..MAX-1]      -- struct file_entry directory array in BSS
 *
 * SENTINEL VALUES:
 *   FAT_FREE (0x0000) -- block is not allocated to any file
 *   FAT_END  (0xFFFF) -- this is the last block in a chain
 *
 * REFERENCES:
 *   dug_os.c lines 354-493 -- behavioral spec (command names and messages)
 *   Project spec Req 5 -- FAT with linked or indexed allocation required
 * =============================================================================
 */

#ifndef DUGOS_FS_H
#define DUGOS_FS_H

#include <stdint.h>

/* ---- Configuration ---------------------------------------------------------*/
#define FS_N_BLOCKS    8        /* number of 32 KB blocks; 8 x 32 KB = 256 KB  */
#define FS_BLOCK_SIZE  32768   /* bytes per block (32 KiB)                     */
#define FS_MAX_FILES   32      /* maximum number of files in the root directory */
#define FS_NAME_LEN    64      /* maximum file name length including '\0'       */
#define FS_DIR_LEN     64      /* maximum directory path length including '\0'  */

/* ---- FAT sentinel values ---------------------------------------------------*/
#define FAT_FREE  0x0000u   /* block not allocated      */
#define FAT_END   0xFFFFu   /* last block in file chain */

/* =============================================================================
 * struct file_entry -- one slot in the flat root directory
 *
 * Each file occupies exactly one of these slots. The 'used' flag distinguishes
 * live entries from free slots. 'first_block' is the index into fat[] of the
 * first block of the file's data chain (FAT_END if the file is empty).
 * =============================================================================
 */
struct file_entry {
    char     name[FS_NAME_LEN];  /* null-terminated file name                 */
    char     dir[FS_DIR_LEN];    /* null-terminated directory path (e.g. "/") */
    uint16_t first_block;        /* index of first block in FAT chain         */
    uint32_t size;               /* total file size in bytes                  */
    uint8_t  used;               /* 1 = slot is occupied, 0 = slot is free    */
};

/* =============================================================================
 * fs_init() -- initialize the file system (called once at boot)
 *
 * Zeros the FAT table, marks all blocks free, and clears the file directory.
 * Must be called before any other fs_* function.
 * =============================================================================
 */
void fs_init(void);

/* =============================================================================
 * fs_write() -- create or overwrite a file with content read from keyboard
 *
 * Parameters:
 *   name -- null-terminated file name (up to FS_NAME_LEN-1 characters)
 *   dir  -- null-terminated directory path the file belongs to
 *
 * Reads lines of text interactively (via kbd_getchar) until the user types
 * a single period '.' on an otherwise empty line. Creates the file in the
 * directory 'dir'. If a file with the same name already exists in that
 * directory, its old content is freed and replaced.
 * =============================================================================
 */
void fs_write(const char *name, const char *dir);

/* =============================================================================
 * fs_read() -- display the content of a file to the VGA screen
 *
 * Parameters:
 *   name -- file name to read
 *   dir  -- directory path containing the file
 * =============================================================================
 */
void fs_read(const char *name, const char *dir);

/* =============================================================================
 * fs_edit() -- append content to an existing file (interactive)
 *
 * Parameters:
 *   name -- file to append to (must already exist)
 *   dir  -- directory path
 *
 * Reads lines interactively until '.' on an empty line, appending each to the
 * file's existing content. Reports an error if the file does not exist.
 * =============================================================================
 */
void fs_edit(const char *name, const char *dir);

/* =============================================================================
 * fs_delete() -- remove a file after interactive confirmation
 *
 * Parameters:
 *   name -- file name to delete
 *   dir  -- directory path
 *
 * Prompts 'Delete X? (y/n)'. On 'y', frees all FAT blocks and clears the
 * directory slot. On anything else, prints "Cancelled."
 * =============================================================================
 */
void fs_delete(const char *name, const char *dir);

/* =============================================================================
 * fs_rename() -- rename a file
 *
 * Parameters:
 *   oldname -- current file name
 *   newname -- new file name
 *   dir     -- directory containing the file
 * =============================================================================
 */
void fs_rename(const char *oldname, const char *newname, const char *dir);

/* =============================================================================
 * fs_copy() -- copy a file to a new name in the same directory
 *
 * Parameters:
 *   src  -- source file name
 *   dst  -- destination file name (must not already exist)
 *   dir  -- directory containing src (dst is placed in the same directory)
 * =============================================================================
 */
void fs_copy(const char *src, const char *dst, const char *dir);

/* =============================================================================
 * fs_list() -- list all files in a directory (called by shell 'ls')
 *
 * Parameter:
 *   dir -- directory path to list (NULL or "/" for root)
 *
 * Prints each file name and size on a separate line to the VGA screen.
 * =============================================================================
 */
void fs_list(const char *dir);

/* =============================================================================
 * fs_delete_dir() -- remove all files in a given directory (called by rmdir)
 *
 * Used internally by the directory subsystem when a directory is deleted.
 * Returns 1 if the directory is empty (no files), 0 if files remain.
 * =============================================================================
 */
int fs_dir_empty(const char *dir);

#endif /* DUGOS_FS_H */
