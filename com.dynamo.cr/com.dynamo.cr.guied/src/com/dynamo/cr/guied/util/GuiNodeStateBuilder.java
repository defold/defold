package com.dynamo.cr.guied.util;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;
import java.util.HashMap;
import java.util.Set;
import java.util.List;
import java.util.Map;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.guied.core.GuiNode;
import com.dynamo.cr.guied.core.GuiSceneLoader;
import com.dynamo.cr.guied.core.GuiSceneNode;
import com.dynamo.cr.guied.core.TemplateNode;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.gui.proto.Gui.NodeDesc;
import com.google.protobuf.Descriptors.FieldDescriptor;

@SuppressWarnings("serial")
public class GuiNodeStateBuilder implements Serializable {
    private static Logger logger = LoggerFactory.getLogger(GuiNodeStateBuilder.class);
    private static final String defaultStateId = "Default";
    private static String stateId = "";
    private static ISceneModel stateModel = null;
    private static PropertyNameFieldMap propertyNameFieldMap = new PropertyNameFieldMap();

    private transient HashMap<String, NodeDesc.Builder> stateMap = new HashMap<String, NodeDesc.Builder>();
    private transient HashMap<String, NodeDesc.Builder> stateMapDefault = null;

    private static class PropertyNameFieldMap {
        HashMap<String, FieldDescriptor> map = new HashMap<String, FieldDescriptor>(64);

        // mapping from field name convention to camelcase naming (google protocol buffer naming convention)
        private PropertyNameFieldMap() {
            List<FieldDescriptor> fields = NodeDesc.getDescriptor().getFields();
            for(FieldDescriptor field : fields) {
                String fieldName = field.getName().substring(0, 1).toUpperCase() + field.getName().substring(1);
                String propertyName = "";
                for(int i = 0; i < fieldName.length(); i++) {
                    char c = fieldName.charAt(i);
                    if(c == '_') {
                        i++;
                        c = Character.toUpperCase(fieldName.charAt(i));
                    }
                    propertyName += c;
                }
                map.put(propertyName, field);
            }
        }
    }

    /**
     * If differing from default state value, sets the states field to value, which then becomes overridden until reset is applied
     * @param node GuiNode
     * @param id accessor id of field (camelcase property name, typed as google protocol buffer naming convention)
     * @param value Value to test/set. Must be of same type as field value
     * @return False if the value is equal to the default state. True if unequal or has previously been set unequal
     */
    public static Boolean setField(GuiNode node, String id , Object value) {
        GuiNodeStateBuilder states = node.getStateBuilder();
        String stateId = getStateId(node);
        if(getDefaultBuilders(states) == null) {
            if(defaultStateId.equals(stateId)) {
                return false;
            }
        }
        NodeDesc.Builder b = states.stateMap.getOrDefault(stateId, null);
        if(b == null) {
            b = NodeDesc.newBuilder();
            states.stateMap.put(stateId, b);
        }
        FieldDescriptor field = propertyNameFieldMap.map.getOrDefault(id, null);
        if(field == null) {
            logger.error("Field '" + id + "' not found in NodeDesc.");
            return false;
        }
        if(b.hasField(field)) {
            return true;
        }
        if(field.getType() == FieldDescriptor.Type.ENUM) {
            value = ((com.google.protobuf.ProtocolMessageEnum) value).getValueDescriptor();
        }
        NodeDesc.Builder defaultBuilder = getDefaultBuilderForField(states, stateId, field);
        if((defaultBuilder != null) && (!defaultBuilder.getField(field).equals(value))) {
            b.setField(field, value);
            return true;
        }
        return false;
    }

    /**
     * Reset states field value to the default state value and resets the overridden state.
     * @param node GuiNode
     * @param id accessor id of field (camelcase property name, typed as google protocol buffer naming convention)
     * @return Default value of field, or null if field doesn't exist.
     */
    public static Object resetField(GuiNode node, String id) {
        FieldDescriptor field = propertyNameFieldMap.map.getOrDefault(id, null);
        if(field == null) {
            logger.error("Field '" + id + "' not found in NodeDesc.");
            return null;
        }
        GuiNodeStateBuilder states = node.getStateBuilder();
        String stateId = getStateId(node);
        NodeDesc.Builder b = states.stateMap.getOrDefault(stateId, null);
        if(b != null) {
            b.clearField(field);
        }
        NodeDesc.Builder defaultBuilder = getDefaultBuilderForField(states, stateId, field);
        return defaultBuilder.getField(field);
    }

