
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

  The filesystem can be mounted while it is running inside gdb (for
  debugging) purposes as follows (adapt to your setup):

  gdb --args ./utepfs --server=server --port=port ./fuse-mnt/ -f

  It can then be unmounted (in another terminal) with

  fusermount -u ./fuse-mnt

  DO NOT CHANGE ANYTHING IN THIS FILE (UNLESS YOUR INSTRUCTOR ALLOWS
  YOU TO DO SO).

  ALL YOUR CODE GOES INTO implementation.c !!!

*/

#define FUSE_USE_VERSION  26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netdb.h>

#define DEFAULT_PORT_NAME "7777"

struct __utepfs_options_struct_t {
  const char *server_name;
  const char *port_name;
  int show_help;
};

#define OPTION(t, p)  { t, offsetof(struct __utepfs_options_struct_t, p), 1 }

static const struct fuse_opt __utepfs_option_spec[] = {
  OPTION("--server=%s", server_name),
  OPTION("--port=%s", port_name),
  OPTION("-h", show_help),
  OPTION("--help", show_help),
  FUSE_OPT_END
};

struct __utepfs_environment_struct_t {
  pthread_mutex_t env_lock;
  uid_t           uid;
  gid_t           gid;
  int             fd;
};

static int __utepfs_open_tcp_fd(const char *server_name,
                                const char *port_name) {
  struct addrinfo hints;
  struct addrinfo *result, *curr;
  int fd;
  int gai_code;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  gai_code = getaddrinfo(server_name, port_name,
                         &hints, &result);
  if (gai_code != 0) {
    fprintf(stderr,
            "Cannot get address info: %s\n",
            gai_strerror(gai_code));
    return -1;
  }
  for (curr=result; curr!=NULL; curr=curr->ai_next) {
    fd = socket(curr->ai_family,
                curr->ai_socktype,
                curr->ai_protocol);
    if (fd >= 0) {
      if (connect(fd,
                  curr->ai_addr,
                  curr->ai_addrlen) >= 0) {
        freeaddrinfo(result);
        return fd;
      }
      if (close(fd) < 0) {
        fprintf(stderr,
                "Cannot close a socket: %s\n",
                strerror(errno));
        freeaddrinfo(result);
        return -1;
      }
    }
  }
  freeaddrinfo(result);
  return -1;
}

static int __utepfs_setup_environment(struct __utepfs_environment_struct_t *env, struct __utepfs_options_struct_t *opts) {
  int fd;
  const char *default_port_name = DEFAULT_PORT_NAME;
  const char *effective_port_name = default_port_name;

  /* Sanity checks */
  if (env == NULL) {
    fprintf(stderr, "Did not receive a FUSE environment\n");
    return 0;
  }
  if (opts == NULL) {
    fprintf(stderr, "Did not receive any options\n");
    return 0;
  }

  /* Open TCP/IP connection */
  if (opts->server_name == NULL) {
    fprintf(stderr, "No server name was specified\n");
    return 0;
  }
  if (*opts->server_name == '\0') {
    fprintf(stderr, "Specified server name is empty\n");
    return 0;
  }
  
  if (opts->port_name != NULL) {
    if (*(opts->port_name) != '\0') {
      effective_port_name = opts->port_name;
    }
  } 
  fd = __utepfs_open_tcp_fd(opts->server_name,
			    effective_port_name);
  if (fd < 0) {
    fprintf(stderr,
	    "Cannot open a TCP/IP connection to server %s at port %s\n",
	    opts->server_name,
	    effective_port_name);
    return 0;
  }
  
  /* Setup lock for the threads */
  if (pthread_mutex_init(&(env->env_lock), NULL) != 0) {
    fprintf(stderr, "Cannot setup mutex: %s\n", strerror(errno));
    if (close(fd) < 0) {
      fprintf(stderr, "Cannot close TCP/IP connection: %s\n", strerror(errno));
    }
    return 0;
  }

  /* Get uid and gid, write back and succeed */
  env->uid = getuid();
  env->gid = getgid();
  env->fd = fd;
  return 1;
}

static void __utepfs_clear_environment(struct __utepfs_environment_struct_t *env) {
  if (pthread_mutex_destroy(&(env->env_lock)) != 0) {
    fprintf(stderr, "Cannot destroy mutex: %s\n", strerror(errno));
  }
  if (close(env->fd) < 0) {
    fprintf(stderr, "Cannot close TCP/IP connection: %s\n", strerror(errno));
  }
}

/* Declaration for the implementations of the operations */

