# Interesting commands that can be typed from the command line or entered
# via a command file:
#
# newVisi   example: set visiId [newVisi rthist]
# closeVisi example: closeVisi $visiId

# temporarily unimplemented (tough to make synchronous enough to return a result):
# addMetResVisi example: set mfpairs [addMetResVisi $visiId]
#    (adds whatever the current metric/foci selection(s) are, if any)
#    (returns a list of metric/focus ids)
# deleteMetResFromVisi $visiId $metricFocusId

#
# Sorry, no cmd to tell a visi to remove a curve, because it's not in the
# paradyn --> visi igen interface like add is.  Go figure.  The closest equivalent
# is closeVisi.
#
# whereAxis clear
# whereAxis select modulename fnname
# whereAxis unselect modulename fnname
# whereAxis select addr
# 
# metricsClear
# metricsSelect metricname
# metricsUnselect metricname
#
# 

set developerMode 0

set helv14bold "Helvetica 13 bold"
set helv12 "Helvetica 11"
set helv12bold "Helvetica 11 bold"
set helv10 "Helvetica 10"
set helv9 "Helvetica 9"
set courier14bold "Courier 14 bold"
set courier12bold "Courier 12 bold"

set whereAxisRootItemFontName $helv14bold
set whereAxisListboxItemFontName $helv12

# This works at Wisconsin when linking with X11R6:
#set cornflowerBlueColor "CornflowerBlue"

# This works at Wisconsin when linking with X11R6:
set cornflowerBlueColor "CornflowerBlue"
set pinkColor "pink"
#set pinkColor "darkred"

# But this is needed when linking with X11R4:
#set cornflowerBlueColor "blue"
#set cornflowerBlueColor "Wheat"

# Packers colors - this is wisconsin :)
#set cornflowerBlueColor "Gold"
#set pinkColor "ForestGreen"

# Or perhaps you prefer UW colors
#set cornflowerBlueColor "white"
#set pinkColor "DarkRed"

proc getFocusInstructionNum {} {
    global insn_num_entry_wname
    return [$insn_num_entry_wname get]
}

proc getFocusPids {} {
    global pids_entry_wname
    return [$pids_entry_wname get]
}

proc disassCreateCalleeButton {parent addr name} {
    # addr must be a *decimal*, not hex, address
    global numCalleeButtons helv12 helv12bold cornflowerBlueColor
    incr numCalleeButtons

    set wname "$parent.callee$numCalleeButtons"

    option add Kperfmon$wname.font $helv12bold

    menubutton $wname \
	    -padx 1 -pady 1 -relief raised -text $name \
	    -menu $wname.m

    menu $wname.m
    $wname.m add command -label "Disassemble" \
	    -command "disassFnByAddrFromMem $addr"
    # TODO -- shouldn't always be frommem; when appropriate, should be as orig. parsed
    $wname.m add command -label "Select" \
	    -command "whereAxis select 0x[format %x $addr]"
    $wname.m add command -label "Unselect" \
	    -command "whereAxis unselect 0x[format %x $addr]"

    return $wname
}

proc disassMakeAliasesGUI {wname aliases} {
    global helv12 helv12bold cornflowerBlueColor
    set num_aliases [llength $aliases]

    set pluralstr ""
    if {$num_aliases > 1} {
	set pluralstr "es"
    }

    set mb $wname.top.aliasesmb
#    option add Kperfmon$mb.font $helv12bold

    menubutton $mb -text "$num_aliases alias$pluralstr" -relief raised \
	    -background $cornflowerBlueColor -menu $wname.top.aliasesmb.m
    pack $mb -side right -fill y
    
    menu $mb.m

    for {set lcv 0} {$lcv < $num_aliases} {incr lcv} {
	set alias [lindex $aliases $lcv]
	$mb.m add command -label "$alias" -font $helv12
    }
}

proc disassMakeBasicBlocksGUI {wname bbs} {
    # bbs is a list of *hex* basic block addresses
    global helv12 helv12bold cornflowerBlueColor
    set num_bbs [llength $bbs]

    set pluralstr ""
    if {$num_bbs > 1} {
	set pluralstr "s"
    }

    set mb $wname.top.bbsmb
#    option add Kperfmon$bbsmb.font $helv12bold

    menubutton $mb -text "$num_bbs basic block$pluralstr" -relief raised \
	    -background $cornflowerBlueColor -menu $wname.top.bbsmb.m
    pack $mb -side right -fill y

    menu $mb.m
    
    for {set lcv 0} {$lcv < $num_bbs} {incr lcv} {
	set bbhexaddr [lindex $bbs $lcv]
	$mb.m add command -label "$bbhexaddr" -font $helv12
    }
}

proc disassMakeWindowCommon {} {
    global helv12 helv14bold courier14bold

    global numDisassWindows
    incr numDisassWindows

    set wname ".disass$numDisassWindows"

    toplevel $wname -class KperfmonDisassWinClass
    # one doesn't pack a toplevel window

    # The long version:
    #option add Tk.KperfmonDisassWinClass*Text.foreground green

    # This works too:
#    option add *KperfmonDisassWinClass*Text.foreground green

    #option add Kperfmon.KperfmonDisassWinClass*Text.foreground green

    frame $wname.top
    pack  $wname.top -side top -fill x

    set lab $wname.top.lab

    option add Kperfmon$lab.font $helv14bold

    label $lab \
	    -background red -foreground white \
	    -padx 7 -pady 1 \
	    -relief groove -anchor w
    pack  $lab -side left -fill both -expand true

    frame $wname.body
    pack  $wname.body -side top -fill both -expand true

    set txt $wname.body.txt
    option add Kperfmon$txt.font $courier14bold

    text  $txt \
	    -yscrollcommand "$wname.body.sb set" \
	    -width 50 -height 8 -wrap word
    pack  $txt -side left -fill both -expand true

    # Tag for register analysis info
    $txt tag configure regAnalTag -font $helv12 \
	    -lmargin1 10 -lmargin2 10

    scrollbar $wname.body.sb -relief sunken -troughcolor lightgrey \
	    -command "$txt yview"
    pack $wname.body.sb -side right -fill y

    return $wname
}

proc getMarkName {addr beforeInsn globalAnalysis isStartOfMark} {
    # args: address of instruction, flag (is before insn?), flag (doing global
    # analysis [true], or an individual instruction [false]), string ("markstart"
    # or "markend")

    # Mark name: "[before|after][addr]_reganal_[global|singleinsn]_[begin|end]

    if {[expr !$globalAnalysis && !$beforeInsn]} {
	error "getMarkName: if single insn (!globalAnalysis), then beforeInsn must be true, not false"
    }

    if {$beforeInsn==1} {
	set first "before"
    } elseif {$beforeInsn==0} {
	set first "after"
    } else {
	error "getMarkName: unknown beforeInsn value: $beforeInsn"
    }

    set second "$addr"
    set third "_reganal_"

    if {$globalAnalysis==1} {
	set fourth "global"
    } elseif {$globalAnalysis==0} {
	set fourth "singleinsn"
    } else {
	puts stderr "getMarkName: unknown globalAnalysisFlag: $globalAnalysis"
    }

    if {$isStartOfMark=="markstart"} {
	set fifth "_begin"
    } elseif {$isStartOfMark=="markend"} {
	set fifth "_end"
    } else {
	error "getMarkName: bad isStartOfMark value: $isStartOfMark"
    }

    return "$first$second$third$fourth$fifth"
}

# The following routine is called from C++ code:
proc createRegAnalysisMark {wname addr beforeInsn linenum globalAnalysis thisInsnAnalysis} {
    if {$globalAnalysis} {
	# Global analysis marker.  Goes before thisInsn marker, if any.
	set startMarkName [getMarkName $addr $beforeInsn 1 markstart]
	$wname mark set $startMarkName $linenum.0
	$wname mark gravity $startMarkName left
    }

    if {$thisInsnAnalysis} {
	# thisInsn marker.  Goes after global analysis marker, if any.
	set startMarkName [getMarkName $addr $beforeInsn 0 markstart]
	$wname mark set $startMarkName $linenum.0
	$wname mark gravity $startMarkName left
    }
}

