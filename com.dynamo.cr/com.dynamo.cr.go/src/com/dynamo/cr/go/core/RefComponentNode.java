package com.dynamo.cr.go.core;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.lang.annotation.Annotation;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.go.Constants;
import com.dynamo.cr.go.core.script.LuaPropertyParser;
import com.dynamo.cr.properties.DynamicProperties;
import com.dynamo.cr.properties.DynamicPropertyAccessor;
import com.dynamo.cr.properties.DynamicPropertyValidator;
import com.dynamo.cr.properties.IPropertyAccessor;
import com.dynamo.cr.properties.IPropertyDesc;
import com.dynamo.cr.properties.IValidator;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.properties.Resource;
import com.dynamo.cr.properties.descriptors.TextPropertyDesc;
import com.dynamo.cr.sceneed.core.ISceneModel;

@SuppressWarnings("serial")
public class RefComponentNode extends ComponentNode {

    @Property(editorType=EditorType.RESOURCE, extensions = {
            "camera", "collectionproxy", "collisionobject", "factory", "gui", "light", "model", "particlefx", "script", "sound", "sprite", "tilegrid", "tilemap", "wav"
    })
    @Resource
    @NotEmpty
    private String component;
    private transient ComponentTypeNode type;

    private Map<String, String> prototypeProperties = new HashMap<String, String>();
    private transient List<LuaPropertyParser.Property> propertyDefaults = new ArrayList<LuaPropertyParser.Property>();

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

    public List<LuaPropertyParser.Property> getPropertyDefaults() {
        return this.propertyDefaults;
    }

    public void setPrototypeProperties(Map<String, String> prototypeProperties) {
        this.prototypeProperties = prototypeProperties;
    }

    public IStatus validateComponent() {
        if (getModel() != null && this.component != null && !this.component.isEmpty()) {
            if (this.type != null) {
                IStatus status = this.type.getStatus();
                if (!status.isOK()) {
                    return new Status(IStatus.ERROR, Constants.PLUGIN_ID, Messages.RefComponentNode_component_INVALID_REFERENCE);
                }
            } else {
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
    public boolean handleReload(IFile file) {
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
                    this.propertyDefaults = new ArrayList<LuaPropertyParser.Property>();
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

    public String getComponentProperty(String id) {
        LuaPropertyParser.Property defProp = getPropertyDefault(id);
        if (defProp != null) {
            String value = this.prototypeProperties.get(id);
            if (value == null) {
                value = defProp.getValue();
            }
            return value;
        }
        return null;
    }

    public Map<String, String> getPrototypeProperties() {
        return this.prototypeProperties;
    }

    public LuaPropertyParser.Property getPropertyDefault(String id) {
        for (LuaPropertyParser.Property property : this.propertyDefaults) {
            if (property.getName().equals(id)) {
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
                LuaPropertyParser.Property[] properties = LuaPropertyParser.parse(buffer.toString());
                for (LuaPropertyParser.Property property : properties) {
                    if (property.getStatus() == LuaPropertyParser.Property.Status.OK) {
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
        for (LuaPropertyParser.Property defProp : this.propertyDefaults) {
            if (defProp.getStatus() == LuaPropertyParser.Property.Status.OK) {
                String name = defProp.getName();
                String[] tokens = name.split("_");
                name = tokens[0];
                int tokenCount = tokens.length;
                for (int j = 1; j < tokenCount; ++j) {
                    name += tokens[j].substring(0, 1).toUpperCase() + tokens[j].substring(1);
                }
                descs[i] = new TextPropertyDesc<RefComponentNode, ISceneModel>(defProp.getName(), name, "", EditorType.DEFAULT);
            }
            ++i;
        }
        return descs;
    }

    class Accessor implements IPropertyAccessor<RefComponentNode, ISceneModel> {

        @Override
        public void setValue(RefComponentNode obj, String property, Object value,
                ISceneModel world) {
            LuaPropertyParser.Property defProp = getPropertyDefault(property);
            if (defProp != null) {
                prototypeProperties.put(property, (String)value);
            } else {
                throw new RuntimeException(String.format("No such property %s", property));
            }

        }

        @Override
        public Object getValue(RefComponentNode obj, String property, ISceneModel world) {
            String value = getComponentProperty(property);
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
            return new Object[0];
        }

        @Override
        public void resetValue(RefComponentNode obj, String property, ISceneModel world) {
            LuaPropertyParser.Property defProp = getPropertyDefault(property);
            if (defProp != null) {
                prototypeProperties.remove(property);
            } else {
                throw new RuntimeException(String.format("No such property %s", property));
            }
        }

        @Override
        public boolean isOverridden(RefComponentNode obj, String property, ISceneModel world) {
            LuaPropertyParser.Property defProp = getPropertyDefault(property);
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

    class Validator implements IValidator<Object, Annotation, ISceneModel> {

        @Override
        public IStatus validate(Annotation validationParameters, Object object,
                String property, Object value, ISceneModel world) {
            LuaPropertyParser.Property p = getPropertyDefault(property);
            switch (p.getType()) {
            case NUMBER:
                try {
                    @SuppressWarnings("unused")
                    Double v = Double.valueOf((String)value);
                } catch (NumberFormatException e) {
                    return new Status(IStatus.ERROR, Constants.PLUGIN_ID, NLS.bind(Messages.RefComponentNode_component_INVALID_NUMBER, value));
                }
                break;
            case HASH:
                break;
            case URL:
                if (!value.equals("") && ((String)value).charAt(0) != '/') {
                    return new Status(IStatus.ERROR, Constants.PLUGIN_ID, NLS.bind(Messages.RefComponentNode_component_NON_GLOBAL_URL, value));
                }
            }
            return Status.OK_STATUS;
        }

    }

    @DynamicPropertyValidator
    public IValidator<Object, Annotation, ISceneModel> getDynamicValidator(ISceneModel world) {
        return new Validator();
    }
}
