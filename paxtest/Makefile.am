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
# 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

noinst_PROGRAMS = paxtest
paxtest_SOURCES = paxtest.c
noinst_HEADERS = paxtest.h

INCLUDES = -I$(top_srcdir)/lib -I../ -I../lib -I../paxlib

LDADD = ../paxlib/libpax.a ../lib/libgnu.a $(LIBINTL) $(LIBICONV)

