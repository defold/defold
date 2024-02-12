package com.defold.editor;

import java.io.Reader;
import java.io.Writer;
import java.util.*;

public interface CodeTranspiler {

    record Import (long rowIndex, String importString) implements Comparable<Import> {
        @Override

        public int compareTo(Import other) {
            return Objects.compare(this, other, COMPARATOR);
        }

        private static final Comparator<Import> COMPARATOR = Comparator.comparingLong(Import::rowIndex).thenComparing(Import::importString);
    }

    Map<Import, List<String>> getImportCandidateProjPaths(Reader sourceReader) throws Exception;
    void build(Reader sourceReader, Writer transpiledOutputWriter) throws Exception;
}
