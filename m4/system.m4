AC_DEFUN([PU_SYSTEM],[
  AC_HEADER_STDC

  AC_CHECK_HEADERS_ONCE([string.h memory.h fcntl.h unistd.h sys/wait.h sys/gentape.h \
 sys/tape.h sys/device.h sys/param.h sys/buf.h sys/tprintf.h sys/mtio.h \
 sgtty.h sys/io/trioctl.h inttypes.h locale.h utime.h])

  AC_CHECK_HEADERS_ONCE([sys/time.h])
  AC_HEADER_TIME

  AC_CHECK_DECLS_ONCE([time valloc])

  AC_CHECK_MEMBERS([struct stat.st_blksize]) dnl instead of AC_STRUCT_ST_BLKSIZE
  AC_STRUCT_ST_BLOCKS
  AC_STRUCT_ST_BLKSIZE

  AC_CHECK_FUNCS_ONCE(lstat mkfifo setlocale)
  AC_REQUIRE([gl_AC_TYPE_UINTMAX_T])
])