# The following routine is called from C++ code:
proc insertRegAnalInfoForInsn {wname addr beforeInsn globalAnalysis the_text} {
    # globalAnalysis: if true, then doing global analysis, else just the effects of
    # this insn.

    set startMarkName [getMarkName $addr $beforeInsn $globalAnalysis markstart]
    set endMarkName [getMarkName $addr $beforeInsn $globalAnalysis markend]

    set otherStartMarkName [getMarkName $addr 1 [expr !$globalAnalysis] markstart]

    # Needed kludge: when inserting global analysis, change thisInsn's mark-start's
    # gravity from left to right before inserting text.  After text is inserted,
    # change it back to left.
    if {$globalAnalysis} {
	$wname mark gravity $otherStartMarkName right
    }

    # Create the end mark.  Give it right gravity as we're inserting code; change
    # its gravity to left when done inserting info.
    $wname mark set $endMarkName $startMarkName
    $wname mark gravity $endMarkName right

    # Insert desired text
    $wname insert $startMarkName "$the_text" regAnalTag

    # Needed kludge: when inserting global analysis, change thisInsn's mark-start's
    # gravity from left to right before inserting text.  After text is inserted,
    # change it back to left.
    if {$globalAnalysis} {
	$wname mark gravity $otherStartMarkName left
    }

    # The following line is ESSENTIAL, lest right gravity continue to move the
    # mark down as new text is inserted before the end!  Now that we've inserted
    # the desired text, we want the end mark to stay right where it is.  Changing
    # its gravity to left does just that.
    $wname mark gravity $endMarkName left
}

proc processRegAnalUnclick {wname addr beforeInsn globalAnalysis} {
    # globalAnalysis: if true, then doing global analysis, else just the effects of
    # this insn.
    set startMarkName [getMarkName $addr $beforeInsn $globalAnalysis markstart]
    set endMarkName [getMarkName $addr $beforeInsn $globalAnalysis markend]

    $wname delete $startMarkName $endMarkName

    set wname_button [getRegAnalButtonName $wname $addr $beforeInsn $globalAnalysis]
    $wname_button configure -text "+"
    $wname_button configure -command "processRegAnalClick $wname $addr $beforeInsn $globalAnalysis"
}

proc processRegAnalClick {wname addr beforeInsn globalAnalysis} {
    # globalAnalysis: if true, then doing global analysis, else just the effects of
    # this insn.

    if {[expr !$globalAnalysis && !$beforeInsn]} {
	error "processRegAnalClick: if single insn, then beforeInsn must be set"
    }

    reqRegAnalInfoForInsn $wname $addr $beforeInsn $globalAnalysis

    set wname_button [getRegAnalButtonName $wname $addr $beforeInsn $globalAnalysis]
    $wname_button configure -text "-"
    $wname_button configure -command "processRegAnalUnclick $wname $addr $beforeInsn $globalAnalysis"
}

proc getRegAnalButtonName {wname addr beforeInsn globalAnalysis} {
    # globalAnalysis: if true, then doing global analysis, else just the effects of
    # this insn.
    set first "$wname.$addr"
    set second "_$beforeInsn"
    if {$globalAnalysis} {
	set third "_global"
    } else {
	set third "_singleinsn"
    }

    return "$first$second$third"
}

# Helper fn for createRegAnalysisButton, below
# globalAnalysis: if true, then doing global analysis, else just the effects of
# this insn.
proc createRegAnalysisButton2 {wname addr beforeInsn globalAnalysis} {
    global helv9 cornflowerBlueColor
    global helv9 pinkColor
    button [getRegAnalButtonName $wname $addr $beforeInsn $globalAnalysis] \
	    -font $helv9 \
	    -padx 1 -pady -0 \
	    -background $cornflowerBlueColor -foreground $pinkColor \
	    -relief raised -text "+" \
	    -command "processRegAnalClick $wname $addr $beforeInsn $globalAnalysis"
}

proc createRegAnalysisButton {wname addr beforeInsn globalAnalysis} {
    # globalAnalysis: if true, then doing global analysis, else just the effects of
    # this insn.

    # Call this routine to create a button labeled "+" in a text widget named by $wname,
    # which if clicked on, will execute the command getRegAnalInfoForInsn, passing
    # it three arguments $wname $markname and $addr

    if {[expr !$beforeInsn && !$globalAnalysis]} {
	error "createRegAnalysisButton: beforeInsn and globalAnalysis cannot both be false"
    }
    $wname window create end -create "createRegAnalysisButton2 $wname $addr $beforeInsn $globalAnalysis" -stretch true
}

# ------------------ Disassemble Functions As Originally Parsed ---------------------

proc disassSelectedFnsAsParsed {} {
#    global analyzeWhenDisassembling

    set codes [getCurrSelectedCode]
    set num [llength $codes]

    for {set lcv 0} {$lcv < $num} {incr lcv} {
	set modAndFnAndBB [lindex $codes $lcv]

	set modulename [lindex $modAndFnAndBB 0]
	set functionaddr [lindex $modAndFnAndBB 1]
	set bbnum -1

	set ncomponents [llength $modAndFnAndBB]

	if {[llength $modAndFnAndBB] > 2} {
	    set bbnum [lindex $modAndFnAndBB 2]
	}

	if {$modulename == ""} {
	    puts stderr "Need to choose a function to disassemble"
	} elseif {$functionaddr == 0} {
	    puts stderr "Cannot disassemble an entire module ($modulename); choose a particular function within it"
	} else {
            # the following routine is implemented in a c++ file:
	    disassFnAsParsed $modulename $functionaddr $bbnum
	}
    }
}

# --------------------- Disassemble Functions From Memory -------------------------

proc disassSelectedFnsFromMem {} {
    set fns [getCurrSelectedCode]
    set num [llength $fns]

    for {set lcv 0} {$lcv < $num} {incr lcv} {
	set modAndFnAndBB [lindex $fns $lcv]

	set ncomponents [llength $modAndFnAndBB]

	set modulename [lindex $modAndFnAndBB 0]
	set functionaddr [lindex $modAndFnAndBB 1]

	set bbnum -1
	if {$ncomponents > 2} {
	    set bbnum [lindex $modAndFnAndBB 2]
	}

	if {$modulename == ""} {
	    puts stderr "Need to choose a function to disassemble"
	} elseif {$functionaddr == 0} {
	    puts stderr "Cannot disassemble an entire module ($modulename); choose a particular function within it"
	} else {
            # the following routine is implemented in a c++ file:
	    launchDisassFnFromMem $modulename $functionaddr $bbnum
	}
    }
}

proc disassFnByAddrFromMem {addr} {
   # addr must be a *decimal*, not hex function
   # like above routine, but takes in a function's start-address, instead of
   # fetching desired functions from a call to getCurrSelectedFns
   launchDisassFnByAddrFromMem $addr
}

proc disassRangeFromMem {isKernel} {
    # parameter: true iff kernel; false iff kerninstd

    global disass_range_from_entry_wname
    set startaddr_expr [$disass_range_from_entry_wname get]

    global disass_range_to_entry_wname
    set endaddr_expr   [$disass_range_to_entry_wname get]

    # expr always seems to return a value in decimal, not hex...
    set startaddr [expr "$startaddr_expr"]
    set endaddr   [expr "$endaddr_expr"]

    global disassIncludeAscii

    # ...and that's why we pass "decimal" as the 1st param in the following:
    launchDisassRangeFromMem $isKernel decimal $startaddr $endaddr $disassIncludeAscii
}

# --------------------------- Metrics -----------------------------

proc metricsClear {} {
    global isMetricSelected
    set num [getNumMetrics]
    for {set lcv 0} {$lcv < $num} {incr lcv} {
	set isMetricSelected($lcv) 0
    }
}

