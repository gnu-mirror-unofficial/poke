# pok-gui.tcl -- PoK gui widgets

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

    Dialog .about -side bottom -anchor e \
        -separator yes -title "About PoK" \
        -modal local

    .about add -text Ok \
        -text "Ok" \
        -command { destroy .about }

    ScrolledWindow .about.sw -scrollbar both -auto both
    text .about.sw.text
    .about.sw.text insert 1.0 {
Proof Of Koncept of a graphical interface for GNU poke.

PoK is a prototype that we use in order to explore the best ways for a
graphical tool to interact with poke.  It is also a good mean to make
sure that poke's machine interface (MI) is complete and effective.

It follows that eye-candy is not a priority for this prototype:
functionality prime.  If you want to write a proper graphical poke
editor, you are welcome to use PoK as a source of inspiration on what
kind of mechanisms and abstractions you want to provide, and on how
you can interact with poke via the MI.

Of course PoK may be useful to do some serious poking in a graphical
way, but if so, that's just by pure chance! ;)
    }
    .about.sw.text configure -state disabled
    .about.sw setwidget .about.sw.text
    pack .about.sw -side top

    .about draw 1
}

# pok_gui_create_mainmenu
#
# Create and populate the main menu.

proc pok_gui_create_mainmenu {} {

    set mainmenu {
        "&File" {} {} 0 {
            {separator}
            {command "&Exit PoK" {} "exit the application" {Ctrl e} -command pok_quit}
        }
        "&Help" {} {} 0 {
            {separator}
            {command "&About" {} "About PoK" {} -command pok_gui_about}
        }
    }

    return $mainmenu
}

# pok_gui_init WIDGET
#
# Initializes the PoK widgets, into WIDGET.

proc pok_gui_init {widget} {

    set mainframe $widget

    wm title . "PoK - GNU poke"

    # Create the mainframe.
    MainFrame $mainframe -separator both \
        -menu [pok_gui_create_mainmenu]

    $mainframe addindicator -text "GNU poke"
    $mainframe addindicator -textvariable poke_version

    # Create the edition text widget.
    set textframe [$mainframe getframe]
    pok_gui_create_scrolled_text $textframe

    pack $textframe -side top -fill both -expand true
    pack $mainframe -fill both -expand true
}
