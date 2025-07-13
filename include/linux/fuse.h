/* SPDX-License-Identifier: ((GPL-2.0 WITH Linux-syscall-note) OR BSD-2-Clause) */
/*
    This file defines the kernel interface of FUSE
    Copyright (C) 2001-2008  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.

    This -- and only this -- header file may also be distributed under
    the terms of the BSD Licence as follows:

    Copyright (C) 2001-2007 Miklos Szeredi. All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
    OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.
*/

/*
 * This file defines the kernel interface of FUSE
 *
 * All communication happens through a single character device, /dev/fuse. The
 * userspace daemon reads requests from this device, processes them, and writes
 * replies back.
 *
 * The structures defined here represent the various requests (from kernel to
 * userspace) and replies (from userspace to kernel).
 *
 * Protocol changelog:
 *
 * 7.1:
 *  - add the following messages:
 *      FUSE_SETATTR, FUSE_SYMLINK, FUSE_MKNOD, FUSE_MKDIR, FUSE_UNLINK,
 *      FUSE_RMDIR, FUSE_RENAME, FUSE_LINK, FUSE_OPEN, FUSE_READ, FUSE_WRITE,
 *      FUSE_RELEASE, FUSE_FSYNC, FUSE_FLUSH, FUSE_SETXATTR, FUSE_GETXATTR,
 *      FUSE_LISTXATTR, FUSE_REMOVEXATTR, FUSE_OPENDIR, FUSE_READDIR,
 *      FUSE_RELEASEDIR
 *  - add padding to messages to accommodate 32-bit servers on 64-bit kernels
 *
 * 7.2:
 *  - add FOPEN_DIRECT_IO and FOPEN_KEEP_CACHE flags
 *  - add FUSE_FSYNCDIR message
 *
 * 7.3:
 *  - add FUSE_ACCESS message
 *  - add FUSE_CREATE message
 *  - add filehandle to fuse_setattr_in
 *
 * 7.4:
 *  - add frsize to fuse_kstatfs
 *  - clean up request size limit checking
 *
 * 7.5:
 *  - add flags and max_write to fuse_init_out
 *
 * 7.6:
 *  - add max_readahead to fuse_init_in and fuse_init_out
 *
 * 7.7:
 *  - add FUSE_INTERRUPT message
 *  - add POSIX file lock support
 *
 * 7.8:
 *  - add lock_owner and flags fields to fuse_release_in
 *  - add FUSE_BMAP message
 *  - add FUSE_DESTROY message
 *
 * 7.9:
 *  - new fuse_getattr_in input argument of GETATTR
 *  - add lk_flags in fuse_lk_in
 *  - add lock_owner field to fuse_setattr_in, fuse_read_in and fuse_write_in
 *  - add blksize field to fuse_attr
 *  - add file flags field to fuse_read_in and fuse_write_in
 *  - Add ATIME_NOW and MTIME_NOW flags to fuse_setattr_in
 *
 * 7.10
 *  - add nonseekable open flag
 *
 * 7.11
 *  - add IOCTL message
 *  - add unsolicited notification support
 *  - add POLL message and NOTIFY_POLL notification
 *
 * 7.12
 *  - add umask flag to input argument of create, mknod and mkdir
 *  - add notification messages for invalidation of inodes and
 *    directory entries
 *
 * 7.13
 *  - make max number of background requests and congestion threshold
 *    tunables
 *
 * 7.14
 *  - add splice support to fuse device
 *
 * 7.15
 *  - add store notify
 *  - add retrieve notify
 *
 * 7.16
 *  - add BATCH_FORGET request
 *  - FUSE_IOCTL_UNRESTRICTED shall now return with array of 'struct
 *    fuse_ioctl_iovec' instead of ambiguous 'struct iovec'
 *  - add FUSE_IOCTL_32BIT flag
 *
 * 7.17
 *  - add FUSE_FLOCK_LOCKS and FUSE_RELEASE_FLOCK_UNLOCK
 *
 * 7.18
 *  - add FUSE_IOCTL_DIR flag
 *  - add FUSE_NOTIFY_DELETE
 *
 * 7.19
 *  - add FUSE_FALLOCATE
 *
 * 7.20
 *  - add FUSE_AUTO_INVAL_DATA
 *
 * 7.21
 *  - add FUSE_READDIRPLUS
 *  - send the requested events in POLL request
 *
 * 7.22
 *  - add FUSE_ASYNC_DIO
 *
 * 7.23
 *  - add FUSE_WRITEBACK_CACHE
 *  - add time_gran to fuse_init_out
 *  - add reserved space to fuse_init_out
 *  - add FATTR_CTIME
 *  - add ctime and ctimensec to fuse_setattr_in
 *  - add FUSE_RENAME2 request
 *  - add FUSE_NO_OPEN_SUPPORT flag
 *
 *  7.24
 *  - add FUSE_LSEEK for SEEK_HOLE and SEEK_DATA support
 *
 *  7.25
 *  - add FUSE_PARALLEL_DIROPS
 *
 *  7.26
 *  - add FUSE_HANDLE_KILLPRIV
 *  - add FUSE_POSIX_ACL
 *
 *  7.27
 *  - add FUSE_ABORT_ERROR
 *
 *  7.28
 *  - add FUSE_COPY_FILE_RANGE
 *  - add FOPEN_CACHE_DIR
 *  - add FUSE_MAX_PAGES, add max_pages to init_out
 *  - add FUSE_CACHE_SYMLINKS
 *
 *  7.29
 *  - add FUSE_NO_OPENDIR_SUPPORT flag
 *
 *  7.30
 *  - add FUSE_EXPLICIT_INVAL_DATA
 *  - add FUSE_IOCTL_COMPAT_X32
 *
 *  7.31
 *  - add FUSE_WRITE_KILL_PRIV flag
 *  - add FUSE_SETUPMAPPING and FUSE_REMOVEMAPPING
 *  - add map_alignment to fuse_init_out, add FUSE_MAP_ALIGNMENT flag
 *
 *  7.32
 *  - add flags to fuse_attr, add FUSE_ATTR_SUBMOUNT, add FUSE_SUBMOUNTS
 *
 *  7.33
 *  - add FUSE_HANDLE_KILLPRIV_V2, FUSE_WRITE_KILL_SUIDGID, FATTR_KILL_SUIDGID
 *  - add FUSE_OPEN_KILL_SUIDGID
 *  - extend fuse_setxattr_in, add FUSE_SETXATTR_EXT
 *  - add FUSE_SETXATTR_ACL_KILL_SGID
 *
 *  7.34
 *  - add FUSE_SYNCFS
 *
 *  7.35
 *  - add FOPEN_NOFLUSH
 *
 *  7.36
 *  - extend fuse_init_in with reserved fields, add FUSE_INIT_EXT init flag
 *  - add flags2 to fuse_init_in and fuse_init_out
 *  - add FUSE_SECURITY_CTX init flag
 *  - add security context to create, mkdir, symlink, and mknod requests
 *  - add FUSE_HAS_INODE_DAX, FUSE_ATTR_DAX
 *
 *  7.37
 *  - add FUSE_TMPFILE
 *
 *  7.38
 *  - add FUSE_EXPIRE_ONLY flag to fuse_notify_inval_entry
 *  - add FOPEN_PARALLEL_DIRECT_WRITES
 *  - add total_extlen to fuse_in_header
 *  - add FUSE_MAX_NR_SECCTX
 *  - add extension header
 *  - add FUSE_EXT_GROUPS
 *  - add FUSE_CREATE_SUPP_GROUP
 *  - add FUSE_HAS_EXPIRE_ONLY
 *
 *  7.39
 *  - add FUSE_DIRECT_IO_ALLOW_MMAP
 *  - add FUSE_STATX and related structures
 *
 *  7.40
 *  - add max_stack_depth to fuse_init_out, add FUSE_PASSTHROUGH init flag
 *  - add backing_id to fuse_open_out, add FOPEN_PASSTHROUGH open flag
 *  - add FUSE_NO_EXPORT_SUPPORT init flag
 *  - add FUSE_NOTIFY_RESEND, add FUSE_HAS_RESEND init flag
 *
 *  7.41
 *  - add FUSE_ALLOW_IDMAP
 *  7.42
 *  - Add FUSE_OVER_IO_URING and all other io-uring related flags and data
 *    structures:
 *    - struct fuse_uring_ent_in_out
 *    - struct fuse_uring_req_header
 *    - struct fuse_uring_cmd_req
 *    - FUSE_URING_IN_OUT_HEADER_SZ
 *    - FUSE_URING_OP_IN_OUT_SZ
 *    - enum fuse_uring_cmd
 *
 *  7.43
 *  - add FUSE_REQUEST_TIMEOUT
 */

