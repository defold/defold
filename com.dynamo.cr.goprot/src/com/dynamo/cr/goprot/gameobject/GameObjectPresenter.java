package com.dynamo.cr.goprot.gameobject;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import com.dynamo.cr.goprot.core.INodeView;
import com.dynamo.cr.goprot.core.NodeModel;
import com.dynamo.cr.goprot.core.NodePresenter;
import com.dynamo.cr.goprot.sprite.SpriteNode;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc.Builder;
import com.google.inject.Inject;
import com.google.protobuf.TextFormat;

public class GameObjectPresenter extends NodePresenter {

    @Inject
    public GameObjectPresenter(NodeModel model, INodeView view) {
        super(model, view);
    }

    public void onAddComponent(GameObjectNode parent, String componentType) {
        // TODO: Use node factory
        ComponentNode child = null;
        if (componentType.equals("sprite")) {
            child = new ComponentNode(new SpriteNode());
        }
        if (child != null) {
            this.model.executeOperation(new AddComponentOperation(parent, child));
        } else {
            throw new UnsupportedOperationException("Component type " + componentType + " not registered.");
        }
    }

    public void onRemoveComponent(ComponentNode child) {
        this.model.executeOperation(new RemoveComponentOperation(child));
    }

    @Override
    public void onLoad(InputStream contents) throws IOException {
        // TODO: Move to loader
        InputStreamReader reader = new InputStreamReader(contents);
        Builder builder = PrototypeDesc.newBuilder();
        TextFormat.merge(reader, builder);
        @SuppressWarnings("unused")
        PrototypeDesc desc = builder.build();
        GameObjectNode gameObject = new GameObjectNode();
        // TODO: Fill out game object
        this.model.setRoot(gameObject);
    }
}