int __utepfs_getattr_implem(int, int *, uid_t, gid_t, const char *, struct stat *);
int __utepfs_readdir_implem(int, int *, const char *, char ***);
int __utepfs_mknod_implem(int, int *, const char *);
int __utepfs_unlink_implem(int, int *, const char *);
int __utepfs_mkdir_implem(int, int *, const char *);
int __utepfs_rmdir_implem(int, int *, const char *);
int __utepfs_rename_implem(int, int *, const char *, const char*);
int __utepfs_truncate_implem(int, int *, const char *, off_t);
int __utepfs_open_implem(int, int *, const char *);
int __utepfs_read_implem(int, int *, const char *, char *, size_t, off_t);
int __utepfs_write_implem(int, int *, const char *, const char *, size_t, off_t);
int __utepfs_utimens_implem(int, int *, const char *, const struct timespec [2]);
int __utepfs_statfs_implem(int, int *, struct statvfs*);
int __utepfs_fsync_implem(int, int *);

/* End of declarations */

/* FUSE operations part */

static int __utepfs_getattr(const char *path, struct stat *st) {
  struct fuse_context *context;
  struct __utepfs_environment_struct_t *env;
  int __utepfs_errno, res;

  context = fuse_get_context();
  env = (struct __utepfs_environment_struct_t *) (context->private_data);

  memset(st, 0, sizeof(struct stat));

  __utepfs_errno = ENOENT;
  pthread_mutex_lock(&(env->env_lock));
  res = __utepfs_getattr_implem(env->fd,
                                &__utepfs_errno,
                                env->uid,
                                env->gid,
                                path,
                                st);
  pthread_mutex_unlock(&(env->env_lock));
  if (res >= 0)
    return res;
  return -__utepfs_errno;
}

static int __utepfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                            off_t offset, struct fuse_file_info *fi) {
  struct fuse_context *context;
  struct __utepfs_environment_struct_t *env;
  int __utepfs_errno, res, i;
  char **names;

  (void) offset;
  (void) fi;

  context = fuse_get_context();
  env = (struct __utepfs_environment_struct_t *) (context->private_data);

  names = NULL;
  __utepfs_errno = ENOENT;
  pthread_mutex_lock(&(env->env_lock));
  res = __utepfs_readdir_implem(env->fd,
                                &__utepfs_errno,
                                path,
                                &names);
  pthread_mutex_unlock(&(env->env_lock));
  if (res >= 0) {
    if (res == 0) {
      filler(buf, ".", NULL, 0);
      filler(buf, "..", NULL, 0);
      return 0;
    } else {
      if (names != NULL) {
        filler(buf, ".", NULL, 0);
        filler(buf, "..", NULL, 0);
        for (i=0;i<res;i++) {
          filler(buf, names[i], NULL, 0);
          free(names[i]);
        }
        free(names);
        return 0;
      } else {
        return -ENOENT;
      }
    }
  }
  return -__utepfs_errno;
}

static int __utepfs_mknod(const char* path, mode_t mode, dev_t dev) {
  struct fuse_context *context;
  struct __utepfs_environment_struct_t *env;
  int __utepfs_errno, res;

  (void) dev;

  if (!S_ISREG(mode)) return -EPERM;

  context = fuse_get_context();
  env = (struct __utepfs_environment_struct_t *) (context->private_data);

  __utepfs_errno = ENOENT;
  pthread_mutex_lock(&(env->env_lock));
  res = __utepfs_mknod_implem(env->fd,
                              &__utepfs_errno,
                              path);
  pthread_mutex_unlock(&(env->env_lock));
  if (res >= 0)
    return res;
  return -__utepfs_errno;
}

static int __utepfs_unlink(const char* path) {
  struct fuse_context *context;
  struct __utepfs_environment_struct_t *env;
  int __utepfs_errno, res;

  context = fuse_get_context();
  env = (struct __utepfs_environment_struct_t *) (context->private_data);

  __utepfs_errno = ENOENT;
  pthread_mutex_lock(&(env->env_lock));
  res = __utepfs_unlink_implem(env->fd,
                               &__utepfs_errno,
                               path);
  pthread_mutex_unlock(&(env->env_lock));
  if (res >= 0)
    return res;
  return -__utepfs_errno;
}

