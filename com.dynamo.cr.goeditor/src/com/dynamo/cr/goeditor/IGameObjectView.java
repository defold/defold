package com.dynamo.cr.goeditor;

import java.util.List;

import com.dynamo.cr.editor.core.IResourceType;

public interface IGameObjectView {

    public interface Presenter {
        public void onAddResourceComponent();
        public void onAddEmbeddedComponent();
        public void onRemoveComponent(Component component);
        public void onSetComponentId(Component component, String id);
        public void dispose();
    }

    public String openAddResourceComponentDialog();
    public IResourceType openAddEmbeddedComponentDialog();
    public void setComponents(List<Component> components);

}
