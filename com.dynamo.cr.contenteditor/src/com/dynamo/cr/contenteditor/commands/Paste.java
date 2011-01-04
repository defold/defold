package com.dynamo.cr.contenteditor.commands;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.swt.dnd.Clipboard;
import org.eclipse.swt.dnd.TextTransfer;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.contenteditor.editors.IEditor;
import com.dynamo.cr.contenteditor.operations.PasteOperation;
import com.dynamo.cr.contenteditor.scene.Node;
import com.dynamo.cr.contenteditor.scene.Scene;

public class Paste extends AbstractHandler {

    public void setScene(Scene scene, Node node) {
        node.setScene(scene);
        for (Node n : node.getChilden()) {
            setScene(scene, n);
        }
    }

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editor = HandlerUtil.getActiveEditor(event);
        if (editor instanceof IEditor) {
            Node paste_target = ((IEditor) editor).getPasteTarget();
            if (paste_target == null)
                paste_target = ((IEditor) editor).getRoot();

            Shell shell = editor.getSite().getShell();
            Clipboard cb = new Clipboard(shell.getDisplay());
            TextTransfer transfer = TextTransfer.getInstance();
            String data = (String) cb.getContents(transfer);

            if (data != null) {
                PasteOperation op = new PasteOperation((IEditor) editor, data, paste_target);
                ((IEditor) editor).executeOperation(op);
            }
        }
        return null;
    }
}
