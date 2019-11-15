# Makefile.am for poke/man

# Copyright (C) 2019 Jose E. Marchesi

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

man_MANS = poke.1

EXTRA_DIST = $(man_MANS)
MAINTAINERCLEANFILES = $(man_MANS)

# The man pages depend on the version number.
common_mandeps = $(top_srcdir)/configure.ac

# Jose writes:
##  Depend on the source, not the binary; we don't need to regenerate
##  the binary when any source file changes, only the main one.
# However, there is no guarantee that the binary will exist, so dependence
# on the source is appropriate, and help2man is cheap anyway.

# Use -o so that the `missing' program can infer the output file.

poke.1: $(top_builddir)/src/poke $(common_mandeps)
	POKEDATADIR=$(abs_top_srcdir)/src $(HELP2MAN) -p poke --name="The GNU extensible binary editor" \
		$(top_builddir)/src/poke -o $@
