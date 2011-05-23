package com.dynamo.cr.guieditor.tools;

import java.util.ArrayList;
import java.util.List;

import javax.media.opengl.GL;
import javax.vecmath.Vector4d;

import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.guieditor.GuiSelectionProvider;
import com.dynamo.cr.guieditor.IGuiEditor;
import com.dynamo.cr.guieditor.operations.SelectOperation;
import com.dynamo.cr.guieditor.operations.SetPropertiesOperation;
import com.dynamo.cr.guieditor.property.BeanPropertyAccessor;
import com.dynamo.cr.guieditor.property.IPropertyAccessor;
import com.dynamo.cr.guieditor.property.IPropertyObjectWorld;
import com.dynamo.cr.guieditor.scene.GuiNode;
import com.dynamo.cr.guieditor.scene.GuiScene;

public class SelectMoveTool {

    private IGuiEditor editor;
    private GuiSelectionProvider selectionProvider;
    private int startX;
    private int startY;
    private int prevX;
    private int prevY;
    private List<GuiNode> originalSelection;
    private List<Vector4d> originalPositions;

    enum State {
        INITIAL,
        MOVING,
        SELECTING,
    }
    private State state = State.INITIAL;
    private boolean changed;

    public SelectMoveTool(IGuiEditor editor, GuiSelectionProvider selectionProvider) {
        this.editor = editor;
        this.originalSelection = selectionProvider.getSelectionList();
        this.selectionProvider = selectionProvider;
    }

    public void mouseDown(MouseEvent e) {
        this.prevX = e.x;
        this.prevY = e.y;
        this.startX = e.x;
        this.startY = e.y;

        List<GuiNode> selectedNodes = editor.rectangleSelect(e.x, e.y, 5, 5);
        if (selectedNodes.size() > 0) {

            ArrayList<GuiNode> tmp = new ArrayList<GuiNode>();
            tmp.add(selectedNodes.get(0));
            selectedNodes = tmp;

            List<GuiNode> toMoveSelection;
            if (selectionProvider.getSelectionList().contains(selectedNodes.get(0))) {
                // Current selection is part of click selection. Keep selection.
                state = State.MOVING;
                toMoveSelection = selectionProvider.getSelectionList();
            }
            else {
                // Something new is selected
                state = State.MOVING;
                addSelectionOperation(selectedNodes);
                selectionProvider.setSelection(selectedNodes);
                toMoveSelection = selectedNodes;
            }

            originalPositions = new ArrayList<Vector4d>();
            for (GuiNode node : toMoveSelection) {
                originalPositions.add(node.getPosition());
            }
        }
        else {
            state = State.SELECTING;
            selectionProvider.setSelection(selectedNodes);
        }
    }

    public void mouseMove(MouseEvent e) {
        if (state == State.SELECTING) {
            int x = Math.min(e.x, startX);
            int y = Math.min(e.y, startY);
            int w = Math.abs(e.x - startX);
            int h = Math.abs(e.y - startY);
            List<GuiNode> selectedNodes = editor.rectangleSelect(x, y, w, h);
            selectionProvider.setSelection(selectedNodes);
        }
        else if (state == State.MOVING) {
            int dx = e.x - startX;
            int dy = e.y - startY;

            if (dx != 0 || dy != 0)
                changed = true;

            int i = 0;
            for (GuiNode node : selectionProvider.getSelectionList()) {
                Vector4d position = new Vector4d(originalPositions.get(i));
                position.x += dx;
                position.y += dy;
                node.setPosition(position);
                ++i;
            }
        }

        this.prevX = e.x;
        this.prevY = e.y;
    }

    public void mouseUp(MouseEvent e) {
        if (state == State.SELECTING) {
            addSelectionOperation(selectionProvider.getSelectionList());
        }
        else if (state == State.MOVING) {
            if (changed) {
                List<Vector4d> newPositions = new ArrayList<Vector4d>();
                for (GuiNode node : selectionProvider.getSelectionList()) {
                    Vector4d position = node.getPosition();
                    newPositions.add(position);
                }
                IPropertyAccessor<?, ? extends IPropertyObjectWorld> tmp = new BeanPropertyAccessor();
                @SuppressWarnings("unchecked")
                IPropertyAccessor<Object, GuiScene> accessor = (IPropertyAccessor<Object, GuiScene>) tmp;
                SetPropertiesOperation<Vector4d> operation = new SetPropertiesOperation<Vector4d>((List) selectionProvider.getSelectionList(), "position", accessor, originalPositions, newPositions, editor.getScene());
                editor.executeOperation(operation);
            }
        }
        state = State.INITIAL;
    }

    private void addSelectionOperation(List<GuiNode> selection) {
        if (!originalSelection.equals(selection)) {
            SelectOperation operation = new SelectOperation(selectionProvider, originalSelection, selection);
            editor.executeOperation(operation);
        }
    }

    public void draw(GL gl) {
        if (state == State.SELECTING) {
            gl.glPushAttrib(GL.GL_DEPTH_BUFFER_BIT | GL.GL_COLOR_BUFFER_BIT);
            gl.glDisable(GL.GL_DEPTH_TEST);
            gl.glEnable(GL.GL_BLEND);
            gl.glBlendFunc (GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);

            gl.glColor4d(0.6, 0.3, 0.3, 0.5);
            gl.glBegin(GL.GL_QUADS);
            gl.glVertex2d(startX, startY);
            gl.glVertex2d(prevX, startY);
            gl.glVertex2d(prevX, prevY);
            gl.glVertex2d(startX, prevY);
            gl.glEnd();

            gl.glPopAttrib();
        }
    }
}
