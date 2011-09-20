package com.dynamo.cr.goeditor;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.io.Reader;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import javax.inject.Inject;

import com.dynamo.cr.editor.core.IResourceTypeRegistry;
import com.dynamo.gameobject.proto.GameObject.ComponentDesc;
import com.dynamo.gameobject.proto.GameObject.EmbeddedComponentDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.google.protobuf.TextFormat;

public class GameObjectModel {

    @Inject IResourceTypeRegistry resourceTypeRegistry;

    private List<Component> components = new ArrayList<Component>();

    public void addComponent(Component component) {
        int index = component.getIndex();

        if (index >= 0) {
            components.add(index, component);
        } else {
            components.add(component);
        }
    }

    public void removeComponent(Component component) {
        int index = components.indexOf(component);
        assert index >= 0;
        component.setIndex(index);
        components.remove(index);
    }

    public List<Component> getComponents() {
        return Collections.unmodifiableList(components);
    }

    private Component componentById(String id) {
        for (Component c : components) {
            if (id.equals(c.getId()))
                return c;
        }
        return null;
    }

    public String getUniqueId(String baseId) {
        String id = baseId;

        int i = 0;
        while (componentById(id) != null) {
            id = String.format("%s%d", baseId, i++);
        }

        return id;
    }

    public boolean isValid() {
        Set<String> ids = new HashSet<String>();
        for (Component c : components) {
            ids.add(c.getId());
        }
        return ids.size() == components.size();
    }

    public void setComponentId(Component component, String id) {
        Component c = componentById(component.getId());
        c.setId(id);
    }

    public void save(OutputStream output) throws IOException {
        PrototypeDesc.Builder builder = PrototypeDesc.newBuilder();
        for (Component component : components) {
            component.addComponenent(builder);
        }
        PrintWriter writer = new PrintWriter(output);
        TextFormat.print(builder.build(), writer);
        writer.flush();
    }

    public void load(InputStream input) throws IOException {
        PrototypeDesc.Builder builder = PrototypeDesc.newBuilder();
        Reader reader = new InputStreamReader(input);
        TextFormat.merge(reader, builder);

        components.clear();
        PrototypeDesc prototypeDesc = builder.build();
        for (ComponentDesc component : prototypeDesc.getComponentsList()) {
            addComponent(new ResourceComponent(resourceTypeRegistry, component));
        }

        for (EmbeddedComponentDesc component : prototypeDesc.getEmbeddedComponentsList()) {
            addComponent(new EmbeddedComponent(resourceTypeRegistry, component));
        }
    }

}
