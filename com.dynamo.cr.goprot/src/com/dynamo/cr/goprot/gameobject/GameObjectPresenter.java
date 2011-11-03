package com.dynamo.cr.goprot.gameobject;

import com.dynamo.cr.goprot.core.INodeView;
import com.dynamo.cr.goprot.core.NodeModel;
import com.dynamo.cr.goprot.core.NodePresenter;
import com.google.inject.Inject;

public class GameObjectPresenter extends NodePresenter {

    @Inject
    public GameObjectPresenter(NodeModel model, INodeView view) {
        super(model, view);
    }

    public void onAddComponent(GameObjectNode parent, ComponentNode child) {
        this.model.executeOperation(new AddComponentOperation(parent, child));
    }

    public void onRemoveComponent(GameObjectNode parent, ComponentNode child) {
        this.model.executeOperation(new RemoveComponentOperation(parent, child));
    }
}
