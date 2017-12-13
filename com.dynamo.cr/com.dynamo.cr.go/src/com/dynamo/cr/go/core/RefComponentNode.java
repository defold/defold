package com.dynamo.cr.go.core;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;
import org.eclipse.swt.graphics.Image;

import com.dynamo.bob.pipeline.LuaScanner;
import com.dynamo.cr.go.Constants;
import com.dynamo.cr.properties.DynamicProperties;
import com.dynamo.cr.properties.DynamicPropertyAccessor;
import com.dynamo.cr.properties.IPropertyAccessor;
import com.dynamo.cr.properties.IPropertyDesc;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.properties.PropertyUtil;
import com.dynamo.cr.properties.Resource;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.gameobject.proto.GameObject.PropertyType;

@SuppressWarnings("serial")
public class RefComponentNode extends ComponentNode {

    @Property(editorType=EditorType.RESOURCE, extensions = {
            "camera", "collectionproxy", "collisionobject", "factory", "gui", "label", "light", "model", "particlefx", "script", "sound", "sprite", "tilegrid", "tilemap", "wav", "spinemodel"
    })
    @Resource
    @NotEmpty
    private String component;
    private transient ComponentTypeNode type;

    private Map<String, Object> prototypeProperties = new HashMap<String, Object>();
    private transient List<LuaScanner.Property> propertyDefaults = new ArrayList<LuaScanner.Property>();

    public RefComponentNode(ComponentTypeNode type) {
        super();
        this.type = type;
        if (this.type != null) {
            this.type.setModel(this.getModel());
            this.type.setFlagsRecursively(Flags.LOCKED);
            addChild(type);
            setTransformable(type.isTransformable());
        }
    }

    public String getComponent() {
        return this.component;
    }

    public void setComponent(String component) {
        this.component = component;
        reloadType();
        if (getId() == null) {
            int index = component.lastIndexOf('.');
            if (index >= 0) {
                setId(component.substring(index + 1));
            }
        }
    }

    public ComponentTypeNode getType() {
        return this.type;
    }

    @Override
    public void setModel(ISceneModel model) {
        super.setModel(model);
        if (model != null) {
            if (this.type == null) {
                reloadType();
            } else {
                reloadComponentProperties();
            }
        }
    }

    public List<LuaScanner.Property> getPropertyDefaults() {
        return this.propertyDefaults;
    }

    public void setPrototypeProperties(Map<String, Object> prototypeProperties) {
        this.prototypeProperties = prototypeProperties;
    }

    public IStatus validateComponent() {
        if (getModel() != null && this.component != null && !this.component.isEmpty()) {
            if (this.type == null) {
                int index = this.component.lastIndexOf('.');
                String message = null;
                if (index < 0) {
                    message = NLS.bind(Messages.RefComponentNode_component_UNKNOWN_TYPE, this.component);
                } else {
                    String ext = this.component.substring(index+1);
                    message = NLS.bind(Messages.RefComponentNode_component_INVALID_TYPE, ext);
                }
                return new Status(IStatus.ERROR, Constants.PLUGIN_ID, message);
            }
        }
        return Status.OK_STATUS;
    }

    @Override
    public String toString() {
        return String.format("%s (%s)", getId(), this.component);
    }

    @Override
    public boolean handleReload(IFile file, boolean childWasReloaded) {
        IFile componentFile = getModel().getFile(this.component);
        if (componentFile.exists() && componentFile.equals(file)) {
            if (reloadType()) {
                return true;
            }
        }
        return false;
    }

    private boolean reloadType() {
        ISceneModel model = getModel();
        if (model != null) {
            try {
                clearChildren();
                this.type = (ComponentTypeNode)model.loadNode(this.component);
                if (this.propertyDefaults != null) {
                    this.propertyDefaults.clear();
                } else {
                    this.propertyDefaults = new ArrayList<LuaScanner.Property>();
                }
                if (this.type != null) {
                    this.type.setFlagsRecursively(Flags.LOCKED);
                    addChild(this.type);
                    setTransformable(type.isTransformable());
                    reloadComponentProperties();
                }
            } catch (Throwable e) {
                // no reason to handle exception since having a null type is invalid state, will be caught in validateComponent below
            }
            return true;
        }
        return false;
    }

    @Override
    public Image getIcon() {
        if (this.type != null) {
            return this.type.getIcon();
        } else {
            return super.getIcon();
        }
    }

    public Object getComponentProperty(String id) {
        LuaScanner.Property defProp = getPropertyDefault(id);
        if (defProp != null) {
            Object value = this.prototypeProperties.get(id);
            if (value == null) {
                value = defProp.value;
            }
            return value;
        }
        return null;
    }