    /**
     * Test if states field value is overridden
     * @param node GuiNode
     * @param id accessor id of field (camelcase property name, typed as google protocol buffer naming convention
     * @param value Value to test. Must be of same type as field value
     * @return True if the state field is overridden
     */
    public static Boolean isFieldOverridden(GuiNode node, String id , Object value) {
        GuiNodeStateBuilder states = node.getStateBuilder();
        String stateId = getStateId(node);
        if(getDefaultBuilders(states) == null) {
            if(defaultStateId.equals(stateId)) {
                return false;
            }
        }
        NodeDesc.Builder b = states.stateMap.getOrDefault(stateId, null);
        if(b == null) {
            return false;
        }
        FieldDescriptor field = propertyNameFieldMap.map.getOrDefault(id, null);
        if(field == null) {
            logger.error("Field '" + id + "' not found in NodeDesc.");
            return false;
        }
        if(b.hasField(field)) {
            return true;
        }
        return false;
    }

    /**
     * Returns string of the default state name
     * @return String of the default state name
     */
    public static String getDefaultStateId() {
        return defaultStateId;
    }

    /**
     * Test if a node is overridable in the current builder state
     * @return True if the node can be overridden in the current state
     */
    public static Boolean isOverridable(GuiNode node) {
        GuiNodeStateBuilder states = node.getStateBuilder();
        if(getDefaultBuilders(states) != null) {
            return true;
        }
        return !GuiNodeStateBuilder.getDefaultStateId().equals(GuiNodeStateBuilder.getStateId(node));
    }

    private static String getStateId(GuiNode node)
    {
        assert(node instanceof GuiNode);
        if(stateModel != null && node.getModel() != null)
        {
            if(node.getModel().hashCode() == stateModel.hashCode()) {
                return stateId;
            }
        }
        if(node.getParent() == null) {
            stateId = defaultStateId;
        } else {
            Node parent = node.getParent();
            while(parent != null) {
                if(parent instanceof GuiSceneNode) {
                    GuiSceneNode sceneNode = (GuiSceneNode) parent;
                    stateId = sceneNode.getCurrentLayout() == null ? defaultStateId : sceneNode.getCurrentLayout().getId();
                    break;
                }
                parent = parent.getParent();
            }
        }
        stateModel = node.getModel();
        return stateId;
    }

    private static NodeDesc.Builder getDefaultBuilderForField(GuiNodeStateBuilder states, String stateId, FieldDescriptor field) {
        HashMap<String, NodeDesc.Builder> builders = states.stateMapDefault == null ? states.stateMap : states.stateMapDefault;
        NodeDesc.Builder defaultBuilder;
        defaultBuilder = builders.getOrDefault(stateId, null);
        if((defaultBuilder != null) && (defaultBuilder.hasField(field))) {
            return defaultBuilder;
        }
        return builders.getOrDefault(defaultStateId, null);
   }

    public static void setStateId(String stateId) {
        GuiNodeStateBuilder.stateId = stateId;
    }

    public static HashMap<String, NodeDesc.Builder> getBuilders(GuiNodeStateBuilder stateBuilder) {
        return stateBuilder.stateMap;
    }

    public static HashMap<String, NodeDesc.Builder> getDefaultBuilders(GuiNodeStateBuilder stateBuilder) {
        return stateBuilder.stateMapDefault;
    }

    public static void setDefaultBuilders(GuiNodeStateBuilder stateBuilder, HashMap<String, NodeDesc.Builder> builders) {
        stateBuilder.stateMapDefault = builders;
    }

    public static void storeState(GuiNode node) {
        // store current node values to existing builder fields, updating changes
        GuiNodeStateBuilder states = node.getStateBuilder();
        String stateId = getStateId(node);
        NodeDesc.Builder currentBuilder = GuiSceneLoader.nodeToBuilder(node);
        if((stateId.equals(defaultStateId)) && (getDefaultBuilders(states) == null)) {
            states.stateMap.put(defaultStateId, currentBuilder);
        } else {
            if(getDefaultBuilders(states)==null)
            {
                if(states.stateMap.getOrDefault(defaultStateId, null)==null) {
                    states.stateMap.put(defaultStateId, currentBuilder);
                }
            }
            NodeDesc.Builder b = states.stateMap.getOrDefault(stateId, NodeDesc.newBuilder());
            states.stateMap.put(stateId, b);
            Map<FieldDescriptor, Object> fields = b.getAllFields();
            for(FieldDescriptor field : fields.keySet()) {
                b.setField(field, currentBuilder.getField(field));
            }
        }
    }

    public static void storeStates(List<Node> nodes) {
        for(Node node : nodes) {
            GuiNode guiNode = (GuiNode) node;
            storeState(guiNode);
            storeStates(node.getChildren());
        }
    }