proc metricsSelectUnselect {name newval} {
    global isMetricSelected metricName
    set num [getNumMetrics]
    for {set lcv 0} {$lcv < $num} {incr lcv} {
	if {$metricName($lcv) == $name} {
	    set isMetricSelected($lcv) $newval
	}
    }
}

proc metricsSelect {name} {
    metricsSelectUnselect $name 1
}

proc metricsUnselect {name} {
    metricsSelectUnselect $name 0
}

proc getSelectedMetrics {} {
    # isMetricSelected$num is the global vrble representing whether a given metric
    # has been selected

    # initialize result to blank list
    lappend result

    # temp hack: # of metrics shouldn't be hardcoded
    set numMetrics [getNumMetrics]

    global isMetricSelected
    for {set lcv 0} {$lcv < $numMetrics} {incr lcv} {
	set val $isMetricSelected($lcv)

	if {$val == 1} {
	    lappend result $lcv
	}
    }

    return $result
}

proc changeInLocation {} {
    global location
    global insn_num_entry_wname

    if {$location == 2} {
 	$insn_num_entry_wname configure -state normal -relief sunken
    } else {
	$insn_num_entry_wname delete 0 end
	$insn_num_entry_wname configure -state disabled -relief flat
    }
}

# proc setPcrPicFromTclVars {} {
#     global pic0choice pic1choice
#     global pcr_user_flag pcr_system_flag pcr_priv_flag

#     setPcrCmd $pic0choice $pic1choice $pcr_user_flag $pcr_system_flag $pcr_priv_flag
# }

proc reSource {} {
    source ./kperfmon.tcl;
}

set commandCheckbuttonFont $helv12
set metricCheckbuttonFont $helv10
set bannerFont $helv14bold
set bannerFontColor "blue"

# proc updateOutliningBlockCountDelayReading {} {
#     # read out of the entry widget, update vrble "outliningBlockCountDelay"
#     global outliningBlockCountDelay

#     set outliningBlockCountDelay [.outlining.delayfr.delay get]
# }

proc cpuSetAll {numCPUs value} {
    global cpustate
    for {set i 0} {$i < $numCPUs} {incr i} {
	set cpustate($i) $value
	cpuSelectionChange $i $value
    }
}

proc initializeMultiCPUFrame {numCPUs} {
    global bannerFont helv10 helv12
    global bannerFontColor

    #
    # CPU part of the where axis
    #
    set cpuFrameName .cpu
    frame $cpuFrameName
    pack  $cpuFrameName -side top -fill x

    label $cpuFrameName.lab -text "CPUs" -font $bannerFont \
	    -foreground $bannerFontColor -relief raised
    pack  $cpuFrameName.lab -side top -fill x
    #
    # CPU axis body section
    #
    frame $cpuFrameName.mid -relief groove  -borderwidth 1
    pack  $cpuFrameName.mid -side top -fill x

    set stride 16
    set j 0
    set k 0
    for {set i 0} {$i < $numCPUs} {incr i} {
	set cpustate($i) 0
	set lab [format "cpu%2d" $i]
	checkbutton $cpuFrameName.mid.$i -text $lab -font $helv10 \
		-variable cpustate($i) \
		-command "cpuSelectionChange $i \$cpustate($i)"
	grid columnconfigure $cpuFrameName.mid $k -weight 1
	grid $cpuFrameName.mid.$i -row $j -column $k -sticky nsew
	incr k
	if {$k >= $stride} {
	    set k 0
	    incr j
	}
    }
    global cpustate_total
    # Select the total of all cpus by default --
    # adjust abstractions.C accordingly
    # Notice that we hardcode the value of CPU_TOTAL here (-2)
    set cpustate_total 1
    checkbutton $cpuFrameName.mid.total -text "total" -font $helv10 \
	    -variable cpustate_total \
	    -command "cpuSelectionChange -2 \$cpustate_total"
    grid columnconfigure $cpuFrameName.mid $k -weight 1
    grid $cpuFrameName.mid.total -row $j -column $k -sticky nsew 
    #
    # Buttons to select/unselect all CPUs at once
    #
    frame $cpuFrameName.all -relief groove  -borderwidth 1
    pack  $cpuFrameName.all -side top -fill x

    button $cpuFrameName.all.select -text "Select all CPUs" -font $helv12 \
	    -command "cpuSetAll $numCPUs 1" -pady 1
    pack  $cpuFrameName.all.select -side left -expand true

    button $cpuFrameName.all.clear -text "Clear all CPUs" -font $helv12 \
	    -command "cpuSetAll $numCPUs 0" -pady 1
    pack  $cpuFrameName.all.clear -side left -expand true
}

