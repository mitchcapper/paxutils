# This file is part of GNU paxutils
#
# Copyright (C) 2005 Free Software Foundation, Inc.
#
# Written by Sergey Poznyakoff
#
# GNU paxutils is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2, or (at your option) any later
# version.
#
# GNU paxutils is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
# Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with GNU paxutils; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

INCLUDES = -I$(top_srcdir)/lib -I../ -I../lib -I../pax

noinst_LIBRARIES = libpax.a
noinst_HEADERS = tar.h paxbuf.h pax.h

libpax_a_SOURCES = \
 error.c\
 exit.c\
 names.c\
 paxbuf.c\
 paxlib.h\
 tarbuf.c\
 rtape.c

localedir = $(datadir)/locale

DISTCLEANFILES = localedir.h
localedir.h : Makefile
	echo '#define LOCALEDIR "$(localedir)"' >$@
	echo "#ifndef DEFAULT_RMT_COMMAND" >> $@
	echo "# define DEFAULT_RMT_COMMAND \"$(DEFAULT_RMT_DIR)/`echo rmt | sed '$(transform)'`$(EXEEXT)\"" >> $@
	echo "#endif" >> $@

rtapelib.o: localedir.h