    public static void restoreState(GuiNode node) {
        // restore current node from current state using existing fields in states builder
        GuiNodeStateBuilder states = node.getStateBuilder();
        HashMap<String, NodeDesc.Builder> defaultStateMap = states.stateMapDefault == null ? states.stateMap : states.stateMapDefault;
        String stateId = getStateId(node);
        NodeDesc.Builder defaultBuilder = defaultStateMap.getOrDefault(defaultStateId, GuiSceneLoader.nodeToBuilder(node));
        defaultStateMap.put(defaultStateId, defaultBuilder);

        NodeDesc.Builder newBuilder = defaultBuilder.clone();;
        if(getDefaultBuilders(states) == null) {
            if(!defaultStateId.equals(stateId)) {
                NodeDesc.Builder b = states.stateMap.getOrDefault(stateId, NodeDesc.newBuilder());
                Map<FieldDescriptor, Object> fields = b.getAllFields();
                for(FieldDescriptor field : fields.keySet()) {
                    newBuilder.setField(field, b.getField(field));
                }
            }
        } else {
            Map<FieldDescriptor, Object> fields;
            NodeDesc.Builder b;
            b = defaultStateMap.getOrDefault(stateId, null);
            if(b != null) {
                fields = b.getAllFields();
                for(FieldDescriptor field : fields.keySet()) {
                    newBuilder.setField(field, b.getField(field));
                }
            }
            b = states.stateMap.getOrDefault(stateId, null);
            if(b != null) {
                fields = b.getAllFields();
                for(FieldDescriptor field : fields.keySet()) {
                    newBuilder.setField(field, b.getField(field));
                }
            }
        }

        // Preserve persistent sub-states of the node (TODO: Define by a property decorator or 'isoverridable' func..)
        String id = node.getId();
        if(node instanceof TemplateNode) {
            TemplateNode templateNode = (TemplateNode) node;
            String template = templateNode.getTemplatePath();
            GuiSceneLoader.builderToNode(node, newBuilder);
            templateNode.setTemplatePath(template);
        } else {
            GuiSceneLoader.builderToNode(node, newBuilder);
        }
        node.setId(id);
    }

    public static void restoreStates(List<Node> nodes) {
        for(Node node : nodes) {
            GuiNode guiNode = (GuiNode) node;
            restoreState(guiNode);
            restoreStates(node.getChildren());
        }
    }

    public static void cloneState(GuiNodeStateBuilder states, String destStateId, String srcStateId) {
        states.stateMap.remove(destStateId);
        if(srcStateId.equals(getDefaultStateId())) {
            return;
        }
        NodeDesc.Builder b = states.stateMap.getOrDefault(srcStateId, null);
        if(b != null) {
            states.stateMap.put(destStateId, b.clone());
        }
    }

    public static void cloneStates(List<Node> nodes, String destStateId, String srcStateId) {
        for(Node node : nodes) {
            GuiNode guiNode = (GuiNode) node;
            cloneState(guiNode.getStateBuilder(), destStateId, srcStateId);
            cloneStates(node.getChildren(), destStateId, srcStateId);
        }
    }

    public static void removeState(GuiNodeStateBuilder states, String stateId) {
        states.stateMap.remove(stateId);
    }

    public static void removeStates(List<Node> nodes, String stateId) {
        for(Node node : nodes) {
            GuiNode guiNode = (GuiNode) node;
            removeState(guiNode.getStateBuilder(), stateId);
            removeStates(node.getChildren(), stateId);
        }
    }

    public static boolean isStateSet(GuiNode node) {
        GuiNodeStateBuilder states = node.getStateBuilder();
        String stateId = getStateId(node);
        if(getDefaultBuilders(states) == null) {
            if(defaultStateId.equals(stateId)) {
                return false;
            }
        }
        NodeDesc.Builder b = states.stateMap.getOrDefault(stateId, null);
        if(b != null) {
            Set<FieldDescriptor> fields = b.getAllFields().keySet();
            if(!fields.isEmpty()) {
                if((fields.size() == 1) && ((FieldDescriptor)fields.toArray()[0]).getName().equals("id")) {
                    return false;
                }
                return true;
            }
        }
        return false;
    }

    private void writeObject(ObjectOutputStream out) throws IOException {
        out.defaultWriteObject();

        HashMap<String, byte[]> builders = new HashMap<String, byte[]>(this.stateMap.size());
        for(String key : this.stateMap.keySet()) {
            NodeDesc.Builder b = this.stateMap.get(key);
            builders.put(key, b.clone().build().toByteArray());
        }
        out.writeObject(builders);
    }

    @SuppressWarnings("unchecked")
    private void readObject(ObjectInputStream in) throws IOException, ClassNotFoundException {
        in.defaultReadObject();

        HashMap<String, byte[]> builders = (HashMap<String, byte[]>)in.readObject();
        this.stateMap = new HashMap<String, NodeDesc.Builder>(builders.size());
        for(String key : builders.keySet()) {
            this.stateMap.put(key, NodeDesc.parseFrom(builders.get(key)).toBuilder());
        }
    }

}


