package com.dynamo.cr.tileeditor.operations;

import java.util.List;

import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.operations.AddChildrenOperation;

public class AddImagesNodeOperation extends AddChildrenOperation {

    public AddImagesNodeOperation(Node parent, List<Node> children, IPresenterContext presenterContext) {
        super("Add Images", parent, children, presenterContext);
    }

}
