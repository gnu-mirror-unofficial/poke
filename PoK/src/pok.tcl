# pok.tcl -- a GNU poke GUI Proof Of Koncept

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

# pok_process_cmd_line_args
#
# Process the arguments passed in the command line.

set pok_debug_mi_p 0

proc pok_process_cmd_line_args {} {

    global argv
    global argc

    global pok_debug_mi_p

    # Process command-line options
    foreach arg $argv {
        if {[string equal $arg "--debug-mi"]} {
            set pok_debug_mi_p 1
        }
    }
}

# pok_init
#
# Initialize PoK and launch the gui.

proc pok_init {} {
    pok_process_cmd_line_args
    pok_gui_init .pok
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

package require BWidget
package require json
package require json::write

# Source internal sources.
source src/pok-gui.tcl
source src/pok-poke.tcl

# Start!
pok_init
