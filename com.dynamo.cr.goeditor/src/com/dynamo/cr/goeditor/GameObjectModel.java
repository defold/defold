package com.dynamo.cr.goeditor;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

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

}
