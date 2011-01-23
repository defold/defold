package com.dynamo.cr.contenteditor.commands;

import java.io.ByteArrayOutputStream;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.swt.dnd.Clipboard;
import org.eclipse.swt.dnd.TextTransfer;
import org.eclipse.swt.dnd.Transfer;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.contenteditor.editors.IEditor;
import com.dynamo.cr.contenteditor.editors.NodeLoaderFactory;
import com.dynamo.cr.contenteditor.scene.CollectionNode;
import com.dynamo.cr.contenteditor.scene.Node;

public class Copy extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editor = HandlerUtil.getActiveEditor(event);
        if (editor instanceof IEditor) {
            Node[] selected_nodes = ((IEditor) editor).getSelectedNodes();
            if (selected_nodes.length >= 0) {

                ((IEditor) editor).setPasteTarget(selected_nodes[0].getParent());

                NodeLoaderFactory factory = ((IEditor) editor).getLoaderFactory();
                Shell shell = editor.getSite().getShell();
                Clipboard cb = new Clipboard(shell.getDisplay());

                ByteArrayOutputStream stream = new ByteArrayOutputStream(1024);
                try {
                    // Create a temporary collection of selected nodes
                    CollectionNode coll = new CollectionNode(((IEditor) editor).getScene(), "clipboard_collection", "dummy");
                    for (Node node : selected_nodes) {
                        // We can't set the parent of existing nodes. Destructive operation.
                        coll.addNodeNoSetParent(node);
                    }
                    factory.save(new NullProgressMonitor(), "clipboard.collection", coll, stream);
                    TextTransfer transfer = TextTransfer.getInstance();
                    cb.setContents(new Object[] {stream.toString()}, new Transfer[] {transfer});
                } catch (Throwable e) {
                    throw new ExecutionException(e.getMessage(), e);
                }
            }
        }
        return null;
    }
}
