import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.listing.*;
import java.io.*;
import java.util.*;
// For each DAT address (hex, '+' separated) print the functions referencing it: "addr fnEntry"
public class XrefsData extends GhidraScript {
    public void run() throws Exception {
        String[] a = getScriptArgs();
        PrintWriter pw = new PrintWriter(new FileWriter(a[0]));
        BufferedReader br = new BufferedReader(new FileReader(a[1]));
        String line;
        while ((line = br.readLine()) != null) {
            line = line.trim(); if (line.isEmpty()) continue;
            Address ad = toAddr(Long.parseUnsignedLong(line, 16));
            ReferenceIterator it = currentProgram.getReferenceManager().getReferencesTo(ad);
            Set<String> fns = new LinkedHashSet<>();
            while (it.hasNext()) {
                Reference r = it.next();
                Function fn = getFunctionContaining(r.getFromAddress());
                if (fn != null) fns.add(fn.getEntryPoint().toString());
            }
            pw.println(line + " " + String.join(",", fns));
        }
        pw.close();
    }
}
