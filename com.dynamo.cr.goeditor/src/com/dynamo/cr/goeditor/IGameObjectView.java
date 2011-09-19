package com.dynamo.cr.goeditor;

import com.google.protobuf.Message;

public interface IGameObjectView {

    public interface Presenter {
        public void onAddResourceComponent();
        public void onAddEmbeddedComponent();
        public void onRemoveComponent(Component component);
    }

    public String openAddResourceComponentDialog();
    public Message openAddEmbeddedComponentDialog();

}