proc initializeKperfmonWindow {numCPUs} {
    global helv12 helv12bold helv14bold cornflowerBlueColor pinkColor
    wm title . "kperfmon"

    frame .top
    pack .top -side top -fill x

    frame .top.left
    pack .top.left -side left -fill both -expand true

    set kperfmon_version_string [get_kperfmon_version_string]
    set wtitle .top.left.title

    label $wtitle -relief raised \
	    -font $helv14bold \
	    -text "Kperfmon $kperfmon_version_string" \
	    -foreground $cornflowerBlueColor -background $pinkColor
    pack $wtitle -side top -fill both -expand true

    # .spec stands for 'specifier' -- module, fn name, where axis
    frame .spec
    pack  .spec -side top -fill both -expand true
       # expand true -- we want extra vertical or horizontal space due to window resize

    frame .spec.left
    pack  .spec.left -side left -fill y -expand false

    # .spec.right is for the where axis
    frame .spec.right
    pack  .spec.right -side left -fill both -expand true
       # -fill both: we'd like all the horiz & vert space available to us
       # (no need for an -expand true with a -fill both)

    # Metrics:
    frame .spec.left.metsframe
    pack  .spec.left.metsframe -side top -fill both -expand true

    frame .spec.left.metsframe.top
    pack  .spec.left.metsframe.top -side top -fill x -expand false

    global bannerFont
    global bannerFontColor

    set metsLabel .spec.left.metsframe.top.lab
    label $metsLabel -text "Metrics" \
	    -font $bannerFont \
	    -foreground $bannerFontColor -relief raised
    pack  $metsLabel -side top -fill x -expand false

    set metsLabel2 .spec.left.metsframe.top.lab2
    label $metsLabel2 -text "(Middle-click on a metric for details)" \
	    -font $helv12 \
	    -foreground blue -relief raised
    pack  $metsLabel2 -side top -fill x -expand false

    frame .spec.left.metsframe.bot
    pack  .spec.left.metsframe.bot -side top -fill both -expand true

    set metsTextWin .spec.left.metsframe.bot.metrics
    # In order to insert newlines (any text), the state of the text widget
    # needs to be "normal".  So we make it normal for now.  But after adding, we'll
    # change it to "disabled" to prevent insertion cursors from appearing there.
    text  $metsTextWin -height 12 -state normal -width 25 \
	    -yscrollcommand ".spec.left.metsframe.bot.sb set" \
	    -spacing3 8

    pack  $metsTextWin -side left -fill both -expand true

    set numMetrics [getNumMetrics]
    set metricClusterId 0
    for {set metnum 0} {$metnum < $numMetrics} {incr metnum} {
	set lastMetricClusterId $metricClusterId
	set metricClusterId [getMetricClusterId $metnum]

	if {$metricClusterId != $lastMetricClusterId} {
	    $metsTextWin insert end "\n"
	}

	set checkbuttonwin "$metsTextWin.$metnum"

	global metricCheckbuttonFont
	checkbutton $checkbuttonwin \
		-font $metricCheckbuttonFont \
		-relief raised \
		-text [getMetricName $metnum] \
		-selectcolor $cornflowerBlueColor -borderwidth 2 \
		-variable isMetricSelected($metnum) \
		-textvariable metricName($metnum)

	bind $checkbuttonwin <Button-2> "showMetricInfo $metnum"

	$metsTextWin window create end -align top -window $checkbuttonwin
    }
    $metsTextWin configure -state disabled

    scrollbar .spec.left.metsframe.bot.sb -relief sunken \
	    -troughcolor lightgrey \
	    -command ".spec.left.metsframe.bot.metrics yview"
    pack      .spec.left.metsframe.bot.sb -side right -fill y

    # Predicates:
    frame .spec.left.preds
    pack  .spec.left.preds -side top -fill x
       # no need for an -expand true with a -fill both

    frame .spec.left.preds.top
    pack  .spec.left.preds.top -side top -fill x
    
    label .spec.left.preds.top.lab -text "Predicates" \
        -font $bannerFont \
        -foreground $bannerFontColor -relief raised
    pack  .spec.left.preds.top.lab -side top -fill x

    frame .spec.left.preds.bot -relief ridge -borderwidth 2
    pack  .spec.left.preds.bot -side top -fill x
       # no need for an -expand true with a -fill both

    # Within predicates: pid
    frame .spec.left.preds.bot.pid
    pack  .spec.left.preds.bot.pid -side top -fill x -expand false

    label .spec.left.preds.bot.pid.lab -text "Pid(s):" -font $helv12bold
    pack  .spec.left.preds.bot.pid.lab -side left -fill x -expand false

    global pids_entry_wname
    set pids_entry_wname .spec.left.preds.bot.pid.en
    entry $pids_entry_wname -font $helv12
    pack  $pids_entry_wname -side left -fill x -expand false

    global commandCheckbuttonFont
    button .spec.left.preds.bot.pid.clear \
        -font $commandCheckbuttonFont \
        -text "Clear" -command {.spec.left.preds.bot.pid.en delete @0 end} \
        -pady 1
    pack   .spec.left.preds.bot.pid.clear -side right -expand false

    # ############## Function Modifiers (entry, exit, insn #) #################

    frame .spec.left.modifs

    frame .spec.left.modifs.top

    label .spec.left.modifs.top.lab -text "Function Modifiers" \
	    -font $bannerFont \
	    -foreground $bannerFontColor -relief raised

    frame .spec.left.modifs.row1

    frame .spec.left.modifs.row2

    radiobutton .spec.left.modifs.row1.rb1 -text "Fn entry" \
	    -variable location \
	    -value 0 -relief groove -command changeInLocation \
	    -font $helv12bold

    radiobutton .spec.left.modifs.row1.rb2 -text "Fn exit" \
	    -variable location \
	    -value 1 -relief groove -command changeInLocation \
	    -font $helv12bold

    radiobutton .spec.left.modifs.row2.rb3 -text "At Insn #" \
	    -variable location \
	    -value 2 -relief groove -command changeInLocation \
	    -font $helv12bold

    global location
    set location 0

    global insn_num_entry_wname
    set insn_num_entry_wname .spec.left.modifs.row2.en
    entry $insn_num_entry_wname -font $helv12 -width 3

    global developerMode
    if {$developerMode == 1} {
	pack  .spec.left.modifs -side top -fill x
	pack  .spec.left.modifs.top -side top -fill x
	pack  .spec.left.modifs.top.lab -side top -fill x
	pack .spec.left.modifs.row1 -side top -fill x
	pack .spec.left.modifs.row2 -side bottom -fill x
	pack .spec.left.modifs.row1.rb1 -side left -fill x -expand true
	pack .spec.left.modifs.row1.rb2 -side left -fill x -expand true
	pack .spec.left.modifs.row2.rb3 -side left -fill x
	pack  $insn_num_entry_wname -side left -fill x -expand false
    }
    
    changeInLocation
    
    # Where Axis:
    frame .spec.right.whereAxis
    pack  .spec.right.whereAxis -side left -fill both -expand true
       # Hmmm, this is one of the cases where an -expand true is useful, despite the
       # -fill both.

    frame .spec.right.whereAxis.code
    pack  .spec.right.whereAxis.code -side top -fill both -expand true

    frame .spec.right.whereAxis.code.top
    pack  .spec.right.whereAxis.code.top -side top -fill x

    label .spec.right.whereAxis.code.top.lab -text "Kernel Code" -font $bannerFont \
	    -foreground $bannerFontColor -relief raised
    pack  .spec.right.whereAxis.code.top.lab -side top -fill x

#    # where axis menus
#    frame .spec.right.whereAxis.code.menu -borderwidth 2 -relief raised
#    pack  .spec.right.whereAxis.code.menu -side top -fill x -expand false

#    menubutton .spec.right.whereAxis.code.menu.abstraction -text Abstraction \
#	    -font $helv12 \
#	    -menu .spec.right.whereAxis.code.menu.abstraction.m
#    menu .spec.right.whereAxis.code.menu.abstraction.m -selectcolor $cornflowerBlueColor

#    menubutton .spec.right.whereAxis.code.menu.navigate -text Navigate \
#	    -font $helv12 \
#	    -menu .spec.right.whereAxis.code.menu.navigate.m
#    menu .spec.right.whereAxis.code.menu.navigate.m -selectcolor $cornflowerBlueColor

#    pack .spec.right.whereAxis.code.menu.abstraction .spec.right.whereAxis.code.menu.navigate \
#	    -side left -padx 4

    # where axis body section, including scrollbars:

    frame .spec.right.whereAxis.code.mid
    pack  .spec.right.whereAxis.code.mid -side top -fill both -expand true
       # Hmmm, this is a case where -expand true is useful despite already
       # having a -fill both.

    scrollbar .spec.right.whereAxis.code.mid.sb -relief sunken -troughcolor lightgrey \
	    -orient vertical \
	    -command "whereAxisNewVertScrollPosition"
    pack      .spec.right.whereAxis.code.mid.sb -side right -fill y -expand false

    # the -borderwidth 5 and -relief groove is to help track down a GUI bug;
    # when you see the border then you know that our code for drawing on top
    # of this widget was unfortunately negated.
    frame .spec.right.whereAxis.code.mid.body -width 3i -height 3i -relief groove -borderwidth 5 -container true
    pack  .spec.right.whereAxis.code.mid.body -side top -fill both -expand true
       # Hmmm, this is a case where -expand true is useful despite already
       # having a -fill both.

    bind  .spec.right.whereAxis.code.mid.body <Button-1> "whereAxisLeftClick %x %y"
    bind  .spec.right.whereAxis.code.mid.body <Double-Button-1> "whereAxisDoubleClick %x %y"
    bind  .spec.right.whereAxis.code.mid.body <Control-Double-Button-1> "whereAxisCtrlDoubleClick %x %y"
    bind  .spec.right.whereAxis.code.mid.body <Button-2> "whereAxisMidClick %x %y"
    bind  .spec.right.whereAxis.code.mid.body <Alt-Motion> "whereAxisAltPress %x %y"
    bind  .spec.right.whereAxis.code.mid.body <Motion> "whereAxisAltRelease"
    # curiously, the "after idle" really does seem necessary; simply
    # doing Tcl_DoWhenIdle() from C++ code doesn't always have the desired
    # effect of making our drawing happen *after* the default frame redrawing.
    # Without "after idle", expose-only events wouldn't redraw properly (you'd
    # just get a frame widget with its 5-pixel border...a surefire giveaway that
    # our hook for redrawing unfortuantely took place before, and thus was
    # completely overwritten by, the usual redrawing for a tk frame widget)
    bind  .spec.right.whereAxis.code.mid.body <Configure> "after idle {whereAxisConfigure}"
    bind  .spec.right.whereAxis.code.mid.body <Expose> "after idle {whereAxisExpose %c}"
    bind  .spec.right.whereAxis.code.mid.body <Visibility> "after idle {whereAxisVisibility %s}"
    # tk wants us to clean up some things we allocated before Tk_MainLoop() completes;
    # the following gives a hook into that.
    bind  .spec.right.whereAxis.code.mid.body <Destroy> "whereAxisDestroyed"

    frame .spec.right.whereAxis.code.bot
    pack  .spec.right.whereAxis.code.bot -side top -fill x
    scrollbar .spec.right.whereAxis.code.bot.sb -relief sunken -troughcolor lightgrey \
	    -orient horizontal \
	    -command "whereAxisNewHorizScrollPosition"
    pack      .spec.right.whereAxis.code.bot.sb -side left -fill x -expand true

    if {$numCPUs > 1} {
	initializeMultiCPUFrame $numCPUs
    }

    frame .specmisc
    pack  .specmisc -side top -fill x

    global justTesting
#    set justTesting false
    set justTesting true

    checkbutton .specmisc.jt -text "Just testing (no launcher)" \
	    -variable justTesting -command {justTestingChange $justTesting} \
	    -font $helv12

    button .specmisc.resource -text "Re-source .tcl" \
	    -font $commandCheckbuttonFont \
	    -command reSource
	    
    global developerMode
    if {$developerMode == 1} {
	pack .specmisc.jt -side left -expand false
	pack .specmisc.resource -side left
    }
    # ##################

    frame .d
    pack .d -side top -fill x

    frame .d.range -borderwidth 4 -relief groove
    pack  .d.range -side left -fill both -expand true

    label .d.range.lab -text "Disassemble a range of memory" -font $helv12bold \
	    -foreground $pinkColor -relief groove
    pack  .d.range.lab -side top -fill x

    frame .d.range.bounds
    pack  .d.range.bounds -side left -fill both -expand true

    frame .d.range.bounds.top
    pack  .d.range.bounds.top -side top -fill x

    frame .d.range.bounds.top.from
    pack  .d.range.bounds.top.from -side left -fill x -expand true
    
    label .d.range.bounds.top.from.lab -text "From-addr:" \
	    -font $helv12 -foreground $pinkColor
    pack  .d.range.bounds.top.from.lab -side left -fill y

    global disass_range_from_entry_wname
    set disass_range_from_entry_wname .d.range.bounds.top.from.en
    entry  $disass_range_from_entry_wname -font $helv12 -width 15
    pack   $disass_range_from_entry_wname -side left -fill x -expand true

    frame .d.range.bounds.top.to
    pack  .d.range.bounds.top.to -side left -fill x -expand true
    
    label .d.range.bounds.top.to.lab -text "To-addr:" -font $helv12 \
	    -foreground $pinkColor
    pack  .d.range.bounds.top.to.lab -side left -fill y

    global disass_range_to_entry_wname
    set disass_range_to_entry_wname .d.range.bounds.top.to.en
    entry $disass_range_to_entry_wname -font $helv12 -width 15
    pack  $disass_range_to_entry_wname -side left -fill x -expand true

    frame .d.range.bounds.bot
    pack  .d.range.bounds.bot -side top -fill x -expand true

    button .d.range.bounds.bot.disass_kernel -text "Disassemble (kernel)" \
	    -font $commandCheckbuttonFont \
	    -command "disassRangeFromMem 1"
    pack   .d.range.bounds.bot.disass_kernel -side left -fill x -expand true

#    button .d.range.bounds.bot.disass_kerninstd -text "Disassemble (kerninstd)" \
#	    -font $commandCheckbuttonFont \
#	    -command "disassRangeFromMem 0"
#    pack   .d.range.bounds.bot.disass_kerninstd -side right -fill x -expand true

    frame .d.range.bounds.bot2
    pack  .d.range.bounds.bot2 -side bottom -fill x -expand true

    global disassIncludeAscii
    set disassIncludeAscii true

    checkbutton .d.range.bounds.bot2.ascii -text "include ascii in disassembly" \
	    -variable disassIncludeAscii -font $helv12
#    pack .d.range.bounds.bot2.ascii

    # ##################

#    frame .d.whereaxis
    frame .d.whereaxis -borderwidth 4 -relief groove
    pack  .d.whereaxis -side right -fill both -expand true

    label .d.whereaxis.lab -text "Disassemble Selected Fn/Block(s)" \
	    -font $helv12bold \
	    -foreground $pinkColor -relief groove
    pack  .d.whereaxis.lab -side top -fill x

#    frame .d.whereaxis.body -borderwidth 4 -relief groove
    frame .d.whereaxis.body
    pack  .d.whereaxis.body -side top -fill both -expand true

    button .d.whereaxis.body.disass1 -text "Disassemble (orig)" \
	    -font $commandCheckbuttonFont \
	    -command disassSelectedFnsAsParsed
    pack .d.whereaxis.body.disass1 -side left -fill x -expand true

    button .d.whereaxis.body.disass2 -text "Disassemble (curr mem)" \
	    -font $helv12 -command disassSelectedFnsFromMem
    pack .d.whereaxis.body.disass2 -side left -fill x -expand true

    # ##################

#     # See ultrasparc user's manual sec B.4 for nice descriptions of each event
#     global pic0choice pic1choice
#     global describePcrPicChoicesFlag
#     set describePcrPicChoicesFlag 0
#     global pcr_user_flag pcr_system_flag pcr_priv_flag

#     set doPicStuff 0

#     if {$doPicStuff} {
# 	frame .pic -borderwidth 4 -relief groove
# 	pack  .pic -side left

# 	label .pic.lab -text "UltraSparc %pic register settings" \
# 	    -font $helv12 \
# 	    -foreground $cornflowerBlueColor \
# 	    -relief groove
# 	pack .pic.lab -side top -fill x

# 	set pic_choose .pic.choose
# 	frame $pic_choose -borderwidth 4 -relief groove
# 	pack $pic_choose -side left -fill y

# 	menubutton $pic_choose.pic0mb -text "pic0" -font $helv12 \
# 	    -menu $pic_choose.pic0mb.m -relief raised
# 	pack $pic_choose.pic0mb -side top

# 	set pic0menu $pic_choose.pic0mb.m
# 	menu  $pic0menu -selectcolor $cornflowerBlueColor
# 	$pic0menu add radiobutton -label "Cycle_cnt (0x0)" \
# 	    -variable pic0choice -value 0 -command processTclHasChangedPcrPicSettings -font $helv12
# 	$pic0menu add radiobutton -label "Instr_cnt (0x1)" \
# 	    -variable pic0choice -value 1 -command processTclHasChangedPcrPicSettings -font $helv12
# 	$pic0menu add radiobutton -label "Dispatch0_IC_miss (0x2)" \
# 	    -variable pic0choice -value 2 -command processTclHasChangedPcrPicSettings -font $helv12
# 	$pic0menu add radiobutton -label "Dispatch0_storeBuf (0x3)" \
# 	    -variable pic0choice -value 3 -command processTclHasChangedPcrPicSettings -font $helv12
# 	$pic0menu add radiobutton -label "IC_ref (0x8)" \
# 	    -variable pic0choice -value 8 -command processTclHasChangedPcrPicSettings -font $helv12
# 	$pic0menu add radiobutton -label "DC_rd (0x9)" \
# 	    -variable pic0choice -value 9 -command processTclHasChangedPcrPicSettings -font $helv12
# 	$pic0menu add radiobutton -label "DC_wr (0xa)" \
# 	    -variable pic0choice -value 10 -command processTclHasChangedPcrPicSettings -font $helv12
# 	$pic0menu add radiobutton -label "Load_use (0xb)" \
# 	    -variable pic0choice -value 11 -command processTclHasChangedPcrPicSettings -font $helv12
# 	$pic0menu add radiobutton -label "EC_ref (0xc)" \
# 	    -variable pic0choice -value 12 -command processTclHasChangedPcrPicSettings -font $helv12
# 	$pic0menu add radiobutton -label "EC_write_hit_RDO (0xd)" \
# 	    -variable pic0choice -value 13 -command processTclHasChangedPcrPicSettings -font $helv12
# 	$pic0menu add radiobutton -label "EC_snoop_inv (0xe)" \
# 	    -variable pic0choice -value 14 -command processTclHasChangedPcrPicSettings -font $helv12
# 	$pic0menu add radiobutton -label "EC_rd_hit (0xf)" \
# 	    -variable pic0choice -value 15 -command processTclHasChangedPcrPicSettings -font $helv12

# 	menubutton $pic_choose.pic1mb -text "pic1" -font $helv12 \
# 	    -menu $pic_choose.pic1mb.m -relief raised
# 	pack $pic_choose.pic1mb -side top

# 	set pic1menu $pic_choose.pic1mb.m
# 	menu  $pic1menu -selectcolor $cornflowerBlueColor
# 	$pic1menu add radiobutton -label "Cycle_cnt (0x0)" \
# 	    -variable pic1choice -value 0 -command processTclHasChangedPcrPicSettings -font $helv12
# 	$pic1menu add radiobutton -label "Instr_cnt (0x1)" \
# 	    -variable pic1choice -value 1 -command processTclHasChangedPcrPicSettings -font $helv12
# 	$pic1menu add radiobutton -label "Dispatch0_mispred (0x2)" \
# 	    -variable pic1choice -value 2 -command processTclHasChangedPcrPicSettings -font $helv12
# 	$pic1menu add radiobutton -label "Dispatch0_FP_use (0x3)" \
# 	    -variable pic1choice -value 3 -command processTclHasChangedPcrPicSettings -font $helv12
# 	$pic1menu add radiobutton -label "IC_hit (0x8)" \
# 	    -variable pic1choice -value 8 -command processTclHasChangedPcrPicSettings -font $helv12
# 	$pic1menu add radiobutton -label "DC_rd_hit (0x9)" \
# 	    -variable pic1choice -value 9 -command processTclHasChangedPcrPicSettings -font $helv12
# 	$pic1menu add radiobutton -label "DC_wr_hit (0xa)" \
# 	    -variable pic1choice -value 10 -command processTclHasChangedPcrPicSettings -font $helv12
# 	$pic1menu add radiobutton -label "Load_use_RAW (0xb)" \
# 	    -variable pic1choice -value 11 -command processTclHasChangedPcrPicSettings -font $helv12
# 	$pic1menu add radiobutton -label "EC_hit (0xc)" \
# 	    -variable pic1choice -value 12 -command processTclHasChangedPcrPicSettings -font $helv12
# 	$pic1menu add radiobutton -label "EC_wb (0xd)" \
# 	    -variable pic1choice -value 13 -command processTclHasChangedPcrPicSettings -font $helv12
# 	$pic1menu add radiobutton -label "EC_snoop_cb (0xe)" \
# 	    -variable pic1choice -value 14 -command processTclHasChangedPcrPicSettings -font $helv12
# 	$pic1menu add radiobutton -label "EC_ic_hit (0xf)" \
# 	    -variable pic1choice -value 15 -command processTclHasChangedPcrPicSettings -font $helv12

# 	checkbutton $pic_choose.descr -text "describe" \
# 	    -font $helv12 -command pcrPicChoiceChangeCmd \
# 	    -variable describePcrPicChoicesFlag
# 	pack $pic_choose.descr -side left -fill y

# 	# Note that the variables pcr_user_flag, etc. have already been set in
# 	# initializeKperfmon
# 	set pic_choose_flags .pic.chooseflags
# 	frame $pic_choose_flags
# 	pack $pic_choose_flags -side left -fill y

# 	checkbutton $pic_choose_flags.ut -text "user" -variable pcr_user_flag \
# 	    -font $helv12 -command processTclHasChangedPcrPicSettings -anchor w -relief ridge
# 	pack        $pic_choose_flags.ut -side top -fill x

# 	checkbutton $pic_choose_flags.st -text "system" -variable pcr_system_flag \
# 	    -font $helv12 -command processTclHasChangedPcrPicSettings -anchor w -relief ridge
# 	pack        $pic_choose_flags.st -side top -fill x 

# 	checkbutton $pic_choose_flags.priv -text "priv" -variable pcr_priv_flag \
# 	    -font $helv12 -command processTclHasChangedPcrPicSettings -anchor w -relief ridge
# 	pack        $pic_choose_flags.priv -side top -fill x

# 	set pic_misc .pic.misc
# 	frame $pic_misc
# 	pack $pic_misc -side left -fill y
    
# 	button $pic_misc.getpicraw -text "Get" \
# 	    -font $commandCheckbuttonFont \
# 	    -command getPicRawCmd
# 	pack   $pic_misc.getpicraw -side top -fill x
# 	button $pic_misc.clearpic -text "Clear" \
# 	    -font $commandCheckbuttonFont \
# 	    -command clearPicCmd
# 	pack   $pic_misc.clearpic -side top -fill x
#     }

#     global helv10
#     set outliningCheckbuttonFont $helv10

#     frame .outlining

#     global outliningDoReplaceFunction
#     checkbutton .outlining.doReplFunc -font $outliningCheckbuttonFont \
# 	    -relief raised \
# 	    -text "do code replacement" \
# 	    -selectcolor $cornflowerBlueColor \
# 	    -borderwidth 2 \
# 	    -variable outliningDoReplaceFunction

#     global outliningReplaceFunctionPatchUpCallSitesToo
#     checkbutton .outlining.replFuncCallSitesToo \
# 	    -font $outliningCheckbuttonFont \
# 	    -relief raised \
# 	    -text "replace call sites too" \
# 	    -selectcolor $cornflowerBlueColor \
# 	    -borderwidth 2 \
# 	    -variable outliningReplaceFunctionPatchUpCallSitesToo

#     frame .outlining.delayfr

#     label .outlining.delayfr.lab \
# 	    -text "delay:" \
# 	    -font $helv10

#     entry .outlining.delayfr.delay \
# 	    -font $helv10 \
# 	    -borderwidth 2

#     button .outlining.delayfr.apply \
# 	    -font $helv10 \
# 	    -text "apply" \
# 	    -command updateOutliningBlockCountDelayReading

#     global outlineChildrenToo
#     checkbutton .outlining.outlineChildrenToo \
# 	    -font $outliningCheckbuttonFont \
# 	    -relief raised \
# 	    -text "outline children too" \
# 	    -selectcolor $cornflowerBlueColor \
# 	    -borderwidth 2 \
# 	    -variable outlineChildrenToo

#     global forceOutliningOfAllChildren
#     checkbutton .outlining.forceOutliningOfAllChildren \
# 	    -font $outliningCheckbuttonFont \
# 	    -relief raised \
# 	    -text "force outlining of all children" \
# 	    -selectcolor $cornflowerBlueColor \
# 	    -borderwidth 2 \
# 	    -variable forceOutliningOfAllChildren

#     global outliningUsingFixedDescendants
#     checkbutton .outlining.outliningUsingFixedDescendants \
# 	    -font $outliningCheckbuttonFont \
# 	    -relief raised \
# 	    -text "use fixedOutlineDescendants vrble" \
# 	    -selectcolor $cornflowerBlueColor \
# 	    -borderwidth 2 \
# 	    -variable outliningUsingFixedDescendants

#     global outliningBlockPlacement

#     menubutton .outlining.placement \
# 	    -borderwidth 2 -relief raised \
# 	    -indicatoron 1 \
# 	    -text "block positioning" \
# 	    -menu .outlining.placement.m

#     menu .outlining.placement.m \
# 	    -selectcolor $cornflowerBlueColor
#     .outlining.placement.m add radiobutton \
# 	    -font $helv12 \
# 	    -label "origSeq" -variable outliningBlockPlacement \
# 	    -value "origSeq"
#     .outlining.placement.m add radiobutton \
# 	    -font $helv12 \
# 	    -label "chains" -variable outliningBlockPlacement \
# 	    -value "chains"

#     menubutton .outlining.hotprefs \
# 	    -borderwidth 2 -relief raised \
# 	    -indicatoron 1 \
# 	    -text "Hot Block Threshold" \
# 	    -menu .outlining.hotprefs.m

#     global outliningHotBlockThresholdChoice
#     menu .outlining.hotprefs.m \
# 	    -selectcolor $cornflowerBlueColor
#     .outlining.hotprefs.m add radiobutton \
# 	    -font $helv12 \
# 	    -label "Any executed block" -variable outliningHotBlockThresholdChoice \
# 	    -value "AnyNonZeroBlock"
#     .outlining.hotprefs.m add radiobutton \
# 	    -font $helv12 \
# 	    -label "5% as often as root fn called" \
# 	    -variable outliningHotBlockThresholdChoice \
# 	    -value "FivePercent"

#     global developerMode
#     if {$developerMode == 1} {
# 	pack  .outlining -side left -fill x
# 	pack .outlining.doReplFunc -side top -fill x
# 	pack .outlining.replFuncCallSitesToo -side top -fill x
# 	pack  .outlining.delayfr -side top -fill x
# 	pack .outlining.delayfr.lab -side left -fill y
# 	pack .outlining.delayfr.delay -side left -fill both -expand true
# 	pack .outlining.delayfr.apply -side right -fill y
# 	pack .outlining.outlineChildrenToo -side top -fill x
# 	pack .outlining.forceOutliningOfAllChildren -side top -fill x
# 	pack .outlining.outliningUsingFixedDescendants -side top -fill x
# 	pack .outlining.placement -side top -fill x
# 	pack .outlining.hotprefs -side top -fill x
#     }
    # ----------------------------------------------------------------------

    # cbs stands for 'command buttons'
    frame .cbs3
    pack  .cbs3 -side left -fill x

    menubutton .cbs3.visi -text "Start a visi" \
	    -font $helv12bold \
	    -menu .cbs3.visi.m -relief raised
    pack .cbs3.visi -side top -fill x
    menu .cbs3.visi.m
    .cbs3.visi.m add command -label "Histogram" -command "newVisi rthist" \
	    -font $helv12
    .cbs3.visi.m add command -label "Barchart" -command "newVisi barChart" \
	    -font $helv12
    .cbs3.visi.m add command -label "Table" -command "newVisi tableVisi" \
	    -font $helv12

#    button .cbs3.gettickraw -text "Get tick" \
#	    -font $helv12bold -command getTickRawCmd
#    pack   .cbs3.gettickraw -side left
	    
    button .cbs3.callonce -text "Call once" \
	    -font $commandCheckbuttonFont \
	    -command callOnceCmd

#     menubutton .cbs3.testvirtualization -text "Test" \
# 	    -font $helv12 \
# 	    -menu .cbs3.testvirtualization.m -relief raised

#     menu   .cbs3.testvirtualization.m
#     .cbs3.testvirtualization.m add command -label "virtualization all_vtimers\[\]" \
# 	    -font $helv12 \
# 	    -command "test virtualization all_vtimers"
#     .cbs3.testvirtualization.m add command -label "virtualization stacklist heap" \
# 	    -font $helv12 \
# 	    -command "test virtualization stacklist"
#     .cbs3.testvirtualization.m add command -label "virtualization hash table" \
# 	    -font $helv12 \
# 	    -command "test virtualization hashtable"
#     .cbs3.testvirtualization.m add command -label "virtualization full ensemble" \
# 	    -font $helv12 \
# 	    -command "test virtualization fullensemble"
#     .cbs3.testvirtualization.m add separator
#     .cbs3.testvirtualization.m add command -label "outlining" \
# 	    -font $helv12 \
# 	    -command "test outlining"

    global developerMode
    if {$developerMode == 1} {
	pack   .cbs3.callonce -side top -fill x
# 	pack   .cbs3.testvirtualization -side top -fill x
    }
}

