package com.dynamo.cr.goeditor;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public class GameObjectModel {

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

    public boolean isOk() {
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

}
