
/*

  UTEPFS: a TCP/IP based distributed filesystem

  UTEPFS is

  Copyright 2018-21 by

  University of Alaska Anchorage, College of Engineering.

  and

  Copyright 2025 by

  University of Texas at El Paso

  Contributor: Christoph Lauter

  and based on

  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -g -O0 -Wall utepfs.c implementation.c `pkg-config fuse --cflags --libs` -o utepfs

*/

#include <stddef.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <arpa/inet.h>

/* The filesystem you implement must support all the 14 operations
   stubbed out below. There need not be support for access rights,
   links, symbolic links. There needs to be support for access and
   modification times and information for statfs.

   You must implement a client component for a TCP/IP-based
   distributed file system. In the 14 operations below, you receive a
   file desciptor fd, over which you can communicate with a server.
   The protocol is described in the homework assignment's text part.

   A short description on how to communicate with the server is
   given in the comment for each function as well.

   CAUTION:

   * You MUST NOT use any global variables in your program for reasons
   due to the way FUSE is designed.

   * You may use any function out of libc for your filesystem,
   including (but not limited to) malloc, calloc, free, strdup,
   strlen, strncpy, strchr, strrchr, memset, memcpy. However, your
   filesystem MUST NOT depend on local memory, as this is a
   distributed file system and the other users should see the
   modifications performed by your client. As a matter of course, your
   FUSE process, which implements the filesystem, MUST NOT leak
   memory: be careful in particular not to leak tiny amounts of memory
   that accumulate over time. In a working setup, a FUSE process is
   supposed to run for a long time!

   It is possible to check for memory leaks by running the FUSE
   process inside valgrind:

   valgrind --leak-check=full ./utepfs --server=server --port=port ./fuse-mnt/ -f

   However, the analysis of the leak indications displayed by valgrind
   is difficult as libfuse contains some small memory leaks (which do
   not accumulate over time). We cannot (easily) fix these memory
   leaks inside libfuse.

   * Avoid putting debug messages into the code. You may use fprintf
   for debugging purposes but they should all go away in the final
   version of the code. Using gdb is more professional, though.

   * You MUST NOT fail with exit(1) in case of an error. All the
   functions you have to implement have ways to indicated failure
   cases. Use these, mapping your internal errors intelligently onto
   the POSIX error conditions.

   * And of course: your code MUST NOT SEGFAULT!

   */

/* Helper types and functions */

/* Put your helper types and helper functions here */

#define MAX_PACKET_SIZE 65535

/**
 * Helper funciton to read exactly n bytes form a FD
 * This handels partial reads
 * @return 0 on success, -1 on failure
 */
static int read_n_bytes (int fd, void *buffer, size_t n){
   size_t total_read = 0;
   ssize_t bytes_read;
   unsigned char *buf = (unsigned char *)buffer;

   while (total_read < n){
      bytes_read = read(fd, buf + total_read, n - total_read);
      if (bytes_read <= 0){
         return -1;
      }
      total_read += bytes_read;
   }
   return 0;
}
/**
 * Helper function to write exactly n bytes to a file descriptor. 
 * @return 0 on scuess, -1 on faillure
 */
static int write_n_bytes (int fd, const void *buffer, size_t n){
   size_t total_written = 0;
   ssize_t bytes_written;
   const unsigned char *buf = (const unsigned char *)buffer;
   while (total_written < n){
      bytes_written = write (fd, buf + total_written, n - total_written);
      if (bytes_written <= 0){
         return -1;
      }
      total_written += bytes_written;
   }
   return 0;
}

/**
 * @brief generate a random 16-bit request ID
 */
static uint16_t generate_request_id(void){
   return (uint16_t)random();
}

/**
 * @brief MJap server error codes to POSIX errno values
 */