# proc setTclPcrPicVrbleSettingsFromKernel {} {
#     global pic0choice pic1choice pcr_user_flag pcr_system_flag pcr_priv_flag
#     set initial_pcr_settings [getCurrPcrSettings]
#     set pic0choice [lindex $initial_pcr_settings 0]
#     set pic1choice [lindex $initial_pcr_settings 1]
#     set pcr_user_flag [lindex $initial_pcr_settings 2]
#     set pcr_system_flag  [lindex $initial_pcr_settings 3]
#     set pcr_priv_flag [lindex $initial_pcr_settings 4]
# }

proc initializeKperfmon {haveGUI numCPUs} {
    global numDisassWindows
    set numDisassWindows 0

    global numCalleeButtons
    set numCalleeButtons 0

    if {$haveGUI} {
	initializeKperfmonWindow $numCPUs
    }

#     # Obtain the current %pcr settings from the kerninstd machine:
#     setTclPcrPicVrbleSettingsFromKernel

#     global pic0choice pic1choice
#     global pcr_user_flag pcr_system_flag pcr_priv_flag

#     # Make sure that pcr_priv_flag is off, and if both user&system flag are off then
#     # turn them both on
#     set pcr_priv_flag 0
#     if {!$pcr_user_flag && !$pcr_system_flag} {
# 	set pcr_user_flag 1
# 	set pcr_system_flag 1
#     }

#     # ...and make these (possibly changed) settings take place on the kerninstd machine:
#     setPcrPicFromTclVars

#     # ---------------------------------------------------------------------- 

#     global shortPic0Desc longPic0Desc shortPic1Desc longPic1Desc
#     set shortPic0Desc(0) "Cycle_cnt (0x0)"
#     set shortPic0Desc(1) "Instr_cnt (0x1)"
#     set shortPic0Desc(2) "Dispatch0_IC_miss (0x2)"
#     set shortPic0Desc(3) "Dispatch0_storeBuf (0x3)"
#     set shortPic0Desc(8) "IC_ref (0x8)"
#     set shortPic0Desc(9) "DC_rd (0x9)"
#     set shortPic0Desc(10) "DC_wr (0xa)"
#     set shortPic0Desc(11) "Load_use (0xb)"
#     set shortPic0Desc(12) "EC_ref (0xc)"
#     set shortPic0Desc(13) "EC_write_hit_RDO (0xd)"
#     set shortPic0Desc(14) "EC_snoop_inv (0xe)"
#     set shortPic0Desc(15) "EC_rd_hit (0xf)"

#     set longPic0Desc(0) "Accumulated cycles"
#     set longPic0Desc(1) "# insns completed.  Note: annulled, mispredicted, trapped instructions are not counted"
#     set longPic0Desc(2) "# cycles where pipeline is stalled waiting to handle an I-$ miss with an empty I-buffer.  Note: this includes E-$ miss processing, should an E-$ miss also occur."
#     set longPic0Desc(3) "# cycles where pipeline is stalled because a store insn -- first in the group -- cannot proceed because the store buffer is full"
#     set longPic0Desc(8) "# of I-$ references.  Note: I-$ refs are fetches of <= 4 insns from an aligned block of 8 insns.  Also note that I-$ refs are usually prefetches, and thus do not exactly correspond to insns executed"
#     set longPic0Desc(9) "# of D-$ read references, including those that will trap.  Non-D-cacheable accesses are not included."
#     set longPic0Desc(10) "# of D-$ write references, including those that will trap.  Non-D-cacheable accesses are not included."
#     set longPic0Desc(11) "# cycles where pipeline is stalled because an insn in execute stage depends on an earlier load that is not yet available.  Note: also counts cycles where no insns are dispatched due to a 1-cycle load-load dependency on the 1st insn presented to grouping logic"
#     set longPic0Desc(12) "# E-$ refs.  Non-cacheable accesses are not counted."
#     set longPic0Desc(13) "# E-$ hits that do a read-for-ownership UPA xaction."
#     set longPic0Desc(14) "# E-$ invalidates from one of several UPA xactions."
#     set longPic0Desc(15) "# E-$ read hits from D-$ misses"

#     # ----------------------------------------------------------------------

#     set shortPic1Desc(0) "Cycle_cnt (0x0)"
#     set shortPic1Desc(1) "Instr_cnt (0x1)"
#     set shortPic1Desc(2) "Dispatch0_mispred (0x2)"
#     set shortPic1Desc(3) "Dispatch0_FP_use (0x3)"
#     set shortPic1Desc(8) "IC_hit (0x8)"
#     set shortPic1Desc(9) "DC_rd_hit (0x9)"
#     set shortPic1Desc(10) "DC_wr_hit (0xa)"
#     set shortPic1Desc(11) "Load_use_RAW (0xb)"
#     set shortPic1Desc(12) "EC_hit (0xc)"
#     set shortPic1Desc(13) "EC_wb (0xd)"
#     set shortPic1Desc(14) "EC_snoop_cb (0xe)"
#     set shortPic1Desc(15) "EC_ic_hit (0xf)"

#     set longPic1Desc(0) $longPic0Desc(0)
#     set longPic1Desc(1) $longPic0Desc(1)
#     set longPic1Desc(2) "# cycles where pipeline is stalled waiting for a mispredicted branch with an empty I-buffer.  Note: a misprediction kills instructions after the dispatch point, so the total number of pipeline bubbles is appx twice as big as this count"
#     set longPic1Desc(3) "# cycles where pipeline is stalled because 1st insn in group depends on an earlier FP result that is not yet available.  Exception: if the earlier insn is stalled for a load_use, then the times is accumulated in the Load_use PIC0 metric, not here."
#     set longPic1Desc(8) "# of I-$ hits"
#     set longPic1Desc(9) "# of D-$ read hits, from one of 2 places: (1) access of D-$ tags without entering load buffer (since it's already empty), and (2) exit of load buffer due to D-$ miss or a non-empty load buffer.  Note that loads that hit in the D-$ may be placed into the load buffer yet later turn into misses, if a snoop occurs during that time (e.g. E-$ miss).  Such cases are not counted as D-$ read hits."
#     set longPic1Desc(10) "# of D-$ write hits"
#     set longPic1Desc(11) "# cycles stalled where an insn in execute stage uses the result of a load, but the load data has to wait for completion of an earlier store (R-A-W hazard)"
#     set longPic1Desc(12) "# E-$ hits"
#     set longPic1Desc(13) "# E-$ misses that do writebacks"
#     set longPic1Desc(14) "# E-$ snoop copy-backs from one of several UPA xactions"
#     set longPic1Desc(15) "# E-$ read hits from I-$ misses"
}

