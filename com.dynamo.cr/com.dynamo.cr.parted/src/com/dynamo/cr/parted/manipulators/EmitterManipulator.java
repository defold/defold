package com.dynamo.cr.parted.manipulators;

import javax.vecmath.Quat4d;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.parted.nodes.EmitterNode;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.types.ValueSpread;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.ui.RootManipulator;
import com.dynamo.cr.sceneed.ui.ScaleAxisManipulator;
import com.dynamo.particle.proto.Particle.EmitterKey;

@SuppressWarnings("serial")
public class EmitterManipulator extends RootManipulator {

    private ScaleAxisManipulator xScaleManipulator;
    private ScaleAxisManipulator yScaleManipulator;
    private ScaleAxisManipulator zScaleManipulator;
    private boolean sizeChanged = false;
    private double width, height, depth;
    private double originalWidth, originalHeight, originalDepth;

    public EmitterManipulator() {
        xScaleManipulator = new ScaleAxisManipulator(this, new float[] {1, 0, 0, 1});
        yScaleManipulator = new ScaleAxisManipulator(this, new float[] {0, 1, 0, 1});
        zScaleManipulator = new ScaleAxisManipulator(this, new float[] {0, 0, 1, 1});

        yScaleManipulator.setRotation(new Quat4d(0.5, 0.5, 0.5, 0.5));
        zScaleManipulator.setRotation(new Quat4d(-0.5, -0.5, -0.5, 0.5));

        addChild(xScaleManipulator);
        addChild(yScaleManipulator);
        addChild(zScaleManipulator);
    }

    @Override
    public boolean match(Object[] selection) {
        if (selection.length > 0 && selection[0] instanceof EmitterNode) {
            return true;
        }
        return false;
    }

    @Override
    protected void transformChanged() {
    }

    private void setSize(EmitterNode node, int component, double value) {
        EmitterKey key = EmitterKey.valueOf(EmitterKey.EMITTER_KEY_SIZE_X.getNumber() + component);
        ValueSpread vs = node.getProperty(key.toString());
        vs.setValue(value);
        node.setProperty(key.toString(), vs);
    }

    private double getSize(EmitterNode node, int component) {
        EmitterKey key = EmitterKey.valueOf(EmitterKey.EMITTER_KEY_SIZE_X.getNumber() + component);
        ValueSpread vs = node.getProperty(key.toString());
        return vs.getValue();
    }

    private boolean isAnimated(EmitterNode node, int component) {
        EmitterKey key = EmitterKey.valueOf(EmitterKey.EMITTER_KEY_SIZE_X.getNumber() + component);
        ValueSpread vs = node.getProperty(key.toString());
        return vs.isAnimated();
    }

    @Override
    public void manipulatorChanged(Manipulator manipulator) {
        sizeChanged = true;
        double factor = 2.0;
        for (Node node : getSelection()) {
            if (node instanceof EmitterNode) {
                EmitterNode emitter = (EmitterNode) node;
                if (manipulator == xScaleManipulator) {
                    width = originalWidth + xScaleManipulator.getDistance() * factor;
                    width = Math.max(0, width);
                    setSize(emitter, 0, width);
                } else if (manipulator == yScaleManipulator) {
                    height = originalHeight + yScaleManipulator.getDistance() * factor;
                    height = Math.max(0, height);
                    setSize(emitter, 1, height);
                } else if (manipulator == zScaleManipulator) {
                    depth = originalDepth + zScaleManipulator.getDistance() * factor;
                    depth = Math.max(0, depth);
                    setSize(emitter, 2, depth);
                }
            }
        }
    }

    @Override
    protected void selectionChanged() {
        EmitterNode emitter = (EmitterNode) getSelection().get(0);
        width = originalWidth = getSize(emitter, 0);
        height = originalHeight = getSize(emitter, 1);
        depth = originalDepth = getSize(emitter, 2);
        setTranslation(emitter.getTranslation());
        setRotation(emitter.getRotation());
    }

    @Override
    public void refresh() {
        EmitterNode emitter = (EmitterNode) getSelection().get(0);

        this.xScaleManipulator.setEnabled(true);
        this.yScaleManipulator.setEnabled(true);
        this.zScaleManipulator.setEnabled(true);

        this.xScaleManipulator.setVisible(true);
        this.yScaleManipulator.setVisible(true);
        this.zScaleManipulator.setVisible(true);

        switch (emitter.getEmitterType()) {
        case EMITTER_TYPE_SPHERE:
            this.xScaleManipulator.setEnabled(!isAnimated(emitter, 0));
            this.yScaleManipulator.setEnabled(false);
            this.zScaleManipulator.setEnabled(false);
            this.yScaleManipulator.setVisible(false);
            this.zScaleManipulator.setVisible(false);
            break;
        case EMITTER_TYPE_CONE:
            this.xScaleManipulator.setEnabled(!isAnimated(emitter, 0));
            this.yScaleManipulator.setEnabled(!isAnimated(emitter, 1));
            this.zScaleManipulator.setEnabled(false);
            this.zScaleManipulator.setVisible(false);
            break;
        case EMITTER_TYPE_BOX:
            this.xScaleManipulator.setEnabled(!isAnimated(emitter, 0));
            this.yScaleManipulator.setEnabled(!isAnimated(emitter, 1));
            this.zScaleManipulator.setEnabled(!isAnimated(emitter, 2));
            break;

        }
    }

    @Override
    public void mouseUp(MouseEvent e) {
        if (sizeChanged) {
            Node node = getSelection().get(0);
            EmitterNode emitter = (EmitterNode) node;
            // We must reset. Otherwise setPropertyValue will detect no change and return null operation
            setSize(emitter, 0, originalWidth);
            setSize(emitter, 1, originalHeight);
            setSize(emitter, 2, originalDepth);

            @SuppressWarnings("unchecked")
            IPropertyModel<Node, ISceneModel> propertyModel = (IPropertyModel<Node, ISceneModel>) node.getAdapter(IPropertyModel.class);

            IUndoableOperation operation = null;
            if (width != originalWidth) {
                operation = propertyModel.setPropertyValue(EmitterKey.EMITTER_KEY_SIZE_X.toString(), new Double[] { width, null });
            } else if (height != originalHeight) {
                operation = propertyModel.setPropertyValue(EmitterKey.EMITTER_KEY_SIZE_Y.toString(), new Double[] { height, null });
            } else if (depth != originalDepth) {
                operation = propertyModel.setPropertyValue(EmitterKey.EMITTER_KEY_SIZE_Z.toString(), new Double[] { depth, null });
            }

            if (operation != null) {
                getController().executeOperation(operation);
            }

            // Set original values to current in order to "continue" scaling
            originalWidth = width;
            originalHeight = height;
            originalDepth = depth;
        }
    }

}