static int map_error_code(uint8_t error_code) {
  switch (error_code) {
    case 0x00: return EACCES;
    case 0x01: return EBADF;
    case 0x02: return EFAULT;
    case 0x03: return ELOOP;
    case 0x04: return ENAMETOOLONG;
    case 0x05: return ENOENT;
    case 0x06: return ENOMEM;
    case 0x07: return ENOTDIR;
    case 0x08: return EOVERFLOW;
    case 0x09: return EINVAL;
    case 0x0a: return EDQUOT;
    case 0x0b: return EEXIST;
    case 0x0c: return ENOSPC;
    case 0x0d: return EPERM;
    case 0x0e: return EROFS;
    case 0x0f: return EBUSY;
    case 0x10: return EIO;
    case 0x11: return EISDIR;
    case 0x12: return ENOTEMPTY;
    case 0x13: return EMLINK;
    case 0x14: return EXDEV;
    case 0x15: return EFBIG;
    case 0x16: return EINTR;
    case 0x17: return ETXTBSY;
    case 0x18: return EMFILE;
    case 0x19: return ENFILE;
    case 0x1a: return ENODEV;
    case 0x1b: return ENXIO;
    case 0x1c: return EOPNOTSUPP;
    case 0x1d: return EWOULDBLOCK;
    case 0x1e: return EAGAIN;
    case 0x1f: return EDESTADDRREQ;
    case 0x20: return EPIPE;
    case 0x21: return ESRCH;
    case 0x22: return ENOSYS;
    default: return EIO; /* Default to I/O error for unknown codes */
  }
}

/**
 * Send a requst to the server with the giver request number and payload.
 * @return 0 on success, -1 on failure.
 */
static int send_request(int fd, uint16_t request_id, uint8_t request_num, const void *payload, size_t payload_size){

   uint16_t total_length = 5 + payload_size;
   unsigned char buffer[MAX_PACKET_SIZE];

   if (total_length > MAX_PACKET_SIZE){
      return -1;
   }

   uint16_t length_net = htons(total_length);
   uint16_t id_net = htons(request_id);

   memcpy(buffer, &length_net, 2);
   memcpy(buffer + 2, &id_net, 2);
   buffer[4] = request_num;

   if (payload_size > 0 && payload != NULL){
      memcpy(buffer + 5, payload, payload_size);
   }
   return write_n_bytes(fd, buffer, total_length);

}

/**
 * @brief receive and validate response header from server.
 * @return 0 on success, -1 on failure.
 */
static int receive_response_header(int fd, uint16_t expected_id, uint8_t expected_num, uint8_t *success_code){
   unsigned char header[6];

   if (read_n_bytes(fd, header, 6) != 0){
      return -1;
   }
}

/* End of helper functions */

/* Implements an emulation of the stat system call on the filesystem
   available through the file descriptor fd.

   If path can be followed and describes a file or directory
   that exists and is accessable, the access information is
   put into stbuf.

   On success, 0 is returned. On failure, -1 is returned and
   the appropriate error code is put into *errnoptr.

   man 2 stat documents all possible error codes and gives more detail
   on what fields of stbuf need to be filled in. Essentially, only the
   following fields need to be supported:

   st_uid      the value passed in argument
   st_gid      the value passed in argument
   st_mode     as provided by the server
   st_nlink    as provided by the server
   st_size     as provided by the server
   st_atim     as provided by the server
   st_mtim     as provided by the server

   Request to server:

   [unsigned 16 bit length in network byte order]
   [unsigned 16 bit request id in network byte order]
   [unsigned 8 bit request number = 0x01]
   [unsigned 16 bit length of path bytes in network byte order]
   [path bytes without '\0' at the end]

   Answer from server:

   [unsigned 16 bit length in network byte order]
   [unsigned 16 bit request id in network byte order]
   [unsigned 8 bit request number = 0x01]
   [unsigned 8 bit success code 0x00 => success, 0xff failure]

   followed by

   failure:

   [unsigned 8 bit error code: 0x00 EACCES
                               0x01 EBADF
                               0x02 EFAULT
                               0x03 ELOOP
                               0x04 ENAMETOOLONG
                               0x05 ENOENT
                               0x06 ENOMEM
                               0x07 ENOTDIR
                               0x08 EOVERFLOW
                               0x09 EINVAL          ]

   success:

   [unsigned 32 bit mode in network byte order]
   [unsigned 32 bit nlink in network byte order]
   [unsigned 32 bit size in network byte order]
   [unsigned 32 bit atime sec in network byte order]
   [unsigned 32 bit atime nsec in network byte order]
   [unsigned 32 bit mtime sec in network byte order]
   [unsigned 32 bit mtime nsec in network byte order]

*/
int __utepfs_getattr_implem(int fd, int *errnoptr,
                            uid_t uid, gid_t gid,
                            const char *path, struct stat *stbuf) {
  return -1; /* STUB, TODO */
}