#ifndef _LINUX_FUSE_H
#define _LINUX_FUSE_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

/*
 * Version negotiation:
 *
 * Both the kernel and userspace send the version they support in the
 * INIT request and reply respectively.
 *
 * If the major versions match then both shall use the smallest
 * of the two minor versions for communication.
 *
 * If the kernel supports a larger major version, then userspace shall
 * reply with the major version it supports, ignore the rest of the
 * INIT message and expect a new INIT message from the kernel with a
 * matching major version.
 *
 * If the library supports a larger major version, then it shall fall
 * back to the major protocol version sent by the kernel for
 * communication and reply with that major version (and an arbitrary
 * supported minor version).
 */

/** Version number of this interface */
#define FUSE_KERNEL_VERSION 7

/** Minor version number of this interface */
#define FUSE_KERNEL_MINOR_VERSION 43

/** The node ID of the root inode */
#define FUSE_ROOT_ID 1

/* Make sure all structures are padded to 64bit boundary, so 32bit
   userspace works under 64bit kernels */

/**
 * struct fuse_attr - File attributes
 *
 * This structure is a subset of the kernel's `struct kstat` and is used to
 * pass file attribute information between the kernel and the FUSE daemon.
 *
 * @ino: Inode number
 * @size: Size of the file in bytes
 * @blocks: Number of 512-byte blocks allocated
 * @atime: Time of last access (seconds)
 * @mtime: Time of last modification (seconds)
 * @ctime: Time of last status change (seconds)
 * @atimensec: Nanosecond part of atime
 * @mtimensec: Nanosecond part of mtime
 * @ctimensec: Nanosecond part of ctime
 * @mode: File mode (permissions and type)
 * @nlink: Number of hard links
 * @uid: User ID of owner
 * @gid: Group ID of owner
 * @rdev: Device ID (if special file)
 * @blksize: Block size for filesystem I/O
 * @flags: File-specific flags
 */
struct fuse_attr {
	uint64_t	ino;
	uint64_t	size;
	uint64_t	blocks;
	uint64_t	atime;
	uint64_t	mtime;
	uint64_t	ctime;
	uint32_t	atimensec;
	uint32_t	mtimensec;
	uint32_t	ctimensec;
	uint32_t	mode;
	uint32_t	nlink;
	uint32_t	uid;
	uint32_t	gid;
	uint32_t	rdev;
	uint32_t	blksize;
	uint32_t	flags;
};

/*
 * The following structures are bit-for-bit compatible with the statx(2) ABI in
 * Linux.
 */

/**
 * struct fuse_sx_time - Timestamp for statx
 *
 * @tv_sec: Seconds since the Epoch
 * @tv_nsec: Nanoseconds
 * @__reserved: Reserved for future use
 */
struct fuse_sx_time {
	int64_t		tv_sec;
	uint32_t	tv_nsec;
	int32_t		__reserved;
};

/**
 * struct fuse_statx - Extended file attributes for FUSE_STATX
 *
 * This structure is compatible with the `struct statx` used by the statx(2)
 * system call, allowing for more detailed file information.
 *
 * @mask: Mask of fields requested by the caller
 * @blksize: Block size for filesystem I/O
 * @attributes: File attributes (e.g., STATX_ATTR_*)
 * @nlink: Number of hard links
 * @uid: User ID of owner
 * @gid: Group ID of owner
 * @mode: File mode (permissions and type)
 * @__spare0: Reserved space
 * @ino: Inode number
 * @size: Size of the file in bytes
 * @blocks: Number of 512-byte blocks allocated
 * @attributes_mask: Mask of supported attributes
 * @atime: Time of last access
 * @btime: Time of file creation (birth time)
 * @ctime: Time of last status change
 * @mtime: Time of last modification
 * @rdev_major: Major device ID for special files
 * @rdev_minor: Minor device ID for special files
 * @dev_major: Major device ID of the device containing the file
 * @dev_minor: Minor device ID of the device containing the file
 * @__spare2: Reserved space
 */
struct fuse_statx {
	uint32_t	mask;
	uint32_t	blksize;
	uint64_t	attributes;
	uint32_t	nlink;
	uint32_t	uid;
	uint32_t	gid;
	uint16_t	mode;
	uint16_t	__spare0[1];
	uint64_t	ino;
	uint64_t	size;
	uint64_t	blocks;
	uint64_t	attributes_mask;
	struct fuse_sx_time	atime;
	struct fuse_sx_time	btime;
	struct fuse_sx_time	ctime;
	struct fuse_sx_time	mtime;
	uint32_t	rdev_major;
	uint32_t	rdev_minor;
	uint32_t	dev_major;
	uint32_t	dev_minor;
	uint64_t	__spare2[14];
};

/**
 * struct fuse_kstatfs - Filesystem statistics
 *
 * Used to return filesystem statistics for FUSE_STATFS, similar to `statfs`.
 *
 * @blocks: Total data blocks in filesystem
 * @bfree: Free blocks in filesystem
 * @bavail: Free blocks available to non-superuser
 * @files: Total file nodes in filesystem
 * @ffree: Free file nodes in filesystem
 * @bsize: Filesystem block size
 * @namelen: Maximum length of filenames
 * @frsize: Fragment size
 * @padding: Padding for alignment
 * @spare: Reserved space
 */
struct fuse_kstatfs {
	uint64_t	blocks;
	uint64_t	bfree;
	uint64_t	bavail;
	uint64_t	files;
	uint64_t	ffree;
	uint32_t	bsize;
	uint32_t	namelen;
	uint32_t	frsize;
	uint32_t	padding;
	uint32_t	spare[6];
};

/**
 * struct fuse_file_lock - File lock information
 *
 * Used for file locking operations (FUSE_GETLK, FUSE_SETLK, FUSE_SETLKW).
 *
 * @start: Starting offset of the lock
 * @end: Ending offset of the lock
 * @type: Type of lock (F_RDLCK, F_WRLCK, F_UNLCK)
 * @pid: Process ID of the lock holder
 */
struct fuse_file_lock {
	uint64_t	start;
	uint64_t	end;
	uint32_t	type;
	uint32_t	pid; /* tgid */
};

/* Bitmasks for fuse_setattr_in.valid */
#define FATTR_MODE		(1 << 0) /* Set mode */
#define FATTR_UID		(1 << 1) /* Set user ID */
#define FATTR_GID		(1 << 2) /* Set group ID */
#define FATTR_SIZE		(1 << 3) /* Set size */
#define FATTR_ATIME		(1 << 4) /* Set access time */
#define FATTR_MTIME		(1 << 5) /* Set modification time */
#define FATTR_FH		(1 << 6) /* Set file handle */
#define FATTR_ATIME_NOW		(1 << 7) /* Set access time to now */
#define FATTR_MTIME_NOW		(1 << 8) /* Set modification time to now */
#define FATTR_LOCKOWNER		(1 << 9) /* Set lock owner */
#define FATTR_CTIME		(1 << 10) /* Set change time */
#define FATTR_KILL_SUIDGID	(1 << 11) /* Kill suid/sgid bits */

/* Flags returned by the OPEN request */
#define FOPEN_DIRECT_IO			(1 << 0) /* bypass page cache for this open file */
#define FOPEN_KEEP_CACHE		(1 << 1) /* don't invalidate the data cache on open */
#define FOPEN_NONSEEKABLE		(1 << 2) /* the file is not seekable */
#define FOPEN_CACHE_DIR			(1 << 3) /* allow caching this directory */
/* the file is stream-like (no file position at all) */
#define FOPEN_STREAM			(1 << 4)
/* don't flush data cache on close (unless FUSE_WRITEBACK_CACHE) */
#define FOPEN_NOFLUSH			(1 << 5)
/* Allow concurrent direct writes on the same inode */
#define FOPEN_PARALLEL_DIRECT_WRITES	(1 << 6)
/* passthrough read/write io for this open file */
#define FOPEN_PASSTHROUGH		(1 << 7)

