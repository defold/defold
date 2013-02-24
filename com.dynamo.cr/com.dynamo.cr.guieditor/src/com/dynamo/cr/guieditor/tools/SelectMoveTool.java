package com.dynamo.cr.guieditor.tools;

import java.util.ArrayList;
import java.util.List;

import javax.media.opengl.GL;
import javax.media.opengl.GL2;
import javax.vecmath.Vector3d;

import org.eclipse.swt.SWT;
import org.eclipse.swt.events.KeyEvent;
import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.guieditor.GuiSelectionProvider;
import com.dynamo.cr.guieditor.IGuiEditor;
import com.dynamo.cr.guieditor.operations.SelectOperation;
import com.dynamo.cr.guieditor.operations.SetPropertiesOperation;
import com.dynamo.cr.guieditor.scene.GuiNode;
import com.dynamo.cr.guieditor.scene.GuiScene;
import com.dynamo.cr.properties.BeanPropertyAccessor;
import com.dynamo.cr.properties.IPropertyAccessor;
import com.dynamo.cr.properties.IPropertyObjectWorld;

public class SelectMoveTool {

    private IGuiEditor editor;
    private GuiSelectionProvider selectionProvider;
    private int startX;
    private int startY;
    private int prevX;
    private int prevY;
    private List<GuiNode> originalSelection;
    private List<Vector3d> originalPositions;

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

            boolean newSelectionPartOfCurrent = selectionProvider.getSelectionList().contains(selectedNodes.get(0));

            if ((e.stateMask & SWT.SHIFT) == SWT.SHIFT) {
                state = State.SELECTING;
                toMoveSelection = new ArrayList<GuiNode>(selectionProvider.getSelectionList());
                if (newSelectionPartOfCurrent) {
                    toMoveSelection.removeAll(selectedNodes);
                } else {
                    toMoveSelection.addAll(selectedNodes);
                }
                addSelectionOperation(toMoveSelection);
            } else {
                state = State.MOVING;
                if (newSelectionPartOfCurrent) {
                    // Current selection is part of click selection. Keep selection.
                    toMoveSelection = selectionProvider.getSelectionList();
                } else {
                    // Something new is selected
                    addSelectionOperation(selectedNodes);
                    selectionProvider.setSelection(selectedNodes);
                    toMoveSelection = selectedNodes;
                }

                originalPositions = new ArrayList<Vector3d>();
                for (GuiNode node : toMoveSelection) {
                    originalPositions.add(node.getPosition());
                }
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
                Vector3d position = new Vector3d(originalPositions.get(i));
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
                List<Vector3d> newPositions = new ArrayList<Vector3d>();
                for (GuiNode node : selectionProvider.getSelectionList()) {
                    Vector3d position = node.getPosition();
                    newPositions.add(position);
                }
                IPropertyAccessor<?, ? extends IPropertyObjectWorld> tmp = new BeanPropertyAccessor();
                @SuppressWarnings("unchecked")
                IPropertyAccessor<Object, GuiScene> accessor = (IPropertyAccessor<Object, GuiScene>) tmp;
                @SuppressWarnings({ "unchecked", "rawtypes" })
                SetPropertiesOperation<Vector3d> operation = new SetPropertiesOperation<Vector3d>((List) selectionProvider.getSelectionList(), "position", accessor, originalPositions, newPositions, editor.getScene());
                editor.executeOperation(operation);
            }
        }
        state = State.INITIAL;
    }

    public void keyPressed(KeyEvent e) {
        int dx = 0;
        int dy = 0;
        if (e.keyCode == SWT.ARROW_DOWN) {
            dy = -1;
        } else if (e.keyCode == SWT.ARROW_UP) {
            dy = 1;
        } else if (e.keyCode == SWT.ARROW_LEFT) {
            dx = -1;
        } else if (e.keyCode == SWT.ARROW_RIGHT) {
            dx = 1;
        }

        if ((e.stateMask & SWT.SHIFT) == SWT.SHIFT) {
            dx *= 10;
            dy *= 10;
        }

        if (dx != 0 || dy != 0) {
            originalPositions = new ArrayList<Vector3d>();
            List<Vector3d> newPositions = new ArrayList<Vector3d>();

            for (GuiNode node : selectionProvider.getSelectionList()) {
                Vector3d position = node.getPosition();
                originalPositions.add(position);
                position = node.getPosition();
                position.add(new Vector3d(dx, dy, 0));
                newPositions.add(position);
            }
            IPropertyAccessor<?, ? extends IPropertyObjectWorld> tmp = new BeanPropertyAccessor();
            @SuppressWarnings("unchecked")
            IPropertyAccessor<Object, GuiScene> accessor = (IPropertyAccessor<Object, GuiScene>) tmp;
            @SuppressWarnings({ "unchecked", "rawtypes" })
            SetPropertiesOperation<Vector3d> operation = new SetPropertiesOperation<Vector3d>((List) selectionProvider.getSelectionList(), "position", accessor, originalPositions, newPositions, editor.getScene());
            editor.executeOperation(operation);
        }

    }

    public void keyReleased(KeyEvent e) {

    }

    private void addSelectionOperation(List<GuiNode> selection) {
        if (!originalSelection.equals(selection)) {
            SelectOperation operation = new SelectOperation(selectionProvider, originalSelection, selection);
            editor.executeOperation(operation);
        }
    }

    public void draw(GL2 gl) {
        if (state == State.SELECTING) {
            gl.glPushAttrib(GL.GL_DEPTH_BUFFER_BIT | GL.GL_COLOR_BUFFER_BIT);
            gl.glDisable(GL.GL_DEPTH_TEST);
            gl.glEnable(GL.GL_BLEND);
            gl.glBlendFunc (GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);

            int minX = Math.min(startX, prevX);
            int minY = Math.min(startY, prevY);
            int maxX = Math.max(startX, prevX);
            int maxY = Math.max(startY, prevY);

            gl.glColor4d(1.0, 1.0, 1.0, 0.2);
            gl.glBegin(GL2.GL_QUADS);
            gl.glVertex2d(minX, minY);
            gl.glVertex2d(maxX, minY);
            gl.glVertex2d(maxX, maxY);
            gl.glVertex2d(minX, maxY);
            gl.glEnd();

            final int border = 1;
            gl.glColor4d(1.0, 1.0, 1.0, 0.2);
            gl.glBegin(GL2.GL_QUADS);
            gl.glVertex2d(minX + border, minY + border);
            gl.glVertex2d(maxX - border, minY + border);
            gl.glVertex2d(maxX - border, maxY - border);
            gl.glVertex2d(minX + border, maxY - border);
            gl.glEnd();

            gl.glPopAttrib();
        }
    }
}
