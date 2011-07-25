#ifndef __TORRENT_SANITIZE_CONFIG_H
#define __TORRENT_SANITIZE_CONFIG_H


/* using MMAP is dangerous if files are not updated atomically;
 * atomic file update works like this:
 *   write to "<filename>.tmp$$"
 *   mv "<filename>.tmp$$" -> "<filename>"
 *   (the tmp file should be on the same filesystem)
 * MMAP is only used to read files - it delays reading the actual content until
 *   the code accesses the memory
 */
#define USE_MMAP

#endif