/* INIT request/reply flags */
#define FUSE_ASYNC_READ			(1 << 0)  /* asynchronous read requests */
#define FUSE_POSIX_LOCKS		(1 << 1)  /* remote locking for POSIX file locks */
/* kernel sends file handle for fstat, etc... (not yet supported) */
#define FUSE_FILE_OPS			(1 << 2)
/* handles the O_TRUNC open flag in the filesystem */
#define FUSE_ATOMIC_O_TRUNC		(1 << 3)
/* filesystem handles lookups of "." and ".." */
#define FUSE_EXPORT_SUPPORT		(1 << 4)
/* filesystem can handle write size larger than 4kB */
#define FUSE_BIG_WRITES			(1 << 5)
/* don't apply umask to file mode on create operations */
#define FUSE_DONT_MASK			(1 << 6)
/* kernel supports splice write on the device */
#define FUSE_SPLICE_WRITE		(1 << 7)
/* kernel supports splice move on the device */
#define FUSE_SPLICE_MOVE		(1 << 8)
/* kernel supports splice read on the device */
#define FUSE_SPLICE_READ		(1 << 9)
#define FUSE_FLOCK_LOCKS		(1 << 10) /* remote locking for BSD style file locks */
#define FUSE_HAS_IOCTL_DIR		(1 << 11) /* kernel supports ioctl on directories */
#define FUSE_AUTO_INVAL_DATA		(1 << 12) /* automatically invalidate cached pages */
/* do READDIRPLUS (READDIR+LOOKUP in one) */
#define FUSE_DO_READDIRPLUS		(1 << 13)
#define FUSE_READDIRPLUS_AUTO		(1 << 14) /* adaptive readdirplus */
#define FUSE_ASYNC_DIO			(1 << 15) /* asynchronous direct I/O submission */
/* use writeback cache for buffered writes */
#define FUSE_WRITEBACK_CACHE		(1 << 16)
#define FUSE_NO_OPEN_SUPPORT		(1 << 17) /* kernel supports zero-message opens */
#define FUSE_PARALLEL_DIROPS		(1 << 18) /* allow parallel lookups and readdir */
/* fs handles killing suid/sgid/cap on write/chown/trunc */
#define FUSE_HANDLE_KILLPRIV		(1 << 19)
#define FUSE_POSIX_ACL			(1 << 20) /* filesystem supports posix acls */
/* reading the device after abort returns ECONNABORTED */
#define FUSE_ABORT_ERROR		(1 << 21)
/* init_out.max_pages contains the max number of req pages */
#define FUSE_MAX_PAGES			(1 << 22)
#define FUSE_CACHE_SYMLINKS		(1 << 23) /* cache READLINK responses */
/* kernel supports zero-message opendir */
#define FUSE_NO_OPENDIR_SUPPORT		(1 << 24)
/* only invalidate cached pages on explicit request */
#define FUSE_EXPLICIT_INVAL_DATA	(1 << 25)
/* init_out.map_alignment contains log2(byte alignment) */
#define FUSE_MAP_ALIGNMENT		(1 << 26)
/* kernel supports auto-mounting directory submounts */
#define FUSE_SUBMOUNTS			(1 << 27)
/* fs handles killing suid/sgid/cap, v2 */
#define FUSE_HANDLE_KILLPRIV_V2		(1 << 28)
/* Server supports extended struct fuse_setxattr_in */
#define FUSE_SETXATTR_EXT		(1 << 29)
#define FUSE_INIT_EXT			(1 << 30) /* extended fuse_init_in request */
#define FUSE_INIT_RESERVED		(1U << 31) /* reserved, do not use */
/* bits 32..63 get shifted down 32 bits into the flags2 field */
/* add security context to create, mkdir, symlink, and mknod */
#define FUSE_SECURITY_CTX		(1ULL << 32)
#define FUSE_HAS_INODE_DAX		(1ULL << 33) /* use per inode DAX */
/* add supplementary group info to create, mkdir, symlink and mknod */
#define FUSE_CREATE_SUPP_GROUP		(1ULL << 34)
/* kernel supports expiry-only entry invalidation */
#define FUSE_HAS_EXPIRE_ONLY		(1ULL << 35)
/* allow shared mmap in FOPEN_DIRECT_IO mode. */
#define FUSE_DIRECT_IO_ALLOW_MMAP	(1ULL << 36)
#define FUSE_PASSTHROUGH		(1ULL << 37) /* passthrough mode */
/* explicitly disable export support */
#define FUSE_NO_EXPORT_SUPPORT		(1ULL << 38)
/* kernel supports resending pending requests */
#define FUSE_HAS_RESEND			(1ULL << 39)
/* Obsolete alias for FUSE_DIRECT_IO_ALLOW_MMAP */
#define FUSE_DIRECT_IO_RELAX		FUSE_DIRECT_IO_ALLOW_MMAP
#define FUSE_ALLOW_IDMAP		(1ULL << 40) /* allow creation of idmapped mounts */
/* Indicate that client supports io-uring */
#define FUSE_OVER_IO_URING		(1ULL << 41)
/* kernel supports timing out requests */
#define FUSE_REQUEST_TIMEOUT		(1ULL << 42)

/* CUSE INIT request/reply flags */
#define CUSE_UNRESTRICTED_IOCTL	(1 << 0) /* use unrestricted ioctl */

/* Release flags */
#define FUSE_RELEASE_FLUSH		(1 << 0) /* Flush data */
#define FUSE_RELEASE_FLOCK_UNLOCK	(1 << 1) /* Release flock lock */

/* Getattr flags */
#define FUSE_GETATTR_FH		(1 << 0) /* Get attributes from file handle */

/* Lock flags */
#define FUSE_LK_FLOCK		(1 << 0) /* Use flock locks */

/* WRITE flags */
/* delayed write from page cache, file handle is guessed */
#define FUSE_WRITE_CACHE		(1 << 0)
#define FUSE_WRITE_LOCKOWNER		(1 << 1) /* lock_owner field is valid */
#define FUSE_WRITE_KILL_SUIDGID		(1 << 2) /* kill suid and sgid bits */

/* Obsolete alias; this flag implies killing suid/sgid only. */
#define FUSE_WRITE_KILL_PRIV	FUSE_WRITE_KILL_SUIDGID

/* Read flags */
#define FUSE_READ_LOCKOWNER	(1 << 1) /* lock_owner field is valid */

/* Ioctl flags */
#define FUSE_IOCTL_COMPAT		(1 << 0) /* 32bit compat ioctl on 64bit machine */
/* not restricted to well-formed ioctls, retry allowed */
#define FUSE_IOCTL_UNRESTRICTED		(1 << 1)
#define FUSE_IOCTL_RETRY		(1 << 2) /* retry with new iovecs */
#define FUSE_IOCTL_32BIT		(1 << 3) /* 32bit ioctl */
#define FUSE_IOCTL_DIR			(1 << 4) /* is a directory */
/* x32 compat ioctl on 64bit machine (64bit time_t) */
#define FUSE_IOCTL_COMPAT_X32		(1 << 5)

#define FUSE_IOCTL_MAX_IOV		256 /* maximum of in_iovecs + out_iovecs */

/* Poll flags */
#define FUSE_POLL_SCHEDULE_NOTIFY	(1 << 0) /* request poll notify */

/* Fsync flags */
#define FUSE_FSYNC_FDATASYNC		(1 << 0) /* Sync data only, not metadata */

/* fuse_attr flags */
#define FUSE_ATTR_SUBMOUNT		(1 << 0) /* Object is a submount root */
/* Enable DAX for this file in per inode DAX mode */
#define FUSE_ATTR_DAX			(1 << 1)

/* Open flags */
#define FUSE_OPEN_KILL_SUIDGID		(1 << 0) /* Kill suid and sgid if executable */

/* setxattr flags */
/* Clear SGID when system.posix_acl_access is set */
#define FUSE_SETXATTR_ACL_KILL_SGID	(1 << 0)

/* notify_inval_entry flags */
#define FUSE_EXPIRE_ONLY		(1 << 0) /* Expire only */

/**
 * enum fuse_ext_type - extension type
 *
 * @FUSE_MAX_NR_SECCTX: maximum value of &fuse_secctx_header.nr_secctx
 * @FUSE_EXT_GROUPS: &fuse_supp_groups extension
 */
enum fuse_ext_type {
	/* Types 0..31 are reserved for fuse_secctx_header */
	FUSE_MAX_NR_SECCTX	= 31,
	FUSE_EXT_GROUPS		= 32,
};

