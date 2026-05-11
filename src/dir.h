/* =============================================================================
 * dir.h -- DugOS directory operations interface (Phase D)
 *
 * PURPOSE:
 *   Declares the interface for creating, navigating, listing, and removing
 *   directories in the DugOS kernel. Directories are implemented using a
 *   flat prefix namespace (not a tree) as recommended in TASK_HANDOFF.md.
 *
 * DESIGN -- FLAT PREFIX NAMESPACE:
 *   A flat array of directory path strings is maintained in BSS. Each entry
 *   is a null-terminated path like "/" (root), "/home", "/docs". The current
 *   working directory (cwd) is tracked as a single path string.
 *
 *   Files belong to a directory by having their 'dir' field match a directory
 *   path (see struct file_entry in fs.h). The file system (fs.c) and directory
 *   subsystem share the same path convention: root = "/".
 *
 *   This approach is simpler than maintaining parent/child tree pointers and
 *   is sufficient for the course rubric (mkdir, cd, rmdir, ls commands).
 *
 *   The '/' root directory always exists and cannot be deleted.
 *
 * BEHAVIORAL SPEC:
 *   Command names, prompts, and error messages match dug_os.c (lines 496-557).
 *
 * REFERENCES:
 *   dug_os.c -- behavioral reference for directory operations
 *   TASK_HANDOFF.md -- flat prefix namespace recommendation
 * =============================================================================
 */

#ifndef DUGOS_DIR_H
#define DUGOS_DIR_H

#define DIR_NAME_LEN 64   /* maximum length of a directory path (incl. '\0') */
#define DIR_MAX      32   /* maximum number of directories (including root)   */

/* =============================================================================
 * dir_init() -- initialize the directory subsystem
 *
 * Creates the root directory "/" and sets cwd to "/". Must be called once
 * at boot before any other dir_* function.
 * =============================================================================
 */
void dir_init(void);

/* =============================================================================
 * dir_getcwd() -- return the current working directory path
 *
 * Returns:
 *   Pointer to the internal null-terminated cwd string. Do not modify it.
 * =============================================================================
 */
const char *dir_getcwd(void);

/* =============================================================================
 * dir_mkdir() -- create a new directory
 *
 * Parameter:
 *   name -- directory name (e.g. "home"). The new directory path will be
 *           constructed as cwd/name (e.g. "/home" when cwd is "/").
 *           Absolute paths beginning with '/' are used as-is.
 * =============================================================================
 */
void dir_mkdir(const char *name);

/* =============================================================================
 * dir_cd() -- change the current working directory
 *
 * Parameter:
 *   target -- directory to switch to. Supports:
 *             "/"     -- go to root
 *             ".."    -- go to parent directory
 *             "name"  -- go to a child named 'name' relative to cwd
 *             "/path" -- absolute path
 * =============================================================================
 */
void dir_cd(const char *target);

/* =============================================================================
 * dir_rmdir() -- remove an empty directory
 *
 * Parameter:
 *   name -- directory name (resolved the same way as dir_mkdir).
 *           The root directory "/" cannot be removed.
 *           The directory must be empty (no files) before it can be removed.
 * =============================================================================
 */
void dir_rmdir(const char *name);

/* =============================================================================
 * dir_ls() -- list all files and immediate subdirectories in the cwd
 *
 * Calls fs_list(cwd) from fs.h to list files, and scans the directory table
 * for immediate child directories to list those as well.
 * =============================================================================
 */
void dir_ls(void);

#endif /* DUGOS_DIR_H */
