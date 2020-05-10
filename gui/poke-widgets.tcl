# poke-widgets.tcl -- Interface widget facilities

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

# pok_gui_select_file
#
# Pop up a file selection widget and ask for a file.

proc pok_gui_open_file {} {

    set f [tk_getOpenFile -initialdir "." \
               -title "Open a file"]

    switch $f {
        "" {
            # Do nothing
            return
        }
        default {
            # XXX open the file
            # pok_open
        }
    }
}

# pok_gui_fatal MSG
#
# Report MSG as a fatal error in a dialog and terminate the process
# immediately.

proc pok_gui_fatal {msg} {

    tk_messageBox -type ok -icon error \
        -message "fatal error: $msg"
    exit
}

# pok_gui_create_scrolled_text FRAME ARGS
#
# Create a scrolled text widget

proc pok_gui_create_scrolled_text {widget args} {

    # Create the text widget
    eval {text ${widget}.text \
	      -xscrollcommand [list ${widget}.xscroll set] \
	      -yscrollcommand [list ${widget}.yscroll set] \
	      -borderwidth 0 -state disabled} $args

    # Create the scroll widgets
    scrollbar ${widget}.xscroll -orient horizontal \
	-command [list ${widget}.text xview]
    scrollbar ${widget}.yscroll -orient vertical \
	-command [list ${widget}.text yview]

    # Pack the widgets
    grid ${widget}.text ${widget}.yscroll -sticky news
    grid ${widget}.xscroll -sticky ew
    grid rowconfigure ${widget} 0 -weight 1
    grid columnconfigure ${widget} 0 -weight 1
}

# pok_gui_about
#
# Show an "about" window.

proc pok_gui_about {} {

    toplevel .about

    frame .about.f

    text .about.f.text
    .about.f.text insert 1.0 {
GNU poke is an interactive, extensible editor for binary data.  Not
limited to editing basic entities such as bits and bytes, it provides
a full-fledged procedural, interactive programming language designed
to describe data structures and to operate on them.
    }
    .about.f.text configure -state disabled
    pack .about.f.text -side top -fill both -expand true

    button .about.f.ok -text Ok -command {destroy .about}
    pack .about.f.ok -side bottom
    pack .about.f -expand true -fill both

    focus .about
    grab .about
    tkwait window .about
}

# pok_gui_create_mainmenu
#
# Create and populate the main menu.

proc pok_gui_create_mainmenu {} {

    set m .menu

    menu ${m}
    . config -menu ${m}

    # File menu
    menu ${m}.file
    ${m}.file add command -label "Exit" -command pok_quit

    # Help menu
    menu ${m}.help
    ${m}.help add command -label "About poke" -command pok_gui_about

    ${m} add cascade -label File -menu ${m}.file
    ${m} add cascade -label Help -menu ${m}.help
}

# pok_gui_init
#
# Create the graphical user interface.

proc pok_gui_init {} {

    set mainframe .

    wm title . "GNU poke"

    # Create the main menu
    pok_gui_create_mainmenu

    # Create the edition text widget.
    frame .textframe
    pok_gui_create_scrolled_text .textframe
    pack .textframe -side top -fill both -expand true

    # Create the indicators bar.
    frame .ibar
    label .ibar.gnu_poke -relief sunken -text "GNU poke"  -padx 2
    label .ibar.poke_version -relief sunken -textvariable poke_version -padx 2
    pack .ibar.poke_version -side right
    pack .ibar.gnu_poke -side right
    pack .ibar -side bottom -fill x -expand false
}
