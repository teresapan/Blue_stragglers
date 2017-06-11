# -*- tcl -*-
#
# An example use of SM's TK widgets
#
#
# Get a value from a slider
#
proc get_value {_value {min 0} {max 1} {tick 0.25} {vals ""} {scale 1} {win ""}} {
   upvar $_value value
   global dev done

   if [info exists value] {
      set val $value
   } elseif [info exists value($dev)] {
      set val $value($dev)
   } else {
      set val 0
   }
   foreach v "val min max tick" {
      set $v [expr [set $v]/(1.0*$scale)]
   }
   if {$vals != ""} {
      foreach v $vals {
	 if {$v > 0} { set sign 1 } else { set sign -1 }
	 lappend newvals [expr $sign*int(abs($v)/(1.0*$scale) + 0.5)]
      }
      set vals $newvals
   }

   if {$scale != 1} {
      set lab "Unscaled Value:"
   } else {
      set lab "Value:"
   }

   toplevel .popup -class Dialog
   frame .popup.frame
   button .popup.frame.ok -text OK -command "set done 1"
   button .popup.frame.quit -text Cancel -command "set done 0"
   label .popup.frame.setval_lab -text $lab
   entry .popup.frame.setval -width 8
   bind .popup.frame.setval <Return> {set done 1}
   scale .popup.slider -from $min -to $max -length 10c -orient horizontal \
       -showvalue 1 -tickinterval $tick
   .popup.slider set $val

   if {$vals != ""} {
      frame .popup.special
      foreach v $vals {
	 button .popup.special.$v -text $v -command ".popup.slider set $v"
	 pack .popup.special.$v -side left
      }
   }
   
   if {$scale != 1} {
      set iscale [expr int(1.0/$scale)]
      if {$iscale == 100} {
	 set lab "Percentage Values"
      } else {
	 set lab "Scaled $iscale"
      }
      
      label .popup.label -text $lab
      pack .popup.label
   }

   pack .popup.frame.ok .popup.frame.quit \
       .popup.frame.setval_lab .popup.frame.setval -side left
   if {$vals != ""} {
      pack .popup.special
   }
   pack .popup.slider
   pack .popup.frame
   #
   # grab the focus
   #
   set old_focus [focus]
   grab set .popup
   focus .popup				
   #
   # put up the dialog, wait for a result, and restore the focus
   #
   if {$win != ""} {
      set x [expr int([winfo rootx $win]+[winfo width $win]/2)]
      set y [expr int([winfo rooty $win]+[winfo height $win]/2)]
      wm geometry .popup +$x+$y
      wm transient .popup .
   }
   tkwait variable done
   focus $old_focus
   #
   # process the results
   #
   if $done {
      set val [.popup.frame.setval get]
      if {$val != ""} {
	 set val [expr $val/(1.0*$scale)]
      } else {
	 set val [.popup.slider get]
      }
   }
   set val [expr $scale*$val]
   
   destroy .popup
   return $val
}

#
# choose which SM widget is current
#
proc set_device {which} {
   global ctype dev

   .fsm$dev config -relief flat
   .fsm$which config -relief raised
   set dev $which

   if {![info exists ctype($dev)]} {
      set ctype($dev) "default"
   }
   set_ctype $ctype($dev)
}

#
# Set angle for current device
#
proc set_angle {name} {
   global dev angle

   set angle $name
   
   angle $angle
}

#
# Set ctype for current device
#
proc set_ctype {name} {
   global ctype dev
   global .ctype

   set ctype($dev) $name
   .sm$dev select; ctype $ctype($dev)

   if {$ctype($dev) == "default"} {
      set name white
   }
   .ctype config -fg $name -bg grey
}

#
# Set expand for current device
#
proc set_expand {name} {
   global dev expand

   set expand $name
   
   expand $expand
}

#
# Set ltype for current device
#
proc set_lweight {name} {
   global lweight

   set lweight $name
   
   lweight $lweight
}

#
# Set ltype for current device
#
proc set_ltype {name} {
   global ltype

   set ltype $name
   ltype $ltype
}

#
# Set ptype for current device
#
proc set_ptype {num style} {
   global ptype dev

   set ptype "$num $style"
   eval ptype $ptype
}
#
# Now start creating widgets
#
# A frame for all the buttons
#
[frame .buttonpanel] configure -relief ridge
#
# Select a device
#
frame .devs
[menubutton .dev -text "Device" -menu .dev.menu] \
    configure -relief raised
