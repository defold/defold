package com.dynamo.cr.contenteditor.commands;

import java.io.ByteArrayInputStream;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.swt.dnd.Clipboard;
import org.eclipse.swt.dnd.TextTransfer;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.contenteditor.editors.IEditor;
import com.dynamo.cr.contenteditor.operations.PasteOperation;
import com.dynamo.cr.contenteditor.scene.CollectionNode;
import com.dynamo.cr.contenteditor.scene.Node;
import com.dynamo.cr.contenteditor.scene.Scene;

public class Paste extends AbstractHandler {

    private void setScene(Scene scene, Node node) {
        node.setScene(scene);
        for (Node n : node.getChildren()) {
            setScene(scene, n);
        }
    }

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editor = HandlerUtil.getActiveEditor(event);
        if (editor instanceof IEditor) {
            Node target = ((IEditor) editor).getPasteTarget();
            if (target == null)
                target = ((IEditor) editor).getRoot();

            Shell shell = editor.getSite().getShell();
            Clipboard cb = new Clipboard(shell.getDisplay());
            TextTransfer transfer = TextTransfer.getInstance();
            String data = (String) cb.getContents(transfer);
            ByteArrayInputStream stream = new ByteArrayInputStream(data.getBytes());
            Scene scene = new Scene();
            try {
                CollectionNode node = (CollectionNode) ((IEditor)editor).getLoaderFactory().load(new NullProgressMonitor(), scene, "clipboard.collection", stream, null);
                setScene(target.getScene(), node);

                if (data != null) {
                    PasteOperation op = new PasteOperation(node.getChildren(), target);
                    ((IEditor) editor).executeOperation(op);
                }
            } catch (Exception e) {
                throw new ExecutionException(e.getMessage(), e);
            }
        }
        return null;
    }
}
