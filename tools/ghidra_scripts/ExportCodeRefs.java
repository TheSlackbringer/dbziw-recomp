// Exports every reference whose target lies in a given code range, and every function, as TSV (issue 014).
// Usage (headless, on a copy of the project): -postScript ExportCodeRefs.java <out.tsv> [start end]
// Lines: "ref<TAB>from<TAB>to<TAB>refType<TAB>fromFunction<TAB>toFunction" and "func<TAB>entry<TAB>end<TAB>name".
// Addresses are hex without 0x; a missing function is "-". Default range 0x100000-0x40d204 (DBZIW's code).
// @category PS2Recomp

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceManager;

import java.io.File;
import java.io.PrintWriter;

public class ExportCodeRefs extends GhidraScript {

    private static String entry(Function function) {
        return function == null ? "-" : Long.toHexString(function.getEntryPoint().getOffset());
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            printerr("usage: ExportCodeRefs.java <out.tsv> [start end]");
            return;
        }
        long start = args.length > 2 ? Long.decode(args[1]) : 0x100000L;
        long end = args.length > 2 ? Long.decode(args[2]) : 0x40d204L;
        FunctionManager functions = currentProgram.getFunctionManager();
        ReferenceManager references = currentProgram.getReferenceManager();
        int refCount = 0, funcCount = 0;
        try (PrintWriter out = new PrintWriter(new File(args[0]), "UTF-8")) {
            FunctionIterator it = functions.getFunctions(true);
            while (it.hasNext() && !monitor.isCancelled()) {
                Function function = it.next();
                out.println("func\t" + entry(function) + "\t"
                        + Long.toHexString(function.getBody().getMaxAddress().getOffset() + 1) + "\t" + function.getName());
                funcCount++;
            }
            for (Address from : references.getReferenceSourceIterator(currentProgram.getMinAddress(), true)) {
                if (monitor.isCancelled()) {
                    break;
                }
                for (Reference reference : references.getReferencesFrom(from)) {
                    Address to = reference.getToAddress();
                    if (!to.isMemoryAddress() || to.getOffset() < start || to.getOffset() >= end) {
                        continue;
                    }
                    out.println("ref\t" + Long.toHexString(from.getOffset()) + "\t" + Long.toHexString(to.getOffset()) + "\t"
                            + reference.getReferenceType() + "\t" + entry(functions.getFunctionContaining(from)) + "\t"
                            + entry(functions.getFunctionContaining(to)));
                    refCount++;
                }
            }
        }
        println("ExportCodeRefs: " + funcCount + " functions, " + refCount + " references -> " + args[0]);
    }
}