/**
 * enum fuse_opcode - FUSE operation codes (opcodes)
 *
 * Each request from the kernel to the userspace filesystem has a specific
 * opcode indicating the operation to be performed. These are sent in
 * struct fuse_in_header.
 *
 * @FUSE_LOOKUP: Look up a directory entry by name
 * @FUSE_FORGET: Kernel is "forgetting" about an inode. No reply is sent
 * @FUSE_GETATTR: Get file attributes (stat)
 * @FUSE_SETATTR: Set file attributes
 * @FUSE_READLINK: Read the target of a symbolic link
 * @FUSE_SYMLINK: Create a symbolic link
 * @FUSE_MKNOD: Create a special file (device file, FIFO)
 * @FUSE_MKDIR: Create a directory
 * @FUSE_UNLINK: Remove a file
 * @FUSE_RMDIR: Remove a directory
 * @FUSE_RENAME: Rename a file or directory
 * @FUSE_LINK: Create a hard link
 * @FUSE_OPEN: Open a file
 * @FUSE_READ: Read data from an open file
 * @FUSE_WRITE: Write data to an open file
 * @FUSE_STATFS: Get filesystem statistics
 * @FUSE_RELEASE: Release an open file
 * @FUSE_FSYNC: Synchronize file contents
 * @FUSE_SETXATTR: Set an extended attribute
 * @FUSE_GETXATTR: Get an extended attribute
 * @FUSE_LISTXATTR: List extended attributes
 * @FUSE_REMOVEXATTR: Remove an extended attribute
 * @FUSE_FLUSH: Flush cached data
 * @FUSE_INIT: Initialize the filesystem session
 * @FUSE_OPENDIR: Open a directory
 * @FUSE_READDIR: Read directory entries
 * @FUSE_RELEASEDIR: Release an open directory
 * @FUSE_FSYNCDIR: Synchronize directory contents
 * @FUSE_GETLK: Test for a file lock
 * @FUSE_SETLK: Set a file lock
 * @FUSE_SETLKW: Set a file lock and wait
 * @FUSE_ACCESS: Check file access permissions
 * @FUSE_CREATE: Create and open a file
 * @FUSE_INTERRUPT: Interrupt a pending request
 * @FUSE_BMAP: Map a block in a file
 * @FUSE_DESTROY: Clean up a filesystem instance
 * @FUSE_IOCTL: I/O control operation
 * @FUSE_POLL: Poll for I/O events
 * @FUSE_NOTIFY_REPLY: Reply to a notification
 * @FUSE_BATCH_FORGET: Batched version of FUSE_FORGET
 * @FUSE_FALLOCATE: Preallocate space for a file
 * @FUSE_READDIRPLUS: Read directory entries plus attributes
 * @FUSE_RENAME2: Extended version of rename
 * @FUSE_LSEEK: Reposition read/write file offset
 * @FUSE_COPY_FILE_RANGE: Copy a range of data from one file to another
 * @FUSE_SETUPMAPPING: (Internal) Setup a memory mapping for a file on a DAX device
 * @FUSE_REMOVEMAPPING: (Internal) Remove a memory mapping
 * @FUSE_SYNCFS: Synchronize filesystem
 * @FUSE_TMPFILE: Create a temporary file
 * @FUSE_STATX: Get extended file attributes (statx)
 * @CUSE_INIT: Initialize a CUSE session
 * @CUSE_INIT_BSWAP_RESERVED: Reserved opcode to detect endian-ness for CUSE
 * @FUSE_INIT_BSWAP_RESERVED: Reserved opcode to detect endian-ness for FUSE
 */
enum fuse_opcode {
	FUSE_LOOKUP		= 1,
	FUSE_FORGET		= 2,  /* no reply */
	FUSE_GETATTR		= 3,
	FUSE_SETATTR		= 4,
	FUSE_READLINK		= 5,
	FUSE_SYMLINK		= 6,
	FUSE_MKNOD		= 8,
	FUSE_MKDIR		= 9,
	FUSE_UNLINK		= 10,
	FUSE_RMDIR		= 11,
	FUSE_RENAME		= 12,
	FUSE_LINK		= 13,
	FUSE_OPEN		= 14,
	FUSE_READ		= 15,
	FUSE_WRITE		= 16,
	FUSE_STATFS		= 17,
	FUSE_RELEASE		= 18,
	FUSE_FSYNC		= 20,
	FUSE_SETXATTR		= 21,
	FUSE_GETXATTR		= 22,
	FUSE_LISTXATTR		= 23,
	FUSE_REMOVEXATTR	= 24,
	FUSE_FLUSH		= 25,
	FUSE_INIT		= 26,
	FUSE_OPENDIR		= 27,
	FUSE_READDIR		= 28,
	FUSE_RELEASEDIR		= 29,
	FUSE_FSYNCDIR		= 30,
	FUSE_GETLK		= 31,
	FUSE_SETLK		= 32,
	FUSE_SETLKW		= 33,
	FUSE_ACCESS		= 34,
	FUSE_CREATE		= 35,
	FUSE_INTERRUPT		= 36,
	FUSE_BMAP		= 37,
	FUSE_DESTROY		= 38,
	FUSE_IOCTL		= 39,
	FUSE_POLL		= 40,
	FUSE_NOTIFY_REPLY	= 41,
	FUSE_BATCH_FORGET	= 42,
	FUSE_FALLOCATE		= 43,
	FUSE_READDIRPLUS	= 44,
	FUSE_RENAME2		= 45,
	FUSE_LSEEK		= 46,
	FUSE_COPY_FILE_RANGE	= 47,
	FUSE_SETUPMAPPING	= 48,
	FUSE_REMOVEMAPPING	= 49,
	FUSE_SYNCFS		= 50,
	FUSE_TMPFILE		= 51,
	FUSE_STATX		= 52,

	/* CUSE specific operations */
	CUSE_INIT		= 4096,

	/* Reserved opcodes: helpful to detect structure endian-ness */
	CUSE_INIT_BSWAP_RESERVED	= 1048576,	/* CUSE_INIT << 8 */
	FUSE_INIT_BSWAP_RESERVED	= 436207616,	/* FUSE_INIT << 24 */
};

/**
 * enum fuse_notify_code - Notification codes for asynchronous notifications
 *
 * These codes specify the type of notification sent from the userspace
 * daemon to the kernel to invalidate caches or perform other actions.
 *
 * @FUSE_NOTIFY_POLL: Notify about a poll event
 * @FUSE_NOTIFY_INVAL_INODE: Invalidate inode data cache
 * @FUSE_NOTIFY_INVAL_ENTRY: Invalidate a directory entry (dentry) cache
 * @FUSE_NOTIFY_STORE: Notify the kernel to store data for a later retrieve
 * @FUSE_NOTIFY_RETRIEVE: Notify the kernel to retrieve data
 * @FUSE_NOTIFY_DELETE: Notify the kernel of a deletion
 * @FUSE_NOTIFY_RESEND: Resend a previous notification
 * @FUSE_NOTIFY_CODE_MAX: Marks the end of the valid notification codes
 */
enum fuse_notify_code {
	FUSE_NOTIFY_POLL   = 1,
	FUSE_NOTIFY_INVAL_INODE = 2,
	FUSE_NOTIFY_INVAL_ENTRY = 3,
	FUSE_NOTIFY_STORE = 4,
	FUSE_NOTIFY_RETRIEVE = 5,
	FUSE_NOTIFY_DELETE = 6,
	FUSE_NOTIFY_RESEND = 7,
	FUSE_NOTIFY_CODE_MAX,
};

/* The read buffer is required to be at least 8k, but may be much larger */
#define FUSE_MIN_READ_BUFFER 8192

/* Size of fuse_entry_out for 32-bit compatibility */
#define FUSE_COMPAT_ENTRY_OUT_SIZE 120

/**
 * struct fuse_entry_out - Output structure for LOOKUP, CREATE, MKNOD, etc.
 *
 * This structure is returned by the filesystem daemon to the kernel in response
 * to operations that create or find a directory entry. It contains information
 * about the inode and caching timeouts.
 *
 * @nodeid: Inode ID. This must be unique
 * @generation: Inode generation number. nodeid:gen must be unique for the fs's
 * lifetime
 * @entry_valid: Cache timeout for the name (in seconds)
 * @attr_valid: Cache timeout for the attributes (in seconds)
 * @entry_valid_nsec: Nanosecond part of the name cache timeout
 * @attr_valid_nsec: Nanosecond part of the attribute cache timeout
 * @attr: The attributes of the inode (mode, size, etc.)
 */
struct fuse_entry_out {
	uint64_t	nodeid;
	uint64_t	generation;
	uint64_t	entry_valid;
	uint64_t	attr_valid;
	uint32_t	entry_valid_nsec;
	uint32_t	attr_valid_nsec;
	struct fuse_attr attr;
};

/**
 * struct fuse_forget_in - Input structure for a single FUSE_FORGET operation
 *
 * Sent by the kernel to tell the filesystem that one or more lookup counts
 * for an inode have been dropped.
 *
 * @nlookup: Number of lookups to forget
 */
struct fuse_forget_in {
	uint64_t	nlookup;
};

/**
 * struct fuse_forget_one - A single entry in a FUSE_BATCH_FORGET request
 *
 * @nodeid: The inode ID to forget
 * @nlookup: The number of lookups to forget for this inode
 */
struct fuse_forget_one {
	uint64_t	nodeid;
	uint64_t	nlookup;
};