/* Implements an emulation of the readdir system call on the filesystem
   available through the file descriptor fd.

   If path can be followed and describes a directory that exists and
   is accessable, the names of the subdirectories and files
   contained in that directory are output into *namesptr. The . and ..
   directories must not be included in that listing.

   If it needs to output file and subdirectory names, the function
   starts by allocating (with calloc) an array of pointers to
   characters of the right size (n entries for n names). Sets
   *namesptr to that pointer. It then goes over all entries
   in that array and allocates, for each of them an array of
   characters of the right size (to hold the i-th name, together
   with the appropriate '\0' terminator). It puts the pointer
   into that i-th array entry and fills the allocated array
   of characters with the appropriate name. The calling function
   will call free on each of the entries of *namesptr and
   on *namesptr.

   The function returns the number of names that have been
   put into namesptr.

   If no name needs to be reported because the directory does
   not contain any file or subdirectory besides . and .., 0 is
   returned and no allocation takes place.

   On failure, -1 is returned and the *errnoptr is set to
   the appropriate error code.

   The error codes are documented in man 2 readdir.

   In the case memory allocation with malloc/calloc fails, failure is
   indicated by returning -1 and setting *errnoptr to EINVAL.

   Request to server:

   [unsigned 16 bit length in network byte order]
   [unsigned 16 bit request id in network byte order]
   [8 bit request number = 0x02]
   [unsigned 16 bit length of path bytes in network byte order]
   [path bytes without '\0' at the end]
   [unsigned 32 bit number of first result in this chunk]

   Answer from server:

   [unsigned 16 bit length in network byte order]
   [unsigned 16 bit request id in network byte order]
   [unsigned 8 bit request number = 0x02]
   [unsigned 8 bit success code 0x00 => success, 0xff failure]

   followed by

   failure:

   [unsigned 8 bit error code: 0x00 EACCES
                               0x01 EBADF
                               0x05 ENOENT
                               0x06 ENOMEM
                               0x07 ENOTDIR
                               0x18 EMFILE
                               0x19 ENFILE      ]

   success:

   [unsigned 32 bit number of elements total in network byte order]
   [unsigned 32 bit number of elements reported in this chunk in network byte order]
   [unsigned 16 bit length of first element in network byte order]
   [bytes of first element without '\0' at the end]
   [unsigned 16 bit length of second element in network byte order]
   [bytes of second element without '\0' at the end]
   ...

   Remark: This function is hindered by the small static size of
   requests and answer in the TCP/IP-based UTEPFS protocol. The server
   hence starts its answers with two 32 bit numbers: the first one is
   the total number of elements to be reported (file and directory
   names). The second one is the number of elements reported in this
   answer for this request. The server then puts a maximum number of
   elements it can report in its answer. In order to get the
   additional elements, the client needs to repeat the request, by
   setting the 32bit number of the first element to report to the
   number of elements it got so far (starting with zero for the very
   first request). The server then reports additional elements.  This
   way, with repeated requests and answers, the client can get all
   elements from the server.

*/
int __utepfs_readdir_implem(int fd, int *errnoptr,
                            const char *path, char ***namesptr) {
  return -1; /* STUB, TODO */
}

/* Implements an emulation of the mknod system call for regular files
   available through the file descriptor fd.

   This function is called only for the creation of regular files.

   If a file gets created, it is of size zero and has default
   ownership and mode bits.

   The call creates the file indicated by path.

   On success, 0 is returned.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The error codes are documented in man 2 mknod.

   Request to server:

   [unsigned 16 bit length in network byte order]
   [unsigned 16 bit request id in network byte order]
   [unsigned 8 bit request number = 0x03]
   [unsigned 16 bit length of path bytes in network byte order]
   [path bytes without '\0' at the end]

   Answer from server:

   [unsigned 16 bit length in network byte order]
   [unsigned 16 bit request id in network byte order]
   [unsigned 8 bit request number = 0x03]
   [unsigned 8 bit success code 0x00 => success, 0xff failure]

   followed by

   failure:

   [unsigned 8 bit error code: 0x00 EACCES
                               0x01 EBADF
                               0x02 EFAULT
                               0x03 ELOOP
                               0x04 ENAMETOOLONG
                               0x05 ENOENT
                               0x06 ENOMEM
                               0x07 ENOTDIR
                               0x09 EINVAL
                               0x0a EDQUOT
                               0x0b EEXIST
                               0x0c ENOSPC
                               0x0d EPERM
                               0x0e EROFS          ]

   success:

   <nothing>

*/
int __utepfs_mknod_implem(int fd, int *errnoptr,
                          const char *path) {
  return -1; /* STUB, TODO */
}

