## Copyright (C) 2019-2022 Free Software Foundation, Inc.
##
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <https://www.gnu.org/licenses/>.

modesdir = $(docdir)/%D%

## ------ ##
## Modes.  ##
## ------ ##

check_PROGRAMS += %D%/modes
TESTS += %D%/modes.test
EXTRA_DIST += %D%/modes.test
nodist_%C%_modes_SOURCES = %D%/modes.y
%D%/modes.c: $(dependencies)

# Don't use gnulib's system headers.
%C%_modes_CPPFLAGS = -I$(top_srcdir)/%D% -I$(top_builddir)/%D%
%C%_modes_CFLAGS = $(TEST_CFLAGS)

dist_modes_DATA = %D%/modes.y %D%/Makefile %D%/README.md
CLEANFILES += %D%/modes.[ch] %D%/modes.output %D%/scan.c
CLEANDIRS += %D%/*.dSYM
