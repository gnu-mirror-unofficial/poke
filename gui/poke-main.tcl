# main.tcl -- main file for the poke GUI

# Main file.

# Copyright (C) 2020 Jose E. Marchesi

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see
# <http://www.gnu.org/licenses/>.

# pok_init
#
# Initialize PoK and launch the gui.

proc pok_init {} {
    pok_gui_init
    pok_start_poke
}

# pok_quit
#
# Exit PoK.

proc pok_quit {} {
    pok_shutdown_poke
    exit
}

# Load external packages.

package require json
package require json::write

# Load scripts.

source [file join $poke_guidir poke-mi.tcl]
source [file join $poke_guidir poke-mi-msg.tcl]
source [file join $poke_guidir poke-widgets.tcl]

# Start!
pok_init
