# This file is part of GNU paxutils
#
# Copyright (c) 2005-2024 Free Software Foundation, Inc.
#
# Written by Sergey Poznyakoff
#
# GNU Paxutils is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 3, or (at your option) any later
# version.
#
# GNU Paxutils is distributed in the hope that it will be useful, but
# without any warranty; without even the implied warranty of
# merchantability or fitness for a particular purpose.  see the gnu general
# public license for more details.
#
# You should have received a copy of the GNU General Public License along
# with GNU Paxutils.  If not, see <http://www.gnu.org/licenses/>.

ACLOCAL_AMFLAGS = -I am -I m4

EXTRA_DIST = Make.rules

SUBDIRS = gnu paxlib paxtest rmt po

gen_start_date = 2008-05-21
prev_change_log = ChangeLog.CVS
changelog_dir = .
include Make.rules

dist-hook:
	$(MAKE) changelog_dir=$(distdir) ChangeLog

