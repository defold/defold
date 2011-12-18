package com.dynamo.cr.ddfeditor.manipulators;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.ddfeditor.scene.SphereCollisionShapeNode;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.ui.RootManipulator;
import com.dynamo.cr.sceneed.ui.ScaleAxisManipulator;

public class SphereRadiusManipulator extends RootManipulator {

    private ScaleAxisManipulator scaleManipulator;
    private double originalRadius;
    private boolean radiusChanged = false;
    private double radius;

    public SphereRadiusManipulator() {
        scaleManipulator = new ScaleAxisManipulator(this, new float[] {1, 0, 0, 1});
        addChild(scaleManipulator);
    }

    @Override
    public boolean match(Object[] selection) {
        if (selection.length > 0 && selection[0] instanceof SphereCollisionShapeNode) {
            return true;
        }
        return false;
    }

    @Override
    protected void transformChanged() {
    }

    @Override
    public void manipulatorChanged(Manipulator manipulator) {
        radiusChanged = true;
        double factor = 0.5;
        for (Node node : getSelection()) {
            if (node instanceof SphereCollisionShapeNode) {
                SphereCollisionShapeNode sphere = (SphereCollisionShapeNode) node;
                radius = originalRadius + scaleManipulator.getDistance() * factor;
                radius = Math.max(0, radius);
                sphere.setRadius(radius);
            }
        }
    }

    @Override
    protected void selectionChanged() {
        Node node = getSelection().get(0);
        originalRadius = ((SphereCollisionShapeNode) node).getRadius();
        setTranslation(node.getTranslation());
    }

    @Override
    public void refresh() {
        selectionChanged();
    }

    @Override
    public void mouseUp(MouseEvent e) {
        if (radiusChanged) {
            Node node = getSelection().get(0);
            SphereCollisionShapeNode sphere = (SphereCollisionShapeNode) node;
            // We must reset radius. Otherwise setPropertyValue will detect no change and return null operation
            sphere.setRadius(originalRadius);
            // Set original radius to current in order to "continue" scaling
            originalRadius = radius;

            @SuppressWarnings("unchecked")
            IPropertyModel<Node, ISceneModel> propertyModel = (IPropertyModel<Node, ISceneModel>) node.getAdapter(IPropertyModel.class);
            IUndoableOperation operation = propertyModel.setPropertyValue("radius", radius);
            if (operation != null) {
                getController().executeOperation(operation);
            }
        }
    }

}
