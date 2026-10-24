// Dumps decompilation of functions that reference a string, or of functions at given addresses.
// args: outFile mode value [depth]
//   mode=str  : value = substring of a string in memory
//   mode=addr : value = comma separated hex addresses
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.mem.*;
import java.io.*;
import java.util.*;

public class DumpRefs extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        String out = a[0], mode = a[1], value = a[2];
        PrintWriter pw = new PrintWriter(new FileWriter(out));
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        Set<Function> funcs = new LinkedHashSet<>();
        if (mode.equals("str")) {
            Memory mem = currentProgram.getMemory();
            byte[] pat = value.getBytes("ISO-8859-1");
            byte[] mask = new byte[pat.length]; Arrays.fill(mask, (byte) 0xff);
            Address s = mem.getMinAddress();
            while (true) {
                Address f = mem.findBytes(s, pat, mask, true, monitor);
                if (f == null) break;
                pw.println("STRING at " + f);
                ReferenceIterator it = currentProgram.getReferenceManager().getReferencesTo(f);
                while (it.hasNext()) {
                    Reference r = it.next();
                    Function fn = getFunctionContaining(r.getFromAddress());
                    pw.println("  ref from " + r.getFromAddress() + " in " + (fn == null ? "?" : fn.getName() + "@" + fn.getEntryPoint()));
                    if (fn != null) funcs.add(fn);
                }
                s = f.add(1);
            }
        } else if (mode.equals("callers")) {
            for (Function t : currentProgram.getFunctionManager().getFunctions(true)) {
                if (!t.getName().equals(value.trim())) continue;
                ReferenceIterator it = currentProgram.getReferenceManager().getReferencesTo(t.getEntryPoint());
                while (it.hasNext()) {
                    Reference r = it.next();
                    Function fn = getFunctionContaining(r.getFromAddress());
                    pw.println("ref " + r.getFromAddress() + " " + r.getReferenceType() + " in " + (fn == null ? "?" : fn.getName()));
                    if (fn != null) funcs.add(fn);
                }
            }
        } else if (mode.equals("name")) {
            for (String nm : value.split("[+;]")) {
                for (Function fn : currentProgram.getFunctionManager().getFunctions(true)) {
                    if (fn.getName().contains(nm.trim())) funcs.add(fn);
                }
            }
        } else {
            for (String h : value.split("[+;]")) {
                Address ad = toAddr(Long.parseUnsignedLong(h.trim(), 16));
                Function fn = getFunctionContaining(ad);
                if (fn != null) funcs.add(fn);
            }
        }
        int depth = a.length > 3 ? Integer.parseInt(a[3]) : 0;
        if (depth > 0) {
            Set<Function> seen = new LinkedHashSet<>(funcs);
            List<Function> frontier = new ArrayList<>(funcs);
            for (int dd = 0; dd < depth; dd++) {
                List<Function> next = new ArrayList<>();
                for (Function f0 : frontier) {
                    Set<Function> cs = new LinkedHashSet<>(f0.getCalledFunctions(monitor));
                    InstructionIterator ii = currentProgram.getListing().getInstructions(f0.getBody(), true);
                    while (ii.hasNext()) {
                        Instruction in = ii.next();
                        for (Reference r : in.getReferencesFrom()) {
                            if (r.getReferenceType().isCall()) continue;
                            Function t = currentProgram.getFunctionManager().getFunctionAt(r.getToAddress());
                            if (t != null) cs.add(t);
                        }
                    }
                    for (Function c : cs) {
                        if (c.isExternal() || c.isThunk()) continue;
                        if (c.getBody().getNumAddresses() > 6000) continue;
                        if (seen.add(c)) next.add(c);
                    }
                }
                frontier = next;
            }
            funcs = seen;
        }
        for (Function fn : funcs) {
            pw.println("\n===== " + fn.getName() + " @ " + fn.getEntryPoint() + " size " + fn.getBody().getNumAddresses());
            DecompileResults r = di.decompileFunction(fn, 120, monitor);
            if (r.decompileCompleted()) pw.println(r.getDecompiledFunction().getC());
            else pw.println("decompile failed: " + r.getErrorMessage());
            pw.println("-- callees:");
            for (Function c : fn.getCalledFunctions(monitor)) pw.println("   " + c.getName() + " @ " + c.getEntryPoint());
        }
        pw.close();
    }
}
