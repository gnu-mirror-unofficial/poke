# Copyright (C) 2019, 2020 Jose E. Marchesi

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


info_TEXINFOS = poke.texi
poke_TEXINFOS = fdl.texi


# This rule generates a list of the node names from the manual.
# It then substitutes spaces with '/' which is a kludge used to
# work around some limitations of readline.
# Look for the macro SPACE_SUBSTITUTE in pk-repl.c to see how this
# is used.
$(srcdir)/nodelist: $(srcdir)/poke.info
	$(SED) -n -e 's/^\* \([^:]*\)::.*/\1/p' $< | $(SED) -e 's| |/|g' > $@

INFO_DEPS = $(srcdir)/nodelist $(srcdir)/poke.info