# proc pcrPicChoiceChangeCmd {} {
#     global describePcrPicChoicesFlag

#     if {$describePcrPicChoicesFlag} {
# 	createPcrPicChoicesArea
# 	fillPcrPicChoices
#     } else {
# 	hidePcrPicChoicesArea
#     }
# }

# proc updatePcrPicGUIAfterChange {} {
#     global describePcrPicChoicesFlag

#     if {$describePcrPicChoicesFlag} {
# 	fillPcrPicChoices
#     }
# }

# proc processTclHasChangedPcrPicSettings {} {
#     # Call this when tk code wants to change the contents of pcr/pic
#     updatePcrPicGUIAfterChange
#     setPcrPicFromTclVars
# }

# proc updatePcrPicGUIFromKernelSettings {} {
#     # Call this (from C++ code) when c++ code has modified the pcr/pic settings,
#     # and would like to ensure that the GUI gets updated, too
#     setTclPcrPicVrbleSettingsFromKernel
#     updatePcrPicGUIAfterChange
# }

# proc createPcrPicChoicesArea {} {
#     global helv12

#     frame .cbs5
#     pack  .cbs5 -side bottom -fill x

#     text .cbs5.text -font $helv12 -height 3 -wrap word \
# 	    -yscrollcommand ".cbs5.sb set"
#     pack .cbs5.text -side left -fill both -expand true

