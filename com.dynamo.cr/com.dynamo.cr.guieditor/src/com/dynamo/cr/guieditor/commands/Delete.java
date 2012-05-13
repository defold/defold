package com.dynamo.cr.guieditor.commands;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.guieditor.IGuiEditor;
import com.dynamo.cr.guieditor.operations.DeleteFontsOperation;
import com.dynamo.cr.guieditor.operations.DeleteGuiNodesOperation;
import com.dynamo.cr.guieditor.operations.DeleteTexturesOperation;
import com.dynamo.cr.guieditor.scene.EditorFontDesc;
import com.dynamo.cr.guieditor.scene.EditorTextureDesc;
import com.dynamo.cr.guieditor.scene.GuiNode;

public class Delete extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editorPart = HandlerUtil.getActiveEditor(event);
        if (editorPart instanceof IGuiEditor) {
            IGuiEditor editor = (IGuiEditor) editorPart;

            List<GuiNode> nodes = new ArrayList<GuiNode>();
            List<EditorTextureDesc> textures = new ArrayList<EditorTextureDesc>();
            List<EditorFontDesc> fonts = new ArrayList<EditorFontDesc>();

            ISelection selection = HandlerUtil.getCurrentSelection(event);

            if (selection instanceof IStructuredSelection) {
                IStructuredSelection structuredSelection = (IStructuredSelection) selection;
                for (Object selected : structuredSelection.toList()) {
                    if (selected instanceof GuiNode) {
                        GuiNode node = (GuiNode) selected;
                        nodes.add(node);
                    }
                    else if (selected instanceof EditorTextureDesc) {
                        textures.add((EditorTextureDesc) selected);
                    }
                    else if (selected instanceof EditorFontDesc) {
                        fonts.add((EditorFontDesc) selected);
                    }
                }

            }

            if (nodes.size() > 0) {
                DeleteGuiNodesOperation operation = new DeleteGuiNodesOperation(editor, nodes);
                editor.executeOperation(operation);
            }

            if (textures.size() > 0) {
                DeleteTexturesOperation operation = new DeleteTexturesOperation(editor, textures);
                editor.executeOperation(operation);
            }

            if (fonts.size() > 0) {
                DeleteFontsOperation operation = new DeleteFontsOperation(editor, fonts);
                editor.executeOperation(operation);
            }

        }
        return null;
    }
}