/* Implements an emulation of the unlink system call for regular files
   available through the file descriptor fd.

   This function is called only for the deletion of regular files.

   On success, 0 is returned.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The error codes are documented in man 2 unlink.

   Request to server:

   [unsigned 16 bit length in network byte order]
   [unsigned 16 bit request id in network byte order]
   [unsigned 8 bit request number = 0x04]
   [unsigned 16 bit length of path bytes in network byte order]
   [path bytes without '\0' at the end]

   Answer from server:

   [unsigned 16 bit length in network byte order]
   [unsigned 16 bit request id in network byte order]
   [unsigned 8 bit request number = 0x04]
   [unsigned 8 bit success code 0x00 => success, 0xff failure]

   followed by

   failure:

   [unsigned 8 bit error code: 0x00 EACCES
                               0x01 EBADF
                               0x02 EFAULT
                               0x03 ELOOP
                               0x04 ENAMETOOLONG
                               0x05 ENOENT
                               0x06 ENOMEM
                               0x07 ENOTDIR
                               0x09 EINVAL
                               0x0d EPERM
                               0x0e EROFS
                               0x0f EBUSY
                               0x10 EIO
                               0x11 EISDIR         ]

   success:

   <nothing>

*/
int __utepfs_unlink_implem(int fd, int *errnoptr,
                           const char *path) {
  return -1; /* STUB, TODO */
}

/* Implements an emulation of the rmdir system call on the filesystem
   available through the file descriptor fd.

   The call deletes the directory indicated by path.

   On success, 0 is returned.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The function call must fail when the directory indicated by path is
   not empty (if there are files or subdirectories other than . and ..).

   The error codes are documented in man 2 rmdir.

   Request to server:

   [unsigned 16 bit length in network byte order]
   [unsigned 16 bit request id in network byte order]
   [unsigned 8 bit request number = 0x05]
   [unsigned 16 bit length of path bytes in network byte order]
   [path bytes without '\0' in the end]

   Answer from server:

   [unsigned 16 bit length in network byte order]
   [unsigned 16 bit request id in network byte order]
   [unsigned 8 bit request number = 0x05]
   [unsigned 8 bit success code 0x00 => success, 0xff failure]

   followed by

   failure:

   [unsigned 8 bit error code: 0x00 EACCES
                               0x02 EFAULT
                               0x03 ELOOP
                               0x04 ENAMETOOLONG
                               0x05 ENOENT
                               0x06 ENOMEM
                               0x07 ENOTDIR
                               0x09 EINVAL
                               0x0d EPERM
                               0x0e EROFS
                               0x0f EBUSY
                               0x12 ENOTEMPTY     ]

   success:

   <nothing>

*/
int __utepfs_rmdir_implem(int fd, int *errnoptr,
                          const char *path) {
  return -1; /* STUB, TODO */
}

/* Implements an emulation of the mkdir system call on the filesystem
   available through the file descriptor fd.

   The call creates the directory indicated by path.

   On success, 0 is returned.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The error codes are documented in man 2 mkdir.

   Request to server:

   [unsigned 16 bit length in network byte order]
   [unsigned 16 bit request id in network byte order]
   [unsigned 8 bit request number = 0x06]
   [unsigned 16 bit length of path bytes in network byte order]
   [path bytes without '\0' in the end]

   Answer from server:

   [unsigned 16 bit length in network byte order]
   [unsigned 16 bit request id in network byte order]
   [unsigned 8 bit request number = 0x06]
   [unsigned 8 bit success code 0x00 => success, 0xff failure]

   followed by

   failure:

   [unsigned 8 bit error code: 0x00 EACCES
                               0x01 EBADF
                               0x02 EFAULT
                               0x03 ELOOP
                               0x04 ENAMETOOLONG
                               0x05 ENOENT
                               0x06 ENOMEM
                               0x07 ENOTDIR
                               0x09 EINVAL
                               0x0a EDQUOT
                               0x0b EEXIST
                               0x0c ENOSPC
                               0x0d EPERM
                               0x0e EROFS
                               0x13 EMLINK            ]

   success:

   <nothing>

*/
int __utepfs_mkdir_implem(int fd, int *errnoptr,
                          const char *path) {
  return -1; /* STUB, TODO */
}

