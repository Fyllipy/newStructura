#!/bin/bash
# usage: run.sh <dll> <outname> <mode> <value> [depth]
SP="C:/Users/JOO~1/AppData/Local/Temp/claude/C--Users-Jo-o-Documents-Repos-Conversor/62e829b0-04f7-4a4c-ac8d-ffd81bb16d8a/scratchpad"
"/c/Ghidra/support/analyzeHeadless.bat" "$SP/gproj" cype -process "$1" -noanalysis -scriptPath "C:/Users/João/Documents/Repos/Conversor/tools/ghidra" -postScript DumpRefs.java "$SP/$2" "$3" "$4" ${5:-0} 2>&1 | grep -E "ERROR|Exception" | head -3
grep -c "=====" "$SP/$2"; wc -c "$SP/$2"
