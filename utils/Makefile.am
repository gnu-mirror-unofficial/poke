# GNU poke - utilities

# Copyright (C) 2020, 2021 Jose E. Marchesi

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

bin_SCRIPTS = pk-elfextractor pk-strings
CLEANFILES = $(bin_SCRIPTS)
EXTRA_DIST = pk-elfextractor.in pk-strings.in

do_subst = sed -e 's,[@]bindir[@],$(bindir),g'

pk-elfextractor: pk-elfextractor.in Makefile
	$(do_subst) < $(srcdir)/pk-elfextractor.in > pk-elfextractor
	chmod +x pk-elfextractor

pk-strings: pk-strings.in Makefile
	$(do_subst) < $(srcdir)/pk-strings.in > pk-strings
	chmod +x pk-strings