/* Implements an emulation of the rename system call on the filesystem
   available through the file descriptor fd.

   The call moves the file or directory indicated by from to to.

   On success, 0 is returned.

   On failure, -1 is returned and *errnoptr is set appropriately.

   Caution: the function does more than what is hinted to by its name.
   In cases the from and to paths differ, the file is moved out of
   the from path and added to the to path.

   The error codes are documented in man 2 rename.

   Request to server:

   [unsigned 16 bit length in network byte order]
   [unsigned 16 bit request id in network byte order]
   [unsigned 8 bit request number = 0x07]
   [unsigned 16 bit length of from bytes in network byte order]
   [from bytes without '\0' in the end]
   [unsigned 16 bit length of to bytes in network byte order]
   [to bytes without '\0' in the end]

   Answer from server:

   [unsigned 16 bit length in network byte order]
   [unsigned 16 bit request id in network byte order]
   [unsigned 8 bit request number = 0x07]
   [unsigned 8 bit success code 0x00 => success, 0xff failure]

   followed by

   failure:

   [unsigned 8 bit error code: 0x00 EACCES
                               0x01 EBADF
                               0x02 EFAULT
                               0x03 ELOOP
                               0x04 ENAMETOOLONG
                               0x05 ENOENT
                               0x06 ENOMEM
                               0x07 ENOTDIR
                               0x09 EINVAL
                               0x0a EDQUOT
                               0x0b EEXIST
                               0x0c ENOSPC
                               0x0d EPERM
                               0x0e EROFS
                               0x0f EBUSY
                               0x11 EISDIR
                               0x12 ENOTEMPTY
                               0x13 EMLINK
                               0x14 EXDEV            ]

   success:

   <nothing>

*/
int __utepfs_rename_implem(int fd, int *errnoptr,
                           const char *from, const char *to) {
  return -1; /* STUB, TODO */
}

/* Implements an emulation of the truncate system call on the filesystem
   available through the file descriptor fd.

   The call changes the size of the file indicated by path to offset
   bytes.

   When the file becomes smaller due to the call, the extending bytes are
   removed. When it becomes larger, zeros are appended.

   On success, 0 is returned.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The error codes are documented in man 2 truncate.

   Request to server:

   [unsigned 16 bit length in network byte order]
   [unsigned 16 bit request id in network byte order]
   [unsigned 8 bit request number = 0x08]
   [unsigned 16 bit length of path bytes in network byte order]
   [path bytes without '\0' in the end]
   [unsigned 32 bit offset in network byte order]

   Answer from server:

   [unsigned 16 bit length in network byte order]
   [unsigned 16 bit request id in network byte order]
   [unsigned 8 bit request number = 0x08]
   [unsigned 8 bit success code 0x00 => success, 0xff failure]

   followed by

   failure:

   [unsigned 8 bit error code: 0x00 EACCES
                               0x01 EBADF
                               0x02 EFAULT
                               0x03 ELOOP
                               0x04 ENAMETOOLONG
                               0x05 ENOENT
                               0x07 ENOTDIR
                               0x09 EINVAL
                               0x0d EPERM
                               0x0e EROFS
                               0x10 EIO
                               0x11 EISDIR
                               0x15 EFBIG
                               0x16 EINTR
                               0x17 ETXTBSY          ]

   success:

   <nothing>

*/
int __utepfs_truncate_implem(int fd, int *errnoptr,
                             const char *path, off_t offset) {
  return -1; /* STUB, TODO */
}

/* Implements an emulation of the open system call on the filesystem
   available through the file descriptor fd, without actually
   performing the opening of the file (no file descriptor is
   returned).

   The call just checks if the file (or directory) indicated by path
   can be accessed, i.e. if the path can be followed to an existing
   object for which the access rights are granted.

   On success, 0 is returned.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The error codes are documented in man 2 open.

   Request to server:

   [unsigned 16 bit length in network byte order]
   [unsigned 16 bit request id in network byte order]
   [unsigned 8 bit request number = 0x09]
   [unsigned 16 bit length of path bytes in network byte order]
   [path bytes without '\0' in the end]

   Answer from server:

   [unsigned 16 bit length in network byte order]
   [unsigned 16 bit request id in network byte order]
   [unsigned 8 bit request number = 0x09]
   [unsigned 8 bit success code 0x00 => success, 0xff failure]

   followed by

   failure:

   [unsigned 8 bit error code: 0x00 EACCES
                               0x01 EBADF
                               0x02 EFAULT
                               0x03 ELOOP
                               0x04 ENAMETOOLONG
                               0x05 ENOENT
                               0x06 ENOMEM
                               0x07 ENOTDIR
                               0x08 EOVERFLOW
                               0x09 EINVAL
                               0x0a EDQUOT
                               0x0b EEXIST
                               0x0c ENOSPC
                               0x0d EPERM
                               0x0e EROFS
                               0x0f EBUSY
                               0x11 EISDIR
                               0x15 EFBIG
                               0x16 EINTR
                               0x17 ETXTBSY
                               0x18 EMFILE
                               0x19 ENFILE
                               0x1a ENODEV
                               0x1b ENXIO
                               0x1c EOPNOTSUPP
                               0x1d EWOULDBLOCK        ]

   success:

   <nothing>

*/
int __utepfs_open_implem(int fd, int *errnoptr,
                         const char *path) {
  return -1; /* STUB, TODO */
}

