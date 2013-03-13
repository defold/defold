package com.dynamo.cr.go.core;

import java.util.Arrays;
import java.util.List;
import java.util.Map;

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
public class ComponentPropertyNode extends PropertyNode<RefComponentNode> {
    private Map<String, Object> properties;

    public ComponentPropertyNode(RefComponentNode node, Map<String, Object> properties) {
        super(node);
        this.properties = properties;
    }

    public Object getComponentProperty(String id) {
        LuaScanner.Property defProp = getNode().getPropertyDefault(id);
        if (defProp != null) {
            Object value = this.properties.get(id);
            if (value == null) {
                value = getPropertyFromAncestor(getParent(), "", getId(), id);
            }
            if (value == null) {
                value = getNode().getComponentProperty(id);
            }
            return value;
        }
        return null;
    }

    private Object getPropertyFromAncestor(Node node, String path, String componentId, String propertyId) {
        if (node instanceof GameObjectPropertyNode) {
            GameObjectPropertyNode goProp = (GameObjectPropertyNode)node;
            // Game objects can be recurring, search for collection if so
            if (!path.isEmpty()) {
                return getPropertyFromAncestor(goProp.getParent(), path, componentId, propertyId);
            } else {
                Object result = getPropertyFromAncestor(goProp.getParent(), goProp.getId(), componentId, propertyId);
                if (result == null) {
                    GameObjectInstanceNode goi = goProp.getNode();
                    if (goi instanceof RefGameObjectInstanceNode) {
                        RefGameObjectInstanceNode ref = (RefGameObjectInstanceNode)goi;
                        Map<String, Object> properties = ref.getComponentProperties(componentId);
                        result = properties.get(propertyId);
                    }
                }
                return result;
            }
        } else if (node instanceof CollectionPropertyNode) {
            CollectionPropertyNode collProp = (CollectionPropertyNode)node;
            Object result = getPropertyFromAncestor(collProp.getParent(), collProp.getId() + "/" + path, componentId, propertyId);
            if (result == null) {
                Map<String, Object> properties = collProp.getNode().getInstanceProperties(path, componentId);
                result = properties.get(propertyId);
            }
            return result;
        }
        return null;
    }

    public Map<String, Object> getComponentProperties() {
        return this.properties;
    }

    public Object getDefaultComponentProperty(String id) {
        return getNode().getComponentProperty(id);
    }

    public List<LuaScanner.Property> getPropertyDefaults() {
        return getNode().getPropertyDefaults();
    }

    @DynamicProperties
    public IPropertyDesc<RefComponentNode, ISceneModel>[] getDynamicProperties() {
        return getNode().getDynamicProperties();
    }

    class Accessor implements IPropertyAccessor<ComponentPropertyNode, ISceneModel> {

        @Override
        public void setValue(ComponentPropertyNode obj, String property, Object value,
                ISceneModel world) {
            LuaScanner.Property defProp = getNode().getPropertyDefault(property);
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
            LuaScanner.Property defProp = getNode().getPropertyDefault(property);
            if (defProp != null && defProp.type == PropertyType.PROPERTY_TYPE_URL) {
                Object[] urls = GoPropertyUtil.extractRelativeURLs(getNode());
                Arrays.sort(urls);
                return urls;
            }
            return new Object[0];
        }

        @Override
        public void resetValue(ComponentPropertyNode obj, String property, ISceneModel world) {
            LuaScanner.Property defProp = getNode().getPropertyDefault(property);
            if (defProp != null) {
                properties.remove(property);
            } else {
                throw new RuntimeException(String.format("No such property %s", property));
            }
        }

        @Override
        public boolean isOverridden(ComponentPropertyNode obj, String property, ISceneModel world) {
            LuaScanner.Property defProp = getNode().getPropertyDefault(property);
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
        return getNode().getId();
    }

    @Override
    protected void sortChildren() {
        // Leaf so no sorting necessary
    }
}