static int __utepfs_mkdir(const char* path, mode_t mode) {
  struct fuse_context *context;
  struct __utepfs_environment_struct_t *env;
  int __utepfs_errno, res;

  context = fuse_get_context();
  env = (struct __utepfs_environment_struct_t *) (context->private_data);

  __utepfs_errno = ENOENT;
  pthread_mutex_lock(&(env->env_lock));
  res = __utepfs_mkdir_implem(env->fd,
                              &__utepfs_errno,
                              path);
  pthread_mutex_unlock(&(env->env_lock));
  if (res >= 0)
    return res;
  return -__utepfs_errno;
}

static int __utepfs_rmdir(const char* path) {
  struct fuse_context *context;
  struct __utepfs_environment_struct_t *env;
  int __utepfs_errno, res;

  context = fuse_get_context();
  env = (struct __utepfs_environment_struct_t *) (context->private_data);

  __utepfs_errno = ENOENT;
  pthread_mutex_lock(&(env->env_lock));
  res = __utepfs_rmdir_implem(env->fd,
                              &__utepfs_errno,
                              path);
  pthread_mutex_unlock(&(env->env_lock));
  if (res >= 0)
    return res;
  return -__utepfs_errno;
}

static int __utepfs_rename(const char* from, const char* to) {
  struct fuse_context *context;
  struct __utepfs_environment_struct_t *env;
  int __utepfs_errno, res;

  context = fuse_get_context();
  env = (struct __utepfs_environment_struct_t *) (context->private_data);

  __utepfs_errno = ENOENT;
  pthread_mutex_lock(&(env->env_lock));
  res = __utepfs_rename_implem(env->fd,
                               &__utepfs_errno,
                               from,
                               to);
  pthread_mutex_unlock(&(env->env_lock));
  if (res >= 0)
    return res;
  return -__utepfs_errno;
}

static int __utepfs_truncate(const char* path, off_t size) {
  struct fuse_context *context;
  struct __utepfs_environment_struct_t *env;
  int __utepfs_errno, res;

  context = fuse_get_context();
  env = (struct __utepfs_environment_struct_t *) (context->private_data);

  __utepfs_errno = ENOENT;
  pthread_mutex_lock(&(env->env_lock));
  res = __utepfs_truncate_implem(env->fd,
                                 &__utepfs_errno,
                                 path,
                                 size);
  pthread_mutex_unlock(&(env->env_lock));
  if (res >= 0)
    return res;
  return -__utepfs_errno;
}

static int __utepfs_open(const char* path, struct fuse_file_info* fi) {
  struct fuse_context *context;
  struct __utepfs_environment_struct_t *env;
  int __utepfs_errno, res;

  if (!(((fi->flags & O_ACCMODE) == O_RDONLY) ||
        ((fi->flags & O_ACCMODE) == O_WRONLY) ||
        ((fi->flags & O_ACCMODE) == O_RDWR))) return -EINVAL;
  if (fi->flags & O_TRUNC) return -EINVAL;

  context = fuse_get_context();
  env = (struct __utepfs_environment_struct_t *) (context->private_data);

  __utepfs_errno = ENOENT;
  pthread_mutex_lock(&(env->env_lock));
  res = __utepfs_open_implem(env->fd,
                             &__utepfs_errno,
                             path);
  pthread_mutex_unlock(&(env->env_lock));
  if (res >= 0)
    return res;
  return -__utepfs_errno;
}

static int __utepfs_read(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi) {
  struct fuse_context *context;
  struct __utepfs_environment_struct_t *env;
  int __utepfs_errno, res;

  (void) fi;

  context = fuse_get_context();
  env = (struct __utepfs_environment_struct_t *) (context->private_data);

  __utepfs_errno = EFAULT;
  pthread_mutex_lock(&(env->env_lock));
  res = __utepfs_read_implem(env->fd,
                             &__utepfs_errno,
                             path,
                             buf,
                             size,
                             offset);
  pthread_mutex_unlock(&(env->env_lock));
  if (res >= 0)
    return res;
  return -__utepfs_errno;
}

static int __utepfs_write(const char* path, const char *buf, size_t size, off_t offset, struct fuse_file_info* fi) {
  struct fuse_context *context;
  struct __utepfs_environment_struct_t *env;
  int __utepfs_errno, res;

  (void) fi;

  context = fuse_get_context();
  env = (struct __utepfs_environment_struct_t *) (context->private_data);

  __utepfs_errno = EFAULT;
  pthread_mutex_lock(&(env->env_lock));
  res = __utepfs_write_implem(env->fd,
                              &__utepfs_errno,
                              path,
                              buf,
                              size,
                              offset);
  pthread_mutex_unlock(&(env->env_lock));
  if (res >= 0)
    return res;
  return -__utepfs_errno;
}