/**
 * struct fuse_batch_forget_in - Input for a FUSE_BATCH_FORGET operation
 *
 * This is the header for a batch forget request, which is followed by `count`
 * instances of `fuse_forget_one`.
 *
 * @count: Number of `fuse_forget_one` entries that follow
 * @dummy: Padding for alignment
 */
struct fuse_batch_forget_in {
	uint32_t	count;
	uint32_t	dummy;
};

/**
 * struct fuse_getattr_in - Input structure for FUSE_GETATTR
 *
 * @getattr_flags: Flags for the getattr operation
 * @dummy: Padding
 * @fh: Optional file handle, if FUSE_GETATTR_FH is set
 */
struct fuse_getattr_in {
	uint32_t	getattr_flags;
	uint32_t	dummy;
	uint64_t	fh;
};

/* Size of fuse_attr_out for 32-bit compatibility */
#define FUSE_COMPAT_ATTR_OUT_SIZE 96

/**
 * struct fuse_attr_out - Output structure for FUSE_GETATTR and FUSE_SETATTR
 *
 * Contains the retrieved or updated attributes of an inode.
 *
 * @attr_valid: Cache timeout for the attributes (in seconds)
 * @attr_valid_nsec: Nanosecond part of the attribute cache timeout
 * @dummy: Padding
 * @attr: The file attributes
 */
struct fuse_attr_out {
	uint64_t	attr_valid;
	uint32_t	attr_valid_nsec;
	uint32_t	dummy;
	struct fuse_attr attr;
};

/**
 * struct fuse_statx_in - Input structure for FUSE_STATX
 *
 * @getattr_flags: Flags, same as in fuse_getattr_in
 * @reserved: Reserved for future use
 * @fh: Optional file handle
 * @sx_flags: statx flags
 * @sx_mask: Mask of fields to retrieve in statx
 */
struct fuse_statx_in {
	uint32_t	getattr_flags;
	uint32_t	reserved;
	uint64_t	fh;
	uint32_t	sx_flags;
	uint32_t	sx_mask;
};

/**
 * struct fuse_statx_out - Output structure for FUSE_STATX
 *
 * @attr_valid: Cache timeout for the attributes (seconds)
 * @attr_valid_nsec: Nanosecond part of attr_valid
 * @flags: statx result flags
 * @spare: Reserved space
 * @stat: The extended attributes
 */
struct fuse_statx_out {
	uint64_t	attr_valid;
	uint32_t	attr_valid_nsec;
	uint32_t	flags;
	uint64_t	spare[2];
	struct fuse_statx stat;
};

/* Size of fuse_mknod_in for 32-bit compatibility */
#define FUSE_COMPAT_MKNOD_IN_SIZE 8

/**
 * struct fuse_mknod_in - Input structure for FUSE_MKNOD
 *
 * The filename follows this structure in the request buffer.
 * @mode: File mode (permissions and type)
 * @rdev: Device number for special files (major/minor)
 * @umask: Umask to apply to the mode
 * @padding: Padding for alignment
 */
struct fuse_mknod_in {
	uint32_t	mode;
	uint32_t	rdev;
	uint32_t	umask;
	uint32_t	padding;
};

/**
 * struct fuse_mkdir_in - Input structure for FUSE_MKDIR
 *
 * The directory name follows this structure
 *
 * @mode: Directory mode (permissions)
 * @umask: Umask to apply to the mode
 */
struct fuse_mkdir_in {
	uint32_t	mode;
	uint32_t	umask;
};

/**
 * struct fuse_rename_in - Input structure for FUSE_RENAME
 *
 * The old and new names follow this structure
 *
 * @newdir: Inode ID of the new parent directory
 */
struct fuse_rename_in {
	uint64_t	newdir;
};

/**
 * struct fuse_rename2_in - Input structure for FUSE_RENAME2
 *
 * The old and new names follow this structure
 * @newdir: Inode ID of the new parent directory
 * @flags: RENAME_* flags
 * @padding: Padding for alignment
 */
struct fuse_rename2_in {
	uint64_t	newdir;
	uint32_t	flags;
	uint32_t	padding;
};

/**
 * struct fuse_link_in - Input structure for FUSE_LINK
 *
 * The new name follows this structure
 *
 * @oldnodeid: Inode ID of the file to link to
 */
struct fuse_link_in {
	uint64_t	oldnodeid;
};

/**
 * struct fuse_setattr_in - Input structure for FUSE_SETATTR
 *
 * Specifies which attributes to change. The `valid` field is a bitmask
 * indicating which other fields in this struct contain valid data.
 *
 * @valid: Bitmask of FATTR_* constants indicating which fields are set
 * @padding: Padding for alignment
 * @fh: Optional file handle
 * @size: New size
 * @lock_owner: For BSD locks
 * @atime: Last access time (seconds)
 * @mtime: Last modification time (seconds)
 * @ctime: Last status change time (seconds)
 * @atimensec: Nanosecond part of atime
 * @mtimensec: Nanosecond part of mtime
 * @ctimensec: Nanosecond part of ctime
 * @mode: New mode
 * @unused4: Unused padding
 * @uid: New user ID
 * @gid: New group ID
 * @unused5: Unused padding
 */
struct fuse_setattr_in {
	uint32_t	valid;
	uint32_t	padding;
	uint64_t	fh;
	uint64_t	size;
	uint64_t	lock_owner;
	uint64_t	atime;
	uint64_t	mtime;
	uint64_t	ctime;
	uint32_t	atimensec;
	uint32_t	mtimensec;
	uint32_t	ctimensec;
	uint32_t	mode;
	uint32_t	unused4;
	uint32_t	uid;
	uint32_t	gid;
	uint32_t	unused5;
};

/**
 * struct fuse_open_in - Input structure for FUSE_OPEN and FUSE_OPENDIR
 *
 * @flags: Standard open() flags (O_RDONLY, O_WRONLY, etc.)
 * @open_flags: FUSE_OPEN_* flags
 */
struct fuse_open_in {
	uint32_t	flags;
	uint32_t	open_flags;
};

/**
 * struct fuse_create_in - Input structure for FUSE_CREATE
 *
 * The filename follows this structure
 *
 * @flags: Standard open() flags
 * @mode: Mode of the new file
 * @umask: Umask to apply
 * @open_flags: FUSE_OPEN_* flags
 */
struct fuse_create_in {
	uint32_t	flags;
	uint32_t	mode;
	uint32_t	umask;
	uint32_t	open_flags;
};

/**
 * struct fuse_open_out - Output for FUSE_OPEN, FUSE_OPENDIR, and FUSE_CREATE
 *
 * @fh: The file handle, to be used in subsequent requests (read, write, etc.)
 * @open_flags: FOPEN_* flags returned by the filesystem
 * @backing_id: ID for backing file, for DAX
 */
struct fuse_open_out {
	uint64_t	fh;
	uint32_t	open_flags;
	int32_t		backing_id;
};

/**
 * struct fuse_release_in - Input for FUSE_RELEASE and FUSE_RELEASEDIR
 *
 * @fh: The file handle to release
 * @flags: The open flags for the file
 * @release_flags: FUSE_RELEASE_* flags
 * @lock_owner: The lock owner ID
 */
struct fuse_release_in {
	uint64_t	fh;
	uint32_t	flags;
	uint32_t	release_flags;
	uint64_t	lock_owner;
};

/**
 * struct fuse_flush_in - Input structure for FUSE_FLUSH
 *
 * @fh: The file handle to flush
 * @unused: Unused field
 * @padding: Padding for alignment
 * @lock_owner: The lock owner ID
 */
struct fuse_flush_in {
	uint64_t	fh;
	uint32_t	unused;
	uint32_t	padding;
	uint64_t	lock_owner;
};

/**
 * struct fuse_read_in - Input structure for FUSE_READ
 *
 * @fh: File handle
 * @offset: Offset to start reading from
 * @size: Number of bytes to read
 * @read_flags: FUSE_READ_* flags
 * @lock_owner: Lock owner ID
 * @flags: Open flags
 * @padding: Padding for alignment
 */
struct fuse_read_in {
	uint64_t	fh;
	uint64_t	offset;
	uint32_t	size;
	uint32_t	read_flags;
	uint64_t	lock_owner;
	uint32_t	flags;
	uint32_t	padding;
};

/* Size of fuse_write_in for 32-bit compatibility */
#define FUSE_COMPAT_WRITE_IN_SIZE 24

/**
 * struct fuse_write_in - Input structure for FUSE_WRITE
 *
 * The data to be written follows this structure
 *
 * @fh: File handle
 * @offset: Offset to start writing to
 * @size: Number of bytes to write
 * @write_flags: FUSE_WRITE_* flags
 * @lock_owner: Lock owner ID
 * @flags: Open flags
 * @padding: Padding for alignment
 */