/* Implements an emulation of the read system call on the filesystem
   available through the file descriptor fd.

   The call copies up to size bytes from the file indicated by
   path into the buffer, starting to read at offset. See the man page
   for read for the details when offset is beyond the end of the file etc.

   On success, the appropriate number of bytes read into the buffer is
   returned. The value zero is returned on an end-of-file condition.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The error codes are documented in man 2 read.

   Request to server:

   [unsigned 16 bit length in network byte order]
   [unsigned 16 bit request id in network byte order]
   [unsigned 8 bit request number = 0x0a]
   [unsigned 16 bit length of path bytes in network byte order]
   [path bytes without '\0' in the end]
   [unsigned 32 bit size in network byte order]
   [unsigned 32 bit offset in network byte order]

   Answer from server:

   [unsigned 16 bit length in network byte order]
   [unsigned 16 bit request id in network byte order]
   [unsigned 8 bit request number = 0x0a]
   [unsigned 8 bit success code 0x00 => success, 0xff failure]

   followed by

   failure:

   [unsigned 8 bit error code: 0x01 EBADF
                               0x02 EFAULT
                               0x09 EINVAL
                               0x10 EIO
                               0x11 EISDIR
                               0x16 EINTR
                               0x1d EWOULDBLOCK
                               0x1e EAGAIN           ]

   success:

   [unsigned 16 bit length of data in network byte order]
   [data bytes]

*/
int __utepfs_read_implem(int fd, int *errnoptr,
                         const char *path, char *buf, size_t size, off_t offset) {
  return -1; /* STUB, TODO */
}

/* Implements an emulation of the write system call on the filesystem
   available through the file descriptor fd.

   The call copies up to size bytes to the file indicated by
   path into the buffer, starting to write at offset. See the man page
   for write for the details when offset is beyond the end of the file etc.

   On success, the appropriate number of bytes written into the file is
   returned. The value zero is returned on an end-of-file condition.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The error codes are documented in man 2 write.

   Request to server:

   [unsigned 16 bit length in network byte order]
   [unsigned 16 bit request id in network byte order]
   [unsigned 8 bit request number = 0x0b]
   [unsigned 16 bit length of path bytes in network byte order]
   [path bytes without '\0' in the end]
   [unsigned 32 bit size in network byte order]
   [unsigned 32 bit offset in network byte order]
   [data bytes]

   Remark:

   The client must ensure that the request is never longer than the
   maximum request length of 2^16 - 1 bytes, even if the requested
   number of bytes to be written to the file (size) is stored on a 32
   bit word that gets sent to the server in the request.

   Answer from server:

   [unsigned 16 bit length in network byte order]
   [unsigned 16 bit request id in network byte order]
   [unsigned 8 bit request number = 0x0b]
   [unsigned 8 bit success code 0x00 => success, 0xff failure]

   followed by

   failure:

   [unsigned 8 bit error code: 0x01 EBADF
                               0x02 EFAULT
                               0x09 EINVAL
                               0x0a EDQUOT
                               0x0c ENOSPC
                               0x0d EPERM
                               0x10 EIO
                               0x15 EFBIG
                               0x16 EINTR
                               0x1d EWOULDBLOCK
                               0x1e EAGAIN
                               0x1f EDESTADDRREQ
                               0x20 EPIPE            ]

   success:

   [unsigned 16 bit length of data written in network byte order]

*/
int __utepfs_write_implem(int fd, int *errnoptr,
                          const char *path, const char *buf, size_t size, off_t offset) {
  return -1; /* STUB, TODO */
}

