package com.dynamo.cr.contenteditor.manipulator;

import javax.vecmath.Matrix4d;
import javax.vecmath.Vector4d;

import org.eclipse.swt.SWT;

import com.dynamo.cr.contenteditor.math.MathUtil;
import com.dynamo.cr.contenteditor.math.Transform;
import com.dynamo.cr.contenteditor.operations.TransformNodeOperation;
import com.dynamo.cr.contenteditor.scene.Node;
import com.dynamo.cr.contenteditor.scene.NodeUtil;

// http://caad.arch.ethz.ch/info/maya/manual/UserGuide/Overview/TransformingObjects.fm.html
public abstract class TransformManipulator implements IManipulator {
    /**
     * Defines local space manipulation.
     */
    protected final static int LOCAL = 0;
    /**
     * Defines world (global) space manipulation.
     */
    protected final static int WORLD = 1;

    /**
     * The space space in which the manipulation takes place.
     */
    protected int space = LOCAL;
    /**
     * Axis of the selected manipulator handle in world (global) space.
     */
    protected Vector4d handleAxisMS = new Vector4d();
    /**
     * Transforms of the 3 manipulator handles in manipulator space.
     */
    protected final Matrix4d[] handleTransforms = new Matrix4d[3];
    /**
     * Original transform of the manipulator itself, before the manipulation took place.
     */
    protected Matrix4d originalManipulatorTransformWS = new Matrix4d();
    /**
     * Transform of the manipulator itself.
     */
    protected Matrix4d manipulatorTransformWS = new Matrix4d();
    /**
     * Name of the manipulator.
     */
    private String name;
    /**
     * The original local space transforms of the selected nodes, used for undo.
     */
    protected Transform[] originalNodeTransformsLS;
    /**
     * The transforms of the selected nodes relative the manipulator.
     */
    protected Matrix4d[] nodeTransformsMS;

    /**
     * If the manipulator is being moved or not, disables the ability to switch manipulation space.
     */
    private boolean moving = false;

    public TransformManipulator() {
        for (int i = 0; i < 3; ++i) {
            this.handleTransforms[i] = new Matrix4d();
            MathUtil.basisMatrix(this.handleTransforms[i], i);
        }
    }

    Vector4d getHandlePosition(Node[] nodes) {
        Vector4d position = new Vector4d();
        Matrix4d nodeTransform = new Matrix4d();
        Vector4d nodePosition = new Vector4d();
        for (Node n : nodes) {
            n.getTransform(nodeTransform);
            nodeTransform.getColumn(3, nodePosition);
            position.add(nodePosition);
        }
        position.scale(1.0 / nodes.length);

        return position;
    }

    @Override
    public void draw(ManipulatorDrawContext context) {
        if (!this.moving) {
            if (this.space == LOCAL && context.nodes.length > 1)
                this.space = WORLD;

            if (this.space == WORLD) {
                Vector4d translation = getHandlePosition(context.nodes);
                this.manipulatorTransformWS.setIdentity();
                this.manipulatorTransformWS.setColumn(3, translation);
            } else {
                context.nodes[0].getTransform(this.manipulatorTransformWS);
            }
        }
    }

    @Override
    public void mouseDown(ManipulatorContext context) {
        this.moving = true;

        if (this.space == LOCAL && context.nodes.length > 1)
            this.space = WORLD;

        this.originalManipulatorTransformWS.set(this.manipulatorTransformWS);
        this.handleAxisMS.set(1.0, 0.0, 0.0, 0.0);
        this.handleTransforms[context.manipulatorHandle].transform(handleAxisMS);

        Matrix4d invManipWS = new Matrix4d(this.manipulatorTransformWS);
        invManipWS.invert();
        this.originalNodeTransformsLS = new Transform[context.nodes.length];
        this.nodeTransformsMS = new Matrix4d[context.nodes.length];
        Matrix4d nodeTransformWS = new Matrix4d();
        int i = 0;
        for (Node n : context.nodes) {
            this.originalNodeTransformsLS[i] = new Transform();
            n.getLocalTransform(this.originalNodeTransformsLS[i]);
            this.nodeTransformsMS[i] = new Matrix4d(invManipWS);
            n.getTransform(nodeTransformWS);
            this.nodeTransformsMS[i].mul(nodeTransformWS);
            ++i;
        }
    }

    @Override
    public void mouseMove(ManipulatorContext context)
    {
        Matrix4d nodeTransformWS = new Matrix4d();
        int i = 0;
        for (Node n : context.nodes) {
            nodeTransformWS.set(this.manipulatorTransformWS);
            nodeTransformWS.mul(this.nodeTransformsMS[i]);
            NodeUtil.setWorldTransform(n, nodeTransformWS);
            ++i;
        }
    }

    @Override
    public void mouseUp(ManipulatorContext context) {
        this.moving = false;

        Transform[] t = new Transform[context.nodes.length];
        for (int i = 0; i < context.nodes.length; ++i) {
            t[i] = new Transform();
            context.nodes[i].getLocalTransform(t[i]);
        }
        TransformNodeOperation op = new TransformNodeOperation("move", context.nodes, this.originalNodeTransformsLS, t);
        context.editor.executeOperation(op);
    }

    @Override
    public void keyPressed(ManipulatorContext ctx) {
        if (this.moving)
            return;

        if (ctx.keyEvent.character == SWT.TAB) {
            if (this.space == LOCAL)
                this.space = WORLD;
            else
                this.space = LOCAL;
        }
    }

    @Override
    public void setName(String name) {
        this.name = name;
    }

    @Override
    public String getName() {
        return this.name;
    }
}
