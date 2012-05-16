package com.dynamo.cr.go.core;

import java.lang.annotation.Annotation;
import java.util.List;
import java.util.Map;

import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.go.core.script.LuaPropertyParser;
import com.dynamo.cr.properties.DynamicProperties;
import com.dynamo.cr.properties.DynamicPropertyAccessor;
import com.dynamo.cr.properties.DynamicPropertyValidator;
import com.dynamo.cr.properties.IPropertyAccessor;
import com.dynamo.cr.properties.IPropertyDesc;
import com.dynamo.cr.properties.IValidator;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;

@SuppressWarnings("serial")
public class ComponentPropertyNode extends Node {
    private RefComponentNode ref;
    private Map<String, String> properties;

    public ComponentPropertyNode(RefComponentNode ref, Map<String, String> properties) {
        super();
        this.ref = ref;
        this.properties = properties;
    }

    public void setRefComponentNode(RefComponentNode ref) {
        this.ref = ref;
    }

    public String getComponentProperty(String id) {
        LuaPropertyParser.Property defProp = ref.getPropertyDefault(id);
        if (defProp != null) {
            String value = this.properties.get(id);
            if (value == null) {
                value = this.ref.getComponentProperty(id);
            }
            return value;
        }
        return null;
    }

    public Map<String, String> getComponentProperties() {
        return this.properties;
    }

    public String getDefaultComponentProperty(String id) {
        return this.ref.getComponentProperty(id);
    }

    public List<LuaPropertyParser.Property> getPropertyDefaults() {
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
            LuaPropertyParser.Property defProp = ref.getPropertyDefault(property);
            if (defProp != null) {
                properties.put(property, (String)value);
            } else {
                throw new RuntimeException(String.format("No such property %s", property));
            }

        }

        @Override
        public Object getValue(ComponentPropertyNode obj, String property, ISceneModel world) {
            String value = getComponentProperty(property);
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
            return new Object[0];
        }

        @Override
        public void resetValue(ComponentPropertyNode obj, String property, ISceneModel world) {
            LuaPropertyParser.Property defProp = ref.getPropertyDefault(property);
            if (defProp != null) {
                properties.remove(property);
            } else {
                throw new RuntimeException(String.format("No such property %s", property));
            }
        }

        @Override
        public boolean isOverridden(ComponentPropertyNode obj, String property, ISceneModel world) {
            LuaPropertyParser.Property defProp = ref.getPropertyDefault(property);
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

    @DynamicPropertyValidator
    public IValidator<Object, Annotation, ISceneModel> getDynamicValidator(ISceneModel world) {
        return ref.getDynamicValidator(world);
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
