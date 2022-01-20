# $Id: generic.tcl,v 1.1 2004/10/25 23:08:38 igor Exp $
# Routines found useful across all tk4.0 programs

proc resize1Scrollbar {sbname newTotal newVisible} {
   # This is a nice n' generic routine  --ari
   # However, it is (currently) only called from C++ code.  If this
   # situation doesn't change, then we might want to just
   # zap this and turn it into C++ code...

   # 'newTotal' and 'newVisible' are tentative values;
   # We use them to calculate 'newFirst' and 'newLast'.
   # We make an effort to keep 'newFirst' as close as possible to 'oldFirst'.

   set oldConfig [$sbname get]
   set oldFirst  [lindex $oldConfig 0]
   set oldLast   [lindex $oldConfig 1]
#   puts stderr "newTotal=$newTotal; newVisible=$newVisible; oldFirst=$oldFirst; oldLast=$oldLast"

   if {$newVisible < $newTotal} {
      # The usual case: not everything fits
      set fracVisible [expr 1.0 * $newVisible / $newTotal]
#      puts stderr "newVisible=$newVisible; newTotal=$newTotal; fracVisible=$fracVisible"

      set newFirst $oldFirst
      set newLast [expr $newFirst + $fracVisible]

#      puts stderr "tentative newFirst=$newFirst; newLast=$newLast"

      if {$newLast > 1.0} {
         set theOverflow [expr $newLast - 1.0]
#         puts stderr "resize1Scrollbar: would overflow by fraction of $theOverflow; moving newFirst back"

         set newFirst [expr $oldFirst - $theOverflow]
         set newLast  [expr $newFirst + $fracVisible]
      } else {
#         puts stderr "resize1Scrollbar: yea, we were able to keep newFirst unchanged at $newFirst"
      }
   } else {
      # the unusual case: everything fits (visible >= total)
#      puts stderr "everything fits"
      set newFirst 0.0
      set newLast  1.0
   }

   if {$newFirst < 0} {
      # This is an assertion failure
      # (But we don't trigger a real assert, since it often happens in practice due to
      # floating point roundoff errors)
      #puts stderr "resize1Scrollbar warning: newFirst is $newFirst"

      set newFirst 0.0
   }
   if {$newLast > 1} {
      # This is an assertion failure
      # (But we don't trigger a real assert, since it often happens in practice due to
      # floating point roundoff errors)
      # puts stderr "resize1Scrollbar warning: newLast is $newLast"

      set newLast 1.0
   }

   $sbname set $newFirst $newLast
   return $newFirst
}