    public Map<String, Object> getPrototypeProperties() {
        return this.prototypeProperties;
    }

    public LuaScanner.Property getPropertyDefault(String id) {
        for (LuaScanner.Property property : this.propertyDefaults) {
            if (property.name.equals(id)) {
                return property;
            }
        }
        return null;
    }

    private void reloadComponentProperties() {
        if (this.component.substring(this.component.lastIndexOf('.')+1).equals("script")) {
            IFile file = getModel().getFile(this.component);
            try {
                InputStreamReader isr = new InputStreamReader(file.getContents());
                BufferedReader reader = new BufferedReader(isr);
                StringBuffer buffer = new StringBuffer();
                String line;
                while ((line = reader.readLine()) != null) {
                    buffer.append(line).append('\n');
                }
                isr.close();
                List<LuaScanner.Property> properties = LuaScanner.scanProperties(buffer.toString());
                for (LuaScanner.Property property : properties) {
                    if (property.status == LuaScanner.Property.Status.OK) {
                        this.propertyDefaults.add(property);
                    }
                }
            } catch (Exception e) {

            }
        }
    }

    @SuppressWarnings("unchecked")
    @DynamicProperties
    public IPropertyDesc<RefComponentNode, ISceneModel>[] getDynamicProperties() {
        IPropertyDesc<RefComponentNode, ISceneModel>[] descs = new IPropertyDesc[this.propertyDefaults.size()];
        int i = 0;
        for (LuaScanner.Property defProp : this.propertyDefaults) {
            if (defProp.status == LuaScanner.Property.Status.OK) {
                String name = defProp.name;
                String[] tokens = name.split("_");
                name = tokens[0];
                int tokenCount = tokens.length;
                for (int j = 1; j < tokenCount; ++j) {
                    name += tokens[j].substring(0, 1).toUpperCase() + tokens[j].substring(1);
                }
                descs[i] = GoPropertyUtil.typeToDesc(defProp.type, defProp.subType, defProp.name, name, "");
            }
            ++i;
        }
        return descs;
    }

    class Accessor implements IPropertyAccessor<RefComponentNode, ISceneModel> {

        @Override
        public void setValue(RefComponentNode obj, String property, Object value,
                ISceneModel world) {
            LuaScanner.Property defProp = getPropertyDefault(property);
            if (defProp != null) {
                Object newValue = null;
                if (prototypeProperties.containsKey(property)) {
                    newValue = PropertyUtil.mergeValue(prototypeProperties.get(property), value);
                } else {
                    newValue = PropertyUtil.mergeValue(defProp.value, value);
                }
                prototypeProperties.put(property, newValue);
            } else {
                throw new RuntimeException(String.format("No such property %s", property));
            }

        }

        @Override
        public Object getValue(RefComponentNode obj, String property, ISceneModel world) {
            Object value = getComponentProperty(property);
            if (value != null) {
                return value;
            } else {
                throw new RuntimeException(String.format("No such property %s", property));
            }
        }

        @Override
        public boolean isEditable(RefComponentNode obj, String property,
                ISceneModel world) {
            return true;
        }

        @Override
        public boolean isVisible(RefComponentNode obj, String property,
                ISceneModel world) {
            return true;
        }

        @Override
        public Object[] getPropertyOptions(RefComponentNode obj, String property,
                ISceneModel world) {
            for (LuaScanner.Property defProp : propertyDefaults) {
                if (defProp.name.equals(property)) {
                    if (defProp.type == PropertyType.PROPERTY_TYPE_URL) {
                        List<Node> children = getParent().getChildren();
                        String[] ids = new String[children.size()];
                        int i = 0;
                        for (Node child : children) {
                            ids[i] = "#" + ((ComponentNode)child).getId();
                            ++i;
                        }
                        return ids;
                    }
                }
            }
            return new Object[0];
        }

        @Override
        public void resetValue(RefComponentNode obj, String property, ISceneModel world) {
            LuaScanner.Property defProp = getPropertyDefault(property);
            if (defProp != null) {
                prototypeProperties.remove(property);
            } else {
                throw new RuntimeException(String.format("No such property %s", property));
            }
        }

        @Override
        public boolean isOverridden(RefComponentNode obj, String property, ISceneModel world) {
            LuaScanner.Property defProp = getPropertyDefault(property);
            if (defProp != null) {
                return prototypeProperties.containsKey(property);
            } else {
                throw new RuntimeException(String.format("No such property %s", property));
            }
        }

    }

    @DynamicPropertyAccessor
    public IPropertyAccessor<RefComponentNode, ISceneModel> getDynamicAccessor(ISceneModel world) {
        return new Accessor();
    }

}