struct fuse_write_in {
	uint64_t	fh;
	uint64_t	offset;
	uint32_t	size;
	uint32_t	write_flags;
	uint64_t	lock_owner;
	uint32_t	flags;
	uint32_t	padding;
};

/**
 * struct fuse_write_out - Output structure for FUSE_WRITE
 *
 * @size: Number of bytes written
 * @padding: Padding for alignment
 */
struct fuse_write_out {
	uint32_t	size;
	uint32_t	padding;
};

/* Size of fuse_statfs_out for 32-bit compatibility */
#define FUSE_COMPAT_STATFS_SIZE 48

/**
 * struct fuse_statfs_out - Output structure for FUSE_STATFS
 *
 * @st: Filesystem statistics (similar to struct statvfs)
 */
struct fuse_statfs_out {
	struct fuse_kstatfs st;
};

/**
 * struct fuse_fsync_in - Input structure for FUSE_FSYNC
 *
 * @fh: File handle
 * @fsync_flags: FUSE_FSYNC_* flags
 * @padding: Padding for alignment
 */
struct fuse_fsync_in {
	uint64_t	fh;
	uint32_t	fsync_flags;
	uint32_t	padding;
};

/* Size of fuse_setxattr_in for 32-bit compatibility */
#define FUSE_COMPAT_SETXATTR_IN_SIZE 8

/**
 * struct fuse_setxattr_in - Input structure for FUSE_SETXATTR
 *
 * The attribute name and value follow this structure
 *
 * @size: Size of the attribute value
 * @flags: XATTR_CREATE or XATTR_REPLACE
 * @setxattr_flags: FUSE_SETXATTR_* flags
 * @padding: Padding for alignment
 */
struct fuse_setxattr_in {
	uint32_t	size;
	uint32_t	flags;
	uint32_t	setxattr_flags;
	uint32_t	padding;
};

/**
 * struct fuse_getxattr_in - Input structure for FUSE_GETXATTR
 *
 * The attribute name follows this structure
 *
 * @size: Size of the buffer to store the value
 * @padding: Padding for alignment
 */
struct fuse_getxattr_in {
	uint32_t	size;
	uint32_t	padding;
};

/**
 * struct fuse_getxattr_out - Output structure for FUSE_GETXATTR
 *
 * If size is non-zero, the attribute value follows this structure
 *
 * @size: Size of the attribute value
 * @padding: Padding for alignment
 */
struct fuse_getxattr_out {
	uint32_t	size;
	uint32_t	padding;
};

/**
 * struct fuse_lk_in - Input for FUSE_GETLK, FUSE_SETLK, and FUSE_SETLKW
 *
 * @fh: File handle
 * @owner: Lock owner
 * @lk: The lock description (range, type)
 * @lk_flags: FUSE_LK_* flags
 * @padding: Padding for alignment
 */
struct fuse_lk_in {
	uint64_t	fh;
	uint64_t	owner;
	struct fuse_file_lock lk;
	uint32_t	lk_flags;
	uint32_t	padding;
};

/**
 * struct fuse_lk_out - Output structure for FUSE_GETLK
 *
 * @lk: Conflicting lock, if any
 */
struct fuse_lk_out {
	struct fuse_file_lock lk;
};

/**
 * struct fuse_access_in - Input structure for FUSE_ACCESS
 *
 * The filename follows this structure
 *
 * @mask: Access mode to check (R_OK, W_OK, etc.)
 * @padding: Padding for alignment
 */
struct fuse_access_in {
	uint32_t	mask;
	uint32_t	padding;
};

/**
 * struct fuse_init_in - Input structure for FUSE_INIT
 *
 * This is the first request sent by the kernel to the daemon
 *
 * @major: Major version of the FUSE protocol
 * @minor: Minor version of the FUSE protocol
 * @max_readahead: Maximum readahead size
 * @flags: FUSE_INIT_* capability flags from kernel
 * @flags2: More FUSE_INIT_* capability flags
 * @unused: Reserved space
 */
struct fuse_init_in {
	uint32_t	major;
	uint32_t	minor;
	uint32_t	max_readahead;
	uint32_t	flags;
	uint32_t	flags2;
	uint32_t	unused[11];
};

/* Size of fuse_init_out for 32-bit compatibility */
#define FUSE_COMPAT_INIT_OUT_SIZE 8
#define FUSE_COMPAT_22_INIT_OUT_SIZE 24

/**
 * struct fuse_init_out - Output structure for FUSE_INIT
 *
 * The daemon's response to the INIT request, negotiating protocol features
 *
 * @major: Major version of the FUSE protocol
 * @minor: Minor version of the FUSE protocol
 * @max_readahead: Maximum readahead size daemon supports
 * @flags: FUSE_INIT_* flags from daemon
 * @max_background: Maximum number of pending background requests
 * @congestion_threshold: Congestion threshold for background requests
 * @max_write: Maximum size of a single write operation
 * @time_gran: Timestamp granularity in nanoseconds
 * @max_pages: Maximum number of pages for a single read/write
 * @map_alignment: For DAX mappings
 * @flags2: More FUSE_INIT_* flags from daemon
 * @max_stack_depth: Maximum stack depth for recursive lookups
 * @request_timeout: Timeout for requests in seconds
 * @unused: Reserved space
 */
struct fuse_init_out {
	uint32_t	major;
	uint32_t	minor;
	uint32_t	max_readahead;
	uint32_t	flags;
	uint16_t	max_background;
	uint16_t	congestion_threshold;
	uint32_t	max_write;
	uint32_t	time_gran;
	uint16_t	max_pages;
	uint16_t	map_alignment;
	uint32_t	flags2;
	uint32_t	max_stack_depth;
	uint16_t	request_timeout;
	uint16_t	unused[11];
};

#define CUSE_INIT_INFO_MAX 4096

/**
 * struct cuse_init_in - Input structure for CUSE_INIT
 *
 * @major: Major version of the FUSE protocol
 * @minor: Minor version of the FUSE protocol
 * @unused: Reserved space
 * @flags: CUSE_INIT_* flags
 */
struct cuse_init_in {
	uint32_t	major;
	uint32_t	minor;
	uint32_t	unused;
	uint32_t	flags;
};

/**
 * struct cuse_init_out - Output structure for CUSE_INIT
 *
 * @major: Major version
 * @minor: Minor version
 * @unused: Reserved space
 * @flags: CUSE_INIT_* flags
 * @max_read: Maximum read size
 * @max_write: Maximum write size
 * @dev_major: Major number of the character device
 * @dev_minor: Minor number of the character device
 * @spare: Reserved space
 */
struct cuse_init_out {
	uint32_t	major;
	uint32_t	minor;
	uint32_t	unused;
	uint32_t	flags;
	uint32_t	max_read;
	uint32_t	max_write;
	uint32_t	dev_major;
	uint32_t	dev_minor;
	uint32_t	spare[10];
};

/**
 * struct fuse_interrupt_in - Input structure for FUSE_INTERRUPT
 *
 * Sent by the kernel to interrupt a request that is taking too long
 *
 * @unique: The `unique` ID of the request to be interrupted
 */
struct fuse_interrupt_in {
	uint64_t	unique;
};

/**
 * struct fuse_bmap_in - Input structure for FUSE_BMAP
 *
 * @block: Logical block number
 * @blocksize: Block size of the filesystem
 * @padding: Padding for alignment
 */
struct fuse_bmap_in {
	uint64_t	block;
	uint32_t	blocksize;
	uint32_t	padding;
};

/**
 * struct fuse_bmap_out - Output structure for FUSE_BMAP
 *
 * @block: Physical block number
 */
struct fuse_bmap_out {
	uint64_t	block;
};

/**
 * struct fuse_ioctl_in - Input structure for FUSE_IOCTL
 *
 * @fh: File handle
 * @flags: FUSE_IOCTL_* flags
 * @cmd: The ioctl command
 * @arg: The ioctl argument (can be a pointer)
 * @in_size: Size of input data (if any)
 * @out_size: Size of output buffer (if any)
 */
struct fuse_ioctl_in {
	uint64_t	fh;
	uint32_t	flags;
	uint32_t	cmd;
	uint64_t	arg;
	uint32_t	in_size;
	uint32_t	out_size;
};

/**
 * struct fuse_ioctl_iovec - Describes a single buffer for FUSE_IOCTL
 *
 * @base: Base address of the buffer
 * @len: Length of the buffer
 */
struct fuse_ioctl_iovec {
	uint64_t	base;
	uint64_t	len;
};

