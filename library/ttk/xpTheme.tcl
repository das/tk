#
# $Id$
#
# Settings for 'xpnative' theme
#

namespace eval ttk::theme::xpnative {

    ttk::style theme settings xpnative {

	ttk::style configure . \
	    -background SystemButtonFace \
	    -foreground SystemWindowText \
	    -selectforeground SystemHighlightText \
	    -selectbackground SystemHighlight \
	    -font TkDefaultFont \
	    ;

	ttk::style map "." \
	    -foreground [list disabled SystemGrayText] \
	    ;

	ttk::style configure TButton -anchor center -padding {1 1} -width -11
	ttk::style configure TRadiobutton -padding 2
	ttk::style configure TCheckbutton -padding 2
	ttk::style configure TMenubutton -padding {8 4}

	ttk::style configure TNotebook -tabmargins {2 2 2 0}
	ttk::style map TNotebook.Tab \
	    -expand [list selected {2 2 2 2}]

	# Treeview:
	ttk::style configure Heading -font TkHeadingFont
	ttk::style configure Treeview -background SystemWindow
	ttk::style map Treeview \
	    -background [list selected SystemHighlight] \
	    -foreground [list selected SystemHighlightText] ;

	ttk::style configure TLabelframe.Label -foreground "#0046d5"

	# OR: -padding {3 3 3 6}, which some apps seem to use.
	ttk::style configure TEntry -padding {2 2 2 4}
	ttk::style map TEntry \
	    -selectbackground [list !focus SystemWindow] \
	    -selectforeground [list !focus SystemWindowText] \
	    ;
	ttk::style configure TCombobox -padding 2
	ttk::style map TCombobox \
	    -selectbackground [list !focus SystemWindow] \
	    -selectforeground [list !focus SystemWindowText] \
	    -foreground	[list {readonly focus} SystemHighlightText] \
	    -focusfill	[list {readonly focus} SystemHighlight] \
	    ;

	ttk::style configure TSpinbox -padding {2 0 14 0}
	ttk::style map TSpinbox \
	    -selectbackground [list !focus SystemWindow] \
	    -selectforeground [list !focus SystemWindowText] \
	    ;

	ttk::style configure Toolbutton -padding {4 4}

	# Vista requires some style modifications. There are some
	# additional or different elements in use.
	if {[tk windowingsystem] eq "win32"
	    && [info exists tcl_platform(osVersion)]
	    && $tcl_platform(osVersion) >= 6.0} {

	    ttk::style element create Combobox.field vsapi \
		COMBOBOX 2 {{} 1}
	    ttk::style element create Combobox.border vsapi \
		COMBOBOX 4 {disabled 4 focus 3 active 2 {} 1}
            ttk::style element create Combobox.rightdownarrow vsapi \
                COMBOBOX 6 {disabled 4 pressed 3 active 2 {} 1}
	    ttk::style layout TCombobox {
		Combobox.border -sticky nswe -border 0 -children {
		    Combobox.rightdownarrow -side right -sticky ns
		    Combobox.padding -expand 1 -sticky nswe -children {
			Combobox.focus -expand 1 -sticky nswe -children {
			    Combobox.textarea -sticky nswe
			}
		    }
		}
	    }
            ttk::style element create ComboboxPopdownFrame.background vsapi\
                COMBOBOX 2
            ttk::style layout ComboboxPopdownFrame {
                ComboboxPopdownFrame.background -sticky news -border 1 -children {
                    ComboboxPopdownFrame.padding -sticky news
                }
            }

            # EDIT EP_EDITBORDER_HVSCROLL
            ttk::style configure TSpinbox -padding {2 0 15 1}
            ttk::style element create Vista.Spinbox.field vsapi \
                EDIT 9 {disabled 4 focus 3 active 2 {} 1} \
                -padding {1 1 1 2}
            ttk::style layout TSpinbox {
                Vista.Spinbox.field -sticky nswe -children {
                    Spinbox.padding -sticky nswe -children {
                        Spinbox.textarea -expand 1 -sticky {}
                    }
                    Spinbox.uparrow -side top -sticky ens
                    Spinbox.downarrow -side bottom -sticky ens
                }
            }
	}
    }
}