#     scrollbar .cbs5.sb -relief raised -command ".cbs5.text yview"
#     pack .cbs5.sb -side right -fill y
# }

# proc fillPcrPicChoices {} {
#     .cbs5.text delete 1.0 end

#     global pic0choice pic1choice
#     global shortPic0Desc longPic0Desc shortPic1Desc longPic1Desc

#     .cbs5.text insert end "PIC0: $shortPic0Desc($pic0choice) "
#     .cbs5.text insert end "$longPic0Desc($pic0choice)\n"
#     .cbs5.text insert end "PIC1: $shortPic1Desc($pic1choice) "
#     .cbs5.text insert end "$longPic1Desc($pic1choice)"
# }

# proc hidePcrPicChoicesArea {} {
#     destroy .cbs5
# }

set metinfonum 0
proc showMetricInfo {metnum} {
    global helv14bold courier12bold
    global metinfonum
    set wname ".metinfo$metinfonum"
    incr metinfonum

    toplevel $wname
    # one doesn't pack a toplevel window

    wm title $wname "Metric description"

    frame $wname.top
    pack $wname.top -side top -fill x

    label $wname.top.lab2 -text [getMetricName $metnum] -font $helv14bold \
	    -foreground white -background HotPink2 -relief raised
    pack $wname.top.lab2 -side top -fill x

    frame $wname.bot
    pack $wname.bot -side bottom -fill both -expand true

    text $wname.bot.text -font $courier12bold \
	    -yscrollcommand "$wname.bot.sb set" \
	    -width 60 -height 12 -wrap word -relief raised
    pack $wname.bot.text -side left -fill both -expand true

    scrollbar $wname.bot.sb -relief sunken -troughcolor lightgrey \
	    -command "$wname.bot.text yview"
    pack $wname.bot.sb -side right -fill y

    set the_text [getMetricDetails $metnum]
    $wname.bot.text insert end $the_text
}

