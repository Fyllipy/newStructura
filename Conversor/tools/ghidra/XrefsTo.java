import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.listing.*;
import java.io.*;
public class XrefsTo extends GhidraScript {
    public void run() throws Exception {
        String[] a = getScriptArgs();
        PrintWriter pw = new PrintWriter(new FileWriter(a[0]));
        for (String h : a[1].split("[+;]")) {
            Address ad = toAddr(Long.parseUnsignedLong(h.trim(), 16));
            ReferenceIterator it = currentProgram.getReferenceManager().getReferencesTo(ad);
            while (it.hasNext()) {
                Reference r = it.next();
                Function fn = getFunctionContaining(r.getFromAddress());
                pw.println(h + " <- " + r.getFromAddress() + " " + r.getReferenceType() + " in " + (fn == null ? "?" : fn.getName() + "@" + fn.getEntryPoint()));
            }
        }
        pw.close();
    }
}
