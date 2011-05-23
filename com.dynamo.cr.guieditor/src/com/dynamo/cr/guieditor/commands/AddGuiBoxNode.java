package com.dynamo.cr.guieditor.commands;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.guieditor.IGuiEditor;
import com.dynamo.cr.guieditor.operations.AddGuiNodeOperation;
import com.dynamo.cr.guieditor.scene.BoxGuiNode;
import com.dynamo.gui.proto.Gui.NodeDesc;
import com.dynamo.gui.proto.Gui.NodeDesc.Type;
import com.dynamo.proto.DdfMath.Vector4;

public class AddGuiBoxNode extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof IGuiEditor) {
            IGuiEditor editor = (IGuiEditor) editorPart;
            int x0 = 100;
            int y0 = 300;

            String id = editor.getScene().getUniqueId();

            NodeDesc nodeDesc = NodeDesc.newBuilder()
                                    .setType(Type.TYPE_BOX)
                                    .setPosition(Vector4.newBuilder().setX(x0).setY(y0))
                                    .setSize(Vector4.newBuilder().setX(200).setY(100))
                                    .setColor(Vector4.newBuilder().setZ((float) (200/255.0)).setW(1))
                                    .setScale(Vector4.newBuilder().setX(1).setY(1).setZ(1))
                                    .setRotation(Vector4.newBuilder().setX(0).setY(0).setZ(0))
                                    .setId(id)
                                    .build();
            BoxGuiNode node = new BoxGuiNode(editor.getScene(), nodeDesc);
            AddGuiNodeOperation operation = new AddGuiNodeOperation(editor, node);
            editor.executeOperation(operation);
        }
        return null;
    }
}

