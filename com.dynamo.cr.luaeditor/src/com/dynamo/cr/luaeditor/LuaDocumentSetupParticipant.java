package com.dynamo.cr.luaeditor;

import org.eclipse.core.filebuffers.IDocumentSetupParticipant;
import org.eclipse.jface.text.IDocument;
import org.eclipse.jface.text.IDocumentExtension3;
import org.eclipse.jface.text.IDocumentPartitioner;
import org.eclipse.jface.text.rules.FastPartitioner;

public class LuaDocumentSetupParticipant implements IDocumentSetupParticipant {

    @Override
    public void setup(IDocument document) {
        IDocumentPartitioner partitioner =
                new FastPartitioner(
                    new LuaPartitionScanner(),
                    new String[] {
                        IDocument.DEFAULT_CONTENT_TYPE,
                        LuaPartitionScanner.LUA_COMMENT_SINGLE,
                        LuaPartitionScanner.LUA_COMMENT_MULTI});

        IDocumentExtension3 ext3 = (IDocumentExtension3) document;
        ext3.setDocumentPartitioner("com.dynamo.cr.luaeditor.partitioning", partitioner);
        partitioner.connect(document);
    }

}
