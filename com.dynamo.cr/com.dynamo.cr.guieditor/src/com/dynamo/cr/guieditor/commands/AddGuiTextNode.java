package com.dynamo.cr.guieditor.commands;

import java.util.Collection;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.guieditor.IGuiEditor;
import com.dynamo.cr.guieditor.operations.AddGuiNodeOperation;
import com.dynamo.cr.guieditor.scene.EditorFontDesc;
import com.dynamo.cr.guieditor.scene.TextGuiNode;
import com.dynamo.gui.proto.Gui.NodeDesc;
import com.dynamo.gui.proto.Gui.NodeDesc.Type;
import com.dynamo.proto.DdfMath.Vector4;

public class AddGuiTextNode extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof IGuiEditor) {
            IGuiEditor editor = (IGuiEditor) editorPart;
            int x0 = 100;
            int y0 = 300;

            Collection<EditorFontDesc> fonts = editor.getScene().getFonts();
            String font = "";
            if (fonts.size() > 0) {
                font = fonts.iterator().next().getName();
            }

            String id = editor.getScene().getUniqueId();

            NodeDesc nodeDesc = NodeDesc.newBuilder()
                                    .setType(Type.TYPE_TEXT)
                                    .setText("unnamed")
                                    .setFont(font)
                                    .setPosition(Vector4.newBuilder().setX(x0).setY(y0))
                                    .setSize(Vector4.newBuilder().setX(200).setY(100))
                                    .setColor(Vector4.newBuilder().setX(1).setY(1).setZ(1).setW(1))
                                    .setScale(Vector4.newBuilder().setX(1).setY(1).setZ(1))
                                    .setRotation(Vector4.newBuilder().setX(0).setY(0).setZ(0))
                                    .setId(id)
                                    .build();
            TextGuiNode node = new TextGuiNode(editor.getScene(), nodeDesc);
            AddGuiNodeOperation operation = new AddGuiNodeOperation(editor, node);
            editor.executeOperation(operation);
        }
        return null;
    }
}
