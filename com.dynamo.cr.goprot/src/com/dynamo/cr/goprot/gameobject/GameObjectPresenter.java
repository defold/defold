package com.dynamo.cr.goprot.gameobject;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import com.dynamo.cr.goprot.core.INodeView;
import com.dynamo.cr.goprot.core.NodeModel;
import com.dynamo.cr.goprot.core.NodePresenter;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc.Builder;
import com.google.inject.Inject;
import com.google.protobuf.TextFormat;

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