static int __utepfs_statfs(const char* path, struct statvfs* stbuf) {
  struct fuse_context *context;
  struct __utepfs_environment_struct_t *env;
  int __utepfs_errno, res;

  (void) path;

  context = fuse_get_context();
  env = (struct __utepfs_environment_struct_t *) (context->private_data);

  memset(stbuf, 0, sizeof(struct statvfs));

  __utepfs_errno = ENOENT;
  pthread_mutex_lock(&(env->env_lock));
  res = __utepfs_statfs_implem(env->fd,
                               &__utepfs_errno,
                               stbuf);
  pthread_mutex_unlock(&(env->env_lock));
  if (res >= 0)
    return res;
  return -__utepfs_errno;
}

static int __utepfs_utimens(const char* path, const struct timespec ts[2]) {
  struct fuse_context *context;
  struct __utepfs_environment_struct_t *env;
  int __utepfs_errno, res;

  context = fuse_get_context();
  env = (struct __utepfs_environment_struct_t *) (context->private_data);

  __utepfs_errno = ENOENT;
  pthread_mutex_lock(&(env->env_lock));
  res = __utepfs_utimens_implem(env->fd,
                                &__utepfs_errno,
                                path,
                                ts);
  pthread_mutex_unlock(&(env->env_lock));
  if (res >= 0)
    return res;
  return -__utepfs_errno;
}

static int __utepfs_fsync(const char *path, int datasync, struct fuse_file_info *fi) {
  struct fuse_context *context;
  struct __utepfs_environment_struct_t *env;
  int __utepfs_errno, res;

  (void) path;
  (void) datasync;
  (void) fi;

  context = fuse_get_context();
  env = (struct __utepfs_environment_struct_t *) (context->private_data);

  __utepfs_errno = EIO;
  pthread_mutex_lock(&(env->env_lock));
  res = __utepfs_fsync_implem(env->fd,
                              &__utepfs_errno);
  pthread_mutex_unlock(&(env->env_lock));
  if (res >= 0)
    return res;
  return -__utepfs_errno;
}

static void __utepfs_destroy(void *private_data) {
  struct __utepfs_environment_struct_t *env;

  if (private_data == NULL) return;
  env = (struct __utepfs_environment_struct_t *) private_data;
  __utepfs_clear_environment(env);
}

static struct fuse_operations __utepfs_operations = {
  .getattr = __utepfs_getattr,
  .readdir = __utepfs_readdir,
  .mkdir = __utepfs_mkdir,
  .mknod = __utepfs_mknod,
  .unlink = __utepfs_unlink,
  .rmdir = __utepfs_rmdir,
  .rename = __utepfs_rename,
  .truncate = __utepfs_truncate,
  .open = __utepfs_open,
  .read = __utepfs_read,
  .write = __utepfs_write,
  .statfs = __utepfs_statfs,
  .utimens = __utepfs_utimens,
  .fsync = __utepfs_fsync,
  .destroy = __utepfs_destroy
};

/* End of FUSE operations part */

static void __utepfs_show_help(const char *name) {
  printf("usage: %s [options] <mountpoint>\n\n", name);
  printf("File-system specific options:\n"
         "    --server=<s>            Name of the UTEPFS server to connect to.\n"
         "    --port=<s>              Port on the UTEPFS server.\n"
         "                            Default: 8888\n"
         "\n");
}

int main(int argc, char *argv[]) {
  struct __utepfs_options_struct_t __utepfs_options;
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  struct __utepfs_environment_struct_t __utepfs_environment;
  struct __utepfs_environment_struct_t *env_ptr = NULL;

  /* Initialize defaults */
  __utepfs_options.server_name = NULL;
  __utepfs_options.port_name = NULL;
  __utepfs_options.show_help = 0;

  /* Parse options */
  if (fuse_opt_parse(&args, &__utepfs_options, __utepfs_option_spec, NULL) == -1)
    return 1;

  /* If we are not just handling help texts, we need to setup the
     file-system environment.
  */
  if (!__utepfs_options.show_help) {
    env_ptr = &__utepfs_environment;
    if (!__utepfs_setup_environment(env_ptr, &__utepfs_options))
      return 1;
  } else {
    /* Handle displaying of help text */
    __utepfs_show_help(argv[0]);
    assert(fuse_opt_add_arg(&args, "--help") == 0);
    args.argv[0] = (char*) "";
  }

  return fuse_main(args.argc, args.argv, &__utepfs_operations, env_ptr);
}