/**
 * struct fuse_ioctl_out - Output structure for FUSE_IOCTL
 *
 * @result: The result of the ioctl, 0 on success, negative on error
 * @flags: FUSE_IOCTL_* flags
 * @in_iovs: Number of input iovecs
 * @out_iovs: Number of output iovecs
 */
struct fuse_ioctl_out {
	int32_t		result;
	uint32_t	flags;
	uint32_t	in_iovs;
	uint32_t	out_iovs;
};

/**
 * struct fuse_poll_in - Input structure for FUSE_POLL
 *
 * @fh: File handle
 * @kh: Kernel handle for poll; must be returned in notification
 * @flags: FUSE_POLL_* flags
 * @events: The poll events to check for (POLLIN, POLLOUT, etc.)
 */
struct fuse_poll_in {
	uint64_t	fh;
	uint64_t	kh;
	uint32_t	flags;
	uint32_t	events;
};

/**
 * struct fuse_poll_out - Output structure for FUSE_POLL
 *
 * @revents: The events that are ready
 * @padding: Padding for alignment
 */
struct fuse_poll_out {
	uint32_t	revents;
	uint32_t	padding;
};

/**
 * struct fuse_notify_poll_wakeup_out - Output for FUSE_NOTIFY_POLL notification
 *
 * @kh: The kernel handle from the original poll request
 */
struct fuse_notify_poll_wakeup_out {
	uint64_t	kh;
};

/**
 * struct fuse_fallocate_in - Input structure for FUSE_FALLOCATE
 *
 * @fh: File handle
 * @offset: Starting offset
 * @length: Length of the range to allocate
 * @mode: fallocate mode (e.g., FALLOC_FL_KEEP_SIZE)
 * @padding: Padding for alignment
 */
struct fuse_fallocate_in {
	uint64_t	fh;
	uint64_t	offset;
	uint64_t	length;
	uint32_t	mode;
	uint32_t	padding;
};

/**
 * FUSE_UNIQUE_RESEND - FUSE request unique ID flag
 *
 * If this bit is set in `fuse_in_header.unique`, it indicates that this is a
 * resend of a request that may have timed out. The filesystem daemon should
 * handle this by either re-processing or checking if it's already been
 * completed.
 */
#define FUSE_UNIQUE_RESEND (1ULL << 63)

/**
 * FUSE_INVALID_UIDGID - Invalid UID/GID marker
 *
 * This value will be set by the kernel to
 * (struct fuse_in_header).{uid,gid} fields in
 * case when::
 *
 *   - fuse daemon enabled FUSE_ALLOW_IDMAP
 *   - idmapping information is not available and uid/gid
 *     can not be mapped in accordance with an idmapping
 *
 * Note: an idmapping information always available
 * for inode creation operations like:
 * FUSE_MKNOD, FUSE_SYMLINK, FUSE_MKDIR, FUSE_TMPFILE,
 * FUSE_CREATE and FUSE_RENAME2 (with RENAME_WHITEOUT).
 */
#define FUSE_INVALID_UIDGID ((uint32_t)(-1))

/**
 * struct fuse_in_header - Header for every request from the kernel
 *
 * Every FUSE request read from /dev/fuse begins with this header
 *
 * @len: Total length of the request, including this header
 * @opcode: The operation code (one of enum fuse_opcode)
 * @unique: Unique request ID
 * @nodeid: Inode ID the operation is for
 * @uid: UID of the calling process
 * @gid: GID of the calling process
 * @pid: PID of the calling process
 * @total_extlen: Length of all extensions in 8-byte units
 * @padding: Padding for alignment
 */
struct fuse_in_header {
	uint32_t	len;
	uint32_t	opcode;
	uint64_t	unique;
	uint64_t	nodeid;
	uint32_t	uid;
	uint32_t	gid;
	uint32_t	pid;
	uint16_t	total_extlen;
	uint16_t	padding;
};

/**
 * struct fuse_out_header - Header for every reply to the kernel
 *
 * Every FUSE reply written to /dev/fuse begins with this header
 *
 * @len: Total length of the reply, including this header
 * @error: Error number (0 for success, negative for error)
 * @unique: The `unique` ID from the corresponding request
 */
struct fuse_out_header {
	uint32_t	len;
	int32_t		error;
	uint64_t	unique;
};

/**
 * struct fuse_dirent - A single directory entry in a READDIR response
 *
 * A READDIR response consists of a buffer of one or more of these structures,
 * one after the other.
 *
 * @ino: Inode number
 * @off: Offset of the next directory entry
 * @namelen: Length of the name
 * @type: File type (DT_REG, DT_DIR, etc.)
 * @name: The filename (flexible array member)
 */
struct fuse_dirent {
	uint64_t	ino;
	uint64_t	off;
	uint32_t	namelen;
	uint32_t	type;
	char name[];
};

/* Align variable length records to 64bit boundary */
#define FUSE_REC_ALIGN(x) \
	(((x) + sizeof(uint64_t) - 1) & ~(sizeof(uint64_t) - 1))

#define FUSE_NAME_OFFSET offsetof(struct fuse_dirent, name)
#define FUSE_DIRENT_ALIGN(x) FUSE_REC_ALIGN(x)

/* Macro to calculate the full, aligned size of a fuse_dirent structure */
#define FUSE_DIRENT_SIZE(d) \
	FUSE_DIRENT_ALIGN(FUSE_NAME_OFFSET + (d)->namelen)

/**
 * struct fuse_direntplus - A single entry in a READDIRPLUS response
 *
 * This combines the directory entry information with the full entry attributes,
 * saving a separate GETATTR call
 *
 * @entry_out: The attributes and cache info for the entry
 * @dirent: The directory entry info (ino, name, etc.)
 */
struct fuse_direntplus {
	struct fuse_entry_out entry_out;
	struct fuse_dirent dirent;
};

#define FUSE_NAME_OFFSET_DIRENTPLUS \
	offsetof(struct fuse_direntplus, dirent.name)

/* Macro to calculate the full, aligned size of a fuse_direntplus structure */
#define FUSE_DIRENTPLUS_SIZE(d) \
	FUSE_DIRENT_ALIGN(FUSE_NAME_OFFSET_DIRENTPLUS + (d)->dirent.namelen)

/**
 * struct fuse_notify_inval_inode_out - Output for FUSE_NOTIFY_INVAL_INODE
 *
 * @ino: The inode number to invalidate
 * @off: Offset of the invalid region (-1 for all)
 * @len: Length of the invalid region (-1 for all)
 */
struct fuse_notify_inval_inode_out {
	uint64_t	ino;
	int64_t		off;
	int64_t		len;
};

/**
 * struct fuse_notify_inval_entry_out - Output for FUSE_NOTIFY_INVAL_ENTRY
 *
 * The name of the entry to invalidate follows this structure
 *
 * @parent: Inode ID of the parent directory
 * @namelen: Length of the name
 * @flags: Flags for the invalidation
 */
struct fuse_notify_inval_entry_out {
	uint64_t	parent;
	uint32_t	namelen;
	uint32_t	flags;
};

/**
 * struct fuse_notify_delete_out - Output for a FUSE_NOTIFY_DELETE notification
 *
 * The name of the deleted entry follows this structure
 *
 * @parent: Inode ID of the parent directory
 * @child: Inode ID of the deleted child
 * @namelen: Length of the child's name
 * @padding: Padding for alignment
 */
struct fuse_notify_delete_out {
	uint64_t	parent;
	uint64_t	child;
	uint32_t	namelen;
	uint32_t	padding;
};

/**
 * struct fuse_notify_store_out - Output for a FUSE_NOTIFY_STORE notification
 *
 * The data to be stored follows this structure
 *
 * @nodeid: Inode ID the data belongs to
 * @offset: Offset of the data within the file
 * @size: Size of the data
 * @padding: Padding for alignment
 */
struct fuse_notify_store_out {
	uint64_t	nodeid;
	uint64_t	offset;
	uint32_t	size;
	uint32_t	padding;
};

/**
 * struct fuse_notify_retrieve_out - Output for FUSE_NOTIFY_RETRIEVE
 *
 * @notify_unique: A unique ID for this retrieve request
 * @nodeid: Inode ID to retrieve data for
 * @offset: Offset to retrieve data from
 * @size: Size of the data to retrieve
 * @padding: Padding for alignment
 */
struct fuse_notify_retrieve_out {
	uint64_t	notify_unique;
	uint64_t	nodeid;
	uint64_t	offset;
	uint32_t	size;
	uint32_t	padding;
};

