package com.dynamo.cr.contenteditor.commands;

import java.io.CharArrayWriter;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.swt.dnd.Clipboard;
import org.eclipse.swt.dnd.TextTransfer;
import org.eclipse.swt.dnd.Transfer;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.contenteditor.editors.IEditor;
import com.dynamo.cr.scene.graph.CollectionNode;
import com.dynamo.cr.scene.graph.Node;
import com.dynamo.cr.scene.resource.CollectionResource;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.google.protobuf.TextFormat;

public class Copy extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editor = HandlerUtil.getActiveEditor(event);
        if (editor instanceof IEditor) {
            Node[] selectedNodes = ((IEditor) editor).getSelectedNodes();
            if (selectedNodes.length > 0) {

                ((IEditor) editor).setPasteTarget(selectedNodes[0].getParent());

                Shell shell = editor.getSite().getShell();
                Clipboard cb = new Clipboard(shell.getDisplay());

                try {
                    // Create a temporary collection of selected nodes
                    CollectionResource dummyResource = new CollectionResource("dummy", null);
                    CollectionNode coll = new CollectionNode("clipboard_collection", dummyResource, ((IEditor) editor).getScene(), ((IEditor)editor).getNodeFactory());
                    Node prevNode = null;
                    for (Node node : selectedNodes) {
                        boolean copy = true;
                        if (prevNode != null) {
                            Node parent = node.getParent();
                            while (parent != null && parent != prevNode) {
                                parent = parent.getParent();
                            }
                            if (parent == prevNode) {
                                copy = false;
                            }
                        }
                        if (copy) {
                            while (node != null && !coll.acceptsChild(node)) {
                                node = node.getParent();
                            }
                            if (node != null) {
                                // We can't set the parent of existing nodes. Destructive operation.
                                coll.addNodeNoSetParent(node);
                                prevNode = node;
                            }
                        }
                    }

                    CollectionDesc desc = coll.buildDescriptor();
                    CharArrayWriter output = new CharArrayWriter(1024);
                    TextFormat.print(desc, output);
                    TextTransfer transfer = TextTransfer.getInstance();
                    cb.setContents(new Object[] {output.toString()}, new Transfer[] {transfer});
                    output.close();
                } catch (Throwable e) {
                    throw new ExecutionException(e.getMessage(), e);
                }
            }
        }
        return null;
    }
}
