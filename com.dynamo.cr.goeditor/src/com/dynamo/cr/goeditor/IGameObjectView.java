package com.dynamo.cr.goeditor;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.List;

import com.dynamo.cr.editor.core.IResourceType;

public interface IGameObjectView {

    public interface Presenter {
        public void onAddResourceComponent();
        public void onAddEmbeddedComponent();
        public void onRemoveComponent(Component component);
        public void onSetComponentId(Component component, String id);
        public boolean isDirty();
        public void onSave(OutputStream output) throws IOException;
        public void onLoad(InputStream input) throws IOException;
    }


    public String openAddResourceComponentDialog();
    public IResourceType openAddEmbeddedComponentDialog(IResourceType[] resourceTypes);
    public void setComponents(List<Component> components);
    public boolean setFocus();
    public void create(String name);
    public void setDirty(boolean dirty);

}