# global outliningDoReplaceFunction
# set outliningDoReplaceFunction 1

# global outliningReplaceFunctionPatchUpCallSitesToo
# set outliningReplaceFunctionPatchUpCallSitesToo 0

# global outliningBlockCountDelay
# set outliningBlockCountDelay 5000

# global outlineChildrenToo
# set outlineChildrenToo 1

# global forceOutliningOfAllChildren
# set forceOutliningOfAllChildren 0

# global outliningUsingFixedDescendants
# set outliningUsingFixedDescendants 0

# global outliningBlockPlacement
# set outliningBlockPlacement chains

# global outliningHotBlockThresholdChoice
# set outliningHotBlockThresholdChoice FivePercent

# global fixedOutlineDescendants
# set fixedOutlineDescendants(tcp/tcp_rput_data) {unix/mutex_enter unix/mutex_vector_enter genunix/.urem genunix/isuioq ip/ip_cksum genunix/canputnext unix/ip_ocsum tcp/tcp_ack_mp unix/putnext ip/mi_timer genunix/strwakeq genunix/releasestr genunix/allocb tcp/tcp_xmit_mp genunix/cv_broadcast unix/disp_lock_enter genunix/sleepq_wakeall_chan genunix/freeb genunix/freemsg genunix/msgdsize ip/mi_set_sth_wroff ip/mi_tpi_ordrel_ind tcp/struio_ioctl genunix/qfill_syncq genunix/putnext_tail genunix/.mul genunix/.div unix/caslong genunix/kmem_cache_alloc unix/lock_set_spl_spin unix/mutex_tryenter genunix/pollwakeup ip/mi_strlog SUNW,UltraSPARC-IIi/gethrtime unix/lock_set_spl unix/mutex_vector_tryenter genunix/pollnotify genunix/cv_signal genunix/turnstile_lookup genunix/turnstile_block genunix/untimeout tcp/tcp_set_rto tcp/tcp_co_drain tcp/tcp_reass tcp/tcp_lookup_ipv4 unix/disp_lock_exit ip/mi_timer_stop unix/membar_enter unix/membar_consumer unix/membar_return genunix/timeout genunix/timeout_common}

# set fixedOutlineDescendants(genunix/timeout) {genunix/timeout_common}

# set forceIncludeOutlineDescendants(tcp/tcp_rput_data) {unix/lock_set_spl_spin}
# # disp_lock_enter requires lock_set_spl_spin to avoid unanalyzable jmp
# set forceIncludeOutlineDescendants(unix/disp_lock_enter) {unix/lock_set_spl_spin}
# set forceExcludeOutlineDescendants {unix/mutex_exit unix/getlastfrompath SUNW,UltraSPARC-IIi/bcopy unix/audit_sock unix/clboot_modload unix/cl_flk_state_transition_notify}
