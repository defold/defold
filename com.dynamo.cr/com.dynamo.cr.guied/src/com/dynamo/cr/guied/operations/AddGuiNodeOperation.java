package com.dynamo.cr.guied.operations;

import javax.vecmath.Point3d;
import javax.vecmath.Vector3d;

import com.dynamo.cr.guied.core.GuiNode;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.operations.AddChildrenOperation;

public class AddGuiNodeOperation extends AddChildrenOperation {

    public AddGuiNodeOperation(Node parent, GuiNode node, IPresenterContext presenterContext) {
        super("Add Node", parent, node, presenterContext);
        Point3d position = new Point3d();
        presenterContext.getCameraFocusPoint(position);
        node.setTranslation(position);
        node.setSize(new Vector3d(200.0, 100.0, 0.0));
        node.setInheritAlpha(true);
    }

}