/* Matches the size of fuse_write_in */
/**
 * struct fuse_notify_retrieve_in - Input for a FUSE_NOTIFY_RETRIEVE reply
 *
 * The retrieved data follows this structure
 *
 * @dummy1: Dummy field for size matching
 * @offset: Offset of the retrieved data
 * @size: Size of the retrieved data
 * @dummy2: Dummy field for size matching
 * @dummy3: Dummy field for size matching
 * @dummy4: Dummy field for size matching
 */
struct fuse_notify_retrieve_in {
	uint64_t	dummy1;
	uint64_t	offset;
	uint32_t	size;
	uint32_t	dummy2;
	uint64_t	dummy3;
	uint64_t	dummy4;
};

/**
 * struct fuse_backing_map - Structure for DAX backing file mapping
 *
 * @fd: File descriptor of the backing file
 * @flags: Flags for the mapping
 * @padding: Padding for alignment
 */
struct fuse_backing_map {
	int32_t		fd;
	uint32_t	flags;
	uint64_t	padding;
};

/* Device ioctls for /dev/fuse: */
#define FUSE_DEV_IOC_MAGIC		229
#define FUSE_DEV_IOC_CLONE		_IOR(FUSE_DEV_IOC_MAGIC, 0, uint32_t)
#define FUSE_DEV_IOC_BACKING_OPEN	_IOW(FUSE_DEV_IOC_MAGIC, 1, \
					     struct fuse_backing_map)
#define FUSE_DEV_IOC_BACKING_CLOSE	_IOW(FUSE_DEV_IOC_MAGIC, 2, uint32_t)

/**
 * struct fuse_lseek_in - Input structure for FUSE_LSEEK
 *
 * @fh: File handle
 * @offset: The offset to seek to/by
 * @whence: The seek type (SEEK_SET, SEEK_CUR, SEEK_END)
 * @padding: Padding for alignment
 */
struct fuse_lseek_in {
	uint64_t	fh;
	uint64_t	offset;
	uint32_t	whence;
	uint32_t	padding;
};

/**
 * struct fuse_lseek_out - Output structure for FUSE_LSEEK
 *
 * @offset: The resulting offset
 */
struct fuse_lseek_out {
	uint64_t	offset;
};

/**
 * struct fuse_copy_file_range_in - Input structure for FUSE_COPY_FILE_RANGE
 *
 * @fh_in: File handle to read from
 * @off_in: Offset to read from
 * @nodeid_out: Inode ID of the destination file
 * @fh_out: File handle to write to
 * @off_out: Offset to write to
 * @len: Number of bytes to copy
 * @flags: Flags for the copy operation
 */
struct fuse_copy_file_range_in {
	uint64_t	fh_in;
	uint64_t	off_in;
	uint64_t	nodeid_out;
	uint64_t	fh_out;
	uint64_t	off_out;
	uint64_t	len;
	uint64_t	flags;
};

#define FUSE_SETUPMAPPING_FLAG_WRITE (1ull << 0) /**< Map for writing */
#define FUSE_SETUPMAPPING_FLAG_READ  (1ull << 1) /**< Map for reading */

/**
 * struct fuse_setupmapping_in - Input structure for FUSE_SETUPMAPPING (DAX)
 *
 * @fh: An already open file handle
 * @foffset: Offset into the file to start the mapping
 * @len: Length of mapping required
 * @flags: Flags, FUSE_SETUPMAPPING_FLAG_*
 * @moffset: Offset in the device's memory window
 */
struct fuse_setupmapping_in {
	uint64_t	fh;
	uint64_t	foffset;
	uint64_t	len;
	uint64_t	flags;
	uint64_t	moffset;
};

/**
 * struct fuse_removemapping_in - Input structure for FUSE_REMOVEMAPPING (DAX)
 *
 * @count: Number of fuse_removemapping_one entries that follow
 */
struct fuse_removemapping_in {
	uint32_t        count;
};

/**
 * struct fuse_removemapping_one - A single unmap request for FUSE_REMOVEMAPPING
 *
 * @moffset: Offset in the DAX window to start the unmapping
 * @len: Length of the mapping to remove
 */
struct fuse_removemapping_one {
	uint64_t        moffset;
	uint64_t	len;
};

/* Maximum number of removemapping entries that can fit in a page */
#define FUSE_REMOVEMAPPING_MAX_ENTRY   \
		(PAGE_SIZE / sizeof(struct fuse_removemapping_one))

/**
 * struct fuse_syncfs_in - Input structure for FUSE_SYNCFS
 *
 * @padding: Reserved
 */
struct fuse_syncfs_in {
	uint64_t	padding;
};

/**
 * struct fuse_secctx - Security context structure
 *
 * For each security context (e.g., SELinux label), a fuse_secctx is sent,
 * followed by the context name (e.g., "selinux"), which is then followed by
 * the actual context-label string.
 *
 * @size: Size of the context label that follows
 * @padding: Padding for alignment
 */
struct fuse_secctx {
	uint32_t	size;
	uint32_t	padding;
};

/**
 * struct fuse_secctx_header - Header for a block of security contexts
 *
 * This precedes a series of `fuse_secctx` structures in a request.
 * Contains the information about how many fuse_secctx structures are being
 * sent and what's the total size of all security contexts (including
 * size of fuse_secctx_header).
 *
 * @size: Total size of all security contexts, including this header
 * @nr_secctx: Number of `fuse_secctx` structures being sent
 */
struct fuse_secctx_header {
	uint32_t	size;
	uint32_t	nr_secctx;
};

/**
 * struct fuse_ext_header - Generic header for FUSE request extensions
 *
 * This is made compatible with fuse_secctx_header by using type values >
 * FUSE_MAX_NR_SECCTX
 *
 * @size: Total size of this extension, including this header
 * @type: Type of the extension
 */
struct fuse_ext_header {
	uint32_t	size;
	uint32_t	type;
};

/**
 * struct fuse_supp_groups - Supplementary group extension structure
 *
 * @nr_groups: Number of supplementary groups
 * @groups: Flexible array of group IDs
 */
struct fuse_supp_groups {
	uint32_t	nr_groups;
	uint32_t	groups[];
};

/* Size of the io_uring header sections */
#define FUSE_URING_IN_OUT_HEADER_SZ 128
#define FUSE_URING_OP_IN_OUT_SZ 128

/**
 * struct fuse_uring_ent_in_out - io_uring communication entry
 *
 * Used as part of the fuse_uring_req_header for io_uring communication
 *
 * @flags: Flags for the uring entry
 * @commit_id: Commit ID to be used in a reply to a ring request
 * @payload_sz: Size of user payload buffer
 * @padding: Padding for alignment
 * @reserved: Reserved space
 */
struct fuse_uring_ent_in_out {
	uint64_t flags;
	uint64_t commit_id;
	uint32_t payload_sz;
	uint32_t padding;
	uint64_t reserved;
};

/**
 * struct fuse_uring_req_header - Header for all fuse-io-uring requests
 *
 * This structure encapsulates the standard FUSE headers for use with the
 * high-performance io_uring interface.
 *
 * @in_out: Space for struct fuse_in_header / struct fuse_out_header
 * @op_in: Space for the per-opcode header (e.g., fuse_read_in)
 * @ring_ent_in_out: The io_uring entry data
 */
struct fuse_uring_req_header {
	char in_out[FUSE_URING_IN_OUT_HEADER_SZ];
	char op_in[FUSE_URING_OP_IN_OUT_SZ];
	struct fuse_uring_ent_in_out ring_ent_in_out;
};

/**
 * enum fuse_uring_cmd - Commands for io_uring
 *
 * Commands sent from the userspace daemon to the kernel via io_uring SQEs
 *
 * @FUSE_IO_URING_CMD_INVALID: Invalid command
 * @FUSE_IO_URING_CMD_REGISTER: Register the request buffer and fetch a fuse
 * request
 * @FUSE_IO_URING_CMD_COMMIT_AND_FETCH: Commit a FUSE request result and fetch
 * the next request
 */
enum fuse_uring_cmd {
	FUSE_IO_URING_CMD_INVALID = 0,
	FUSE_IO_URING_CMD_REGISTER = 1,
	FUSE_IO_URING_CMD_COMMIT_AND_FETCH = 2,
};

/**
 * struct fuse_uring_cmd_req - Command structure for an io_uring SQE
 *
 * This is placed in the 80-byte command area of an io_uring SQE
 *
 * @flags: Flags for the command
 * @commit_id: Entry identifier for commits
 * @qid: The queue index the command is for
 * @padding: Padding for alignment
 */
struct fuse_uring_cmd_req {
	uint64_t flags;
	uint64_t commit_id;
	uint16_t qid;
	uint8_t padding[6];
};

#endif /* _LINUX_FUSE_H */
