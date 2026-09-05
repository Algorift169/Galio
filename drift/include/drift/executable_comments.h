/* SPDX-License-Identifier: AGPL-3.0-only */
/* Executable-comment helpers extract embedded Drift source while preserving source order. */

#ifndef DRIFT_EXECUTABLE_COMMENTS_H
#define DRIFT_EXECUTABLE_COMMENTS_H

/* Extract executable code from block comments using @exc and .exc markers.
 * 
 * Format:
 *   Block comment with documentation
 *   
 *   @exc
 *       say "Hello"
 *       var x = 10
 *       say x
 *   .exc
 *   
 *   More documentation.
 * 
 * Returns: dynamically allocated string containing concatenated executable code.
 *          Must be freed by caller.
 *          Returns empty string if no @exc blocks found.
 */
char *extract_executable_from_exc_blocks(const char *source);

#endif
