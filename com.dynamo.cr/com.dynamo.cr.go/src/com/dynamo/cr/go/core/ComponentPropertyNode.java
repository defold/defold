package com.dynamo.cr.go.core;

import java.util.Arrays;
import java.util.List;
import java.util.Map;

import org.eclipse.swt.graphics.Image;

import com.dynamo.bob.pipeline.LuaScanner;
import com.dynamo.cr.properties.DynamicProperties;
import com.dynamo.cr.properties.DynamicPropertyAccessor;
import com.dynamo.cr.properties.IPropertyAccessor;
import com.dynamo.cr.properties.IPropertyDesc;
import com.dynamo.cr.properties.PropertyUtil;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.gameobject.proto.GameObject.PropertyType;

@SuppressWarnings("serial")
public class ComponentPropertyNode extends Node {
    private RefComponentNode ref;
    private Map<String, Object> properties;

    public ComponentPropertyNode(RefComponentNode ref, Map<String, Object> properties) {
        super();
        this.ref = ref;
        this.properties = properties;
    }

    public void setRefComponentNode(RefComponentNode ref) {
        this.ref = ref;
    }

    public Object getComponentProperty(String id) {
        LuaScanner.Property defProp = ref.getPropertyDefault(id);
        if (defProp != null) {
            Object value = this.properties.get(id);
            if (value == null) {
                value = this.ref.getComponentProperty(id);
            }
            return value;
        }
        return null;
    }

    public Map<String, Object> getComponentProperties() {
        return this.properties;
    }

    public Object getDefaultComponentProperty(String id) {
        return this.ref.getComponentProperty(id);
    }

    public List<LuaScanner.Property> getPropertyDefaults() {
        return this.ref.getPropertyDefaults();
    }

    @DynamicProperties
    public IPropertyDesc<RefComponentNode, ISceneModel>[] getDynamicProperties() {
        return ref.getDynamicProperties();
    }

    class Accessor implements IPropertyAccessor<ComponentPropertyNode, ISceneModel> {

        @Override
        public void setValue(ComponentPropertyNode obj, String property, Object value,
                ISceneModel world) {
            LuaScanner.Property defProp = ref.getPropertyDefault(property);
            if (defProp != null) {
                Object newValue = null;
                if (properties.containsKey(property)) {
                    newValue = PropertyUtil.mergeValue(properties.get(property), value);
                } else {
                    newValue = PropertyUtil.mergeValue(defProp.value, value);
                }
                properties.put(property, newValue);
            } else {
                throw new RuntimeException(String.format("No such property %s", property));
            }

        }

        @Override
        public Object getValue(ComponentPropertyNode obj, String property, ISceneModel world) {
            Object value = getComponentProperty(property);
            if (value != null) {
                return value;
            } else {
                throw new RuntimeException(String.format("No such property %s", property));
            }
        }

        @Override
        public boolean isEditable(ComponentPropertyNode obj, String property,
                ISceneModel world) {
            return true;
        }

        @Override
        public boolean isVisible(ComponentPropertyNode obj, String property,
                ISceneModel world) {
            return true;
        }

        @Override
        public Object[] getPropertyOptions(ComponentPropertyNode obj, String property,
                ISceneModel world) {
            LuaScanner.Property defProp = ref.getPropertyDefault(property);
            if (defProp != null && defProp.type == PropertyType.PROPERTY_TYPE_URL) {
                Object[] urls = GoPropertyUtil.extractRelativeURLs(ref);
                Arrays.sort(urls);
                return urls;
            }
            return new Object[0];
        }

        @Override
        public void resetValue(ComponentPropertyNode obj, String property, ISceneModel world) {
            LuaScanner.Property defProp = ref.getPropertyDefault(property);
            if (defProp != null) {
                properties.remove(property);
            } else {
                throw new RuntimeException(String.format("No such property %s", property));
            }
        }

        @Override
        public boolean isOverridden(ComponentPropertyNode obj, String property, ISceneModel world) {
            LuaScanner.Property defProp = ref.getPropertyDefault(property);
            if (defProp != null) {
                return properties.containsKey(property);
            } else {
                throw new RuntimeException(String.format("No such property %s", property));
            }
        }

    }

    @DynamicPropertyAccessor
    public IPropertyAccessor<ComponentPropertyNode, ISceneModel> getDynamicAccessor(ISceneModel world) {
        return new Accessor();
    }

    public String getId() {
        return ref.getId();
    }

    @Override
    public String toString() {
        return ref.toString();
    }

    @Override
    public Image getIcon() {
        return ref.getIcon();
    }
}