/* Implements an emulation of the utimensat system call on the filesystem
   available through the file descriptor fd.

   The call changes the access and modification times of the file
   or directory indicated by path to the values in ts.

   On success, 0 is returned.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The error codes are documented in man 2 utimensat.

   Request to server:

   [unsigned 16 bit length in network byte order]
   [unsigned 16 bit request id in network byte order]
   [unsigned 8 bit request number = 0x0c]
   [unsigned 16 bit length of path bytes in network byte order]
   [path bytes without '\0' in the end]
   [unsigned 32 bit entry at index 0 sec in network byte order]
   [unsigned 32 bit entry at index 0 nsec in network byte order]
   [unsigned 32 bit entry at index 1 sec in network byte order]
   [unsigned 32 bit entry at index 1 nsec in network byte order]

   Answer from server:

   [unsigned 16 bit length in network byte order]
   [unsigned 16 bit request id in network byte order]
   [unsigned 8 bit request number = 0x0c]
   [unsigned 8 bit success code 0x00 => success, 0xff failure]

   followed by

   failure:

   [unsigned 8 bit error code: 0x00 EACCES
                               0x01 EBADF
                               0x02 EFAULT
                               0x03 ELOOP
                               0x04 ENAMETOOLONG
                               0x05 ENOENT
                               0x07 ENOTDIR
                               0x09 EINVAL
                               0x0d EPERM
                               0x0e EROFS
                               0x21 ESRCH            ]

   success:

   <nothing>

*/
int __utepfs_utimens_implem(int fd, int *errnoptr,
                            const char *path, const struct timespec ts[2]) {
  return -1; /* STUB, TODO */
}

/* Implements an emulation of the statfs system call on the filesystem
   available through the file descriptor fd.

   The call gets information of the filesystem usage and puts in
   into stbuf.

   On success, 0 is returned.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The error codes are documented in man 2 statfs.

   Essentially, only the following fields of struct statvfs need to be
   supported:

   f_bsize   as reported by the server
   f_blocks  as reported by the server
   f_bfree   as reported by the server
   f_bavail  as reported by the server
   f_namemax as reported by the server

   Request to server:

   [unsigned 16 bit length in network byte order]
   [unsigned 16 bit request id in network byte order]
   [unsigned 8 bit request number = 0x0d]

   Answer from server:

   [unsigned 16 bit length in network byte order]
   [unsigned 16 bit request id in network byte order]
   [unsigned 8 bit request number = 0x0d]
   [unsigned 8 bit success code 0x00 => success, 0xff failure]

   followed by

   failure:

   [unsigned 8 bit error code: 0x00 EACCES
                               0x01 EBADF
                               0x02 EFAULT
                               0x03 ELOOP
                               0x04 ENAMETOOLONG
                               0x05 ENOENT
                               0x06 ENOMEM
                               0x07 ENOTDIR
                               0x08 EOVERFLOW
                               0x10 EIO
                               0x16 EINTR
                               0x22 ENOSYS                ]

   success:

   [unsigned 32 bit block size in network byte order]
   [unsigned 32 bit blocks in network byte order]
   [unsigned 32 bit blocks free in network byte order]
   [unsigned 32 bit block available in network byte order]
   [unsigned 32 bit namemax in network byte order]

*/
int __utepfs_statfs_implem(int fd, int *errnoptr,
                           struct statvfs* stbuf) {
  return -1; /* STUB, TODO */
}

/* Implements an emulation of the fsync system call on the filesystem
   available through the file descriptor fd.

   On success, 0 is returned.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The error codes are documented in man 2 fsync.

   Request to server:

   [unsigned 16 bit length in network byte order]
   [unsigned 16 bit request id in network byte order]
   [unsigned 8 bit request number = 0x0e]

   Answer from server:

   [unsigned 16 bit length in network byte order]
   [unsigned 16 bit request id in network byte order]
   [unsigned 8 bit request number = 0x0e]
   [unsigned 8 bit success code 0x00 => success, 0xff failure]

   followed by

   failure:

   [unsigned 8 bit error code: 0x01 EBADF
                               0x09 EINVAL
                               0x0a EDQUOT
                               0x0c ENOSPC
                               0x0e EROFS
                               0x10 EIO        ]

   success:

   <nothing>

*/
int __utepfs_fsync_implem(int fd, int *errnoptr) {
  return -1; /* STUB, TODO */
}