button .quit -text Quit -command "exit" -fg red
#
menu .dev.menu
.dev.menu add command -label "Top" -command "set_device 1"
.dev.menu add command -label "Side" -command "set_device 2"
button .angle -text Angle \
    -command {set_angle \
		  [get_value angle -180 180 45 {-90 -45 0 45 90} 1 .angle]}
button .expand -text Expand \
    -command {set_expand [get_value expand 0 10 1 {.5 1 2 3 4 5} 0.01 .expand]}
#
pack .quit .dev -in .devs -side left
pack .angle .expand -in .devs -side left
pack .devs  -in .buttonpanel -anchor w
#
# Control appearance of lines etc.
#
frame .types
[menubutton .ctype -text Ctype -menu .ctype.menu] \
    configure -relief raised
[menubutton .ltype -text Ltype -menu .ltype.menu] \
    configure -relief raised
button .lweight -text Lweight \
    -command {set_lweight [get_value lweight 0 10 1 {} 1 .lweight]}
[menubutton .ptype -text Ptype -menu .ptype.menu] \
    configure -relief raised
#
menu .ctype.menu
.ctype.menu add command -label Default -command "set_ctype default"
.ctype.menu add command -label Black -command "set_ctype black"
.ctype.menu add command -label White -command "set_ctype white"
.ctype.menu add command -label Red -command "set_ctype red"
.ctype.menu add command -label Green -command "set_ctype green"
.ctype.menu add command -label Blue -command "set_ctype blue"
.ctype.menu add command -label Yellow -command "set_ctype yellow"
.ctype.menu add command -label Cyan -command "set_ctype cyan"
.ctype.menu add command -label Magenta -command "set_ctype magenta"
#
menu .ltype.menu
.ltype.menu add command -label solid -command "set_ltype 0"
.ltype.menu add command -label dots -command "set_ltype 1"
.ltype.menu add command -label "short dashes" -command "set_ltype 2"
.ltype.menu add command -label "long dashes" -command "set_ltype 3"
.ltype.menu add command -label "dot - short dashes" -command "set_ltype 4"
.ltype.menu add command -label "dot - long dashes" -command "set_ltype 5"
.ltype.menu add command -label "short - long dashes" -command "set_ltype 6"
#
# A proc to create cascaded menus for ptypes
#
proc add_ptype_menu n {
   menu .ptype.menu.pt$n
   .ptype.menu.pt$n add command -label open -command "set_ptype $n 0"
   .ptype.menu.pt$n add command -label skeletal -command "set_ptype $n 1"
   .ptype.menu.pt$n add command -label starred -command "set_ptype $n 2"
   .ptype.menu.pt$n add command -label filled -command "set_ptype $n 3"

   return .ptype.menu.pt$n
}
menu .ptype.menu
.ptype.menu add cascade -label "3 corners" -menu [add_ptype_menu 3]
.ptype.menu add cascade -label "4 corners" -menu [add_ptype_menu 4]
.ptype.menu add cascade -label "5 corners" -menu [add_ptype_menu 5]
.ptype.menu add cascade -label "6 corners" -menu [add_ptype_menu 6]
.ptype.menu add cascade -label "7 corners" -menu [add_ptype_menu 7]
.ptype.menu add cascade -label "8 corners" -menu [add_ptype_menu 8]
.ptype.menu add cascade -label "circle" -menu [add_ptype_menu 20]
#
pack .ctype .lweight .ltype .ptype -in .types -side left
pack .types -in .buttonpanel -anchor w
#
# Actually do things
#
frame .actions
button .box -text Box -command {.sm$dev select; test box}
[menubutton .tests -text Tests -menu .tests.menu] configure -relief raised
button .erase -text Erase -command {.sm$dev select; erase}
#
menu .tests.menu
.tests.menu add command -label Cross -command {.sm$dev select; test cross}
.tests.menu add command -label Dot -command {.sm$dev select; test dot}
.tests.menu add command -label Square -command {.sm$dev select; test square}
#
pack .box .tests .erase -in .actions -side left
pack .actions -in .buttonpanel -anchor w
#
# Actual plotting widgets
#
[frame .fsm1] configure -relief flat
[sm .sm1] configure -xsize 400 -ysize 400
pack .sm1 -in .fsm1 -fill both -expand true 

[frame .fsm2] configure -relief flat
[sm .sm2] configure -xsize 200 -ysize 200
pack .sm2 -in .fsm2 -fill both -expand true 
#
pack .fsm1 -fill both -expand true -pady 2m
pack .fsm2 -fill both -expand true -pady 2m -side left
pack .buttonpanel
#
# Select defaults
#
set dev 1; set_device $dev
