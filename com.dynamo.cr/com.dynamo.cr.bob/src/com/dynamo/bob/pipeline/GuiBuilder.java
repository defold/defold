// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.dynamo.bob.pipeline;

import java.io.IOException;
import java.util.*;
import java.util.function.Function;
import java.util.stream.Collectors;

import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Tuple3d;
import javax.vecmath.Vector3d;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.fs.ResourceUtil;
import com.dynamo.bob.font.FontStyles;
import com.dynamo.bob.util.BobNLS;
import com.dynamo.bob.util.MathUtil;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.bob.util.TextureUtil;
import com.dynamo.gamesys.proto.Gui.Property;
import com.dynamo.gamesys.proto.Gui.Property.PropertyType;
import com.dynamo.proto.DdfMath.Quat;
import com.dynamo.proto.DdfMath.Vector3;
import com.dynamo.proto.DdfMath.Vector4;
import com.dynamo.proto.DdfMath.Vector4One;
import com.dynamo.gamesys.proto.Gui.NodeDesc;
import com.dynamo.gamesys.proto.Gui.NodeDesc.Type;
import com.dynamo.gamesys.proto.Gui.SceneDesc;
import com.dynamo.gamesys.proto.Gui.SceneDesc.FontDesc;
import com.dynamo.gamesys.proto.Gui.SceneDesc.ParticleFXDesc;
import com.dynamo.gamesys.proto.Gui.SceneDesc.LayerDesc;
import com.dynamo.gamesys.proto.Gui.SceneDesc.LayoutDesc;
import com.dynamo.gamesys.proto.Gui.SceneDesc.TextureDesc;
import com.dynamo.gamesys.proto.Gui.SceneDesc.MaterialDesc;
import com.dynamo.gamesys.proto.Gui.SceneDesc.ResourceDesc;
import com.google.protobuf.Descriptors;
import com.google.protobuf.Descriptors.FieldDescriptor;
import com.google.protobuf.MessageOrBuilder;

import org.apache.commons.io.FilenameUtils;

@ProtoParams(srcClass = SceneDesc.class, messageClass = SceneDesc.class)
@BuilderParams(name="Gui", inExts=".gui", outExt=".guic")
public class GuiBuilder extends ProtoBuilder<SceneDesc.Builder> {
    private final Map<String, Set<String>> fontStyleNames = new HashMap<>();
    private final Map<Integer, IGuiCustomNode> customNodes = new HashMap<>();
    private final HashMap<String, SceneDesc.Builder> sourceSceneCache = new HashMap<>();
    private final Set<String> migrationCacheKeys = new TreeSet<>();
    private final Set<String> declaredTemplates = new HashSet<>();
    private final Set<String> visitingTemplates = new LinkedHashSet<>();
    private boolean hasNodeCallbacks;
    private GuiBuildContext customNodeContext;
    private SceneDesc resolvedScene;

    private void validateStyleSelection(String input, NodeDesc node, Map<String, Set<String>> styles) throws CompileExceptionError {
        if (node.getType() == Type.TYPE_TEXT && styles.containsKey(node.getFont()) && !styles.get(node.getFont()).contains(node.getStyle()))
            throw new CompileExceptionError(project.getResource(input), 0,
                    GuiBuildContext.formatNodeLocation(node, "") + "font style '" + node.getStyle() + "' does not exist");
    }


    private static void quatToEuler(Quat4d quat, Tuple3d euler) {
        double heading;
        double attitude;
        double bank;
        double test = quat.x * quat.y + quat.z * quat.w;
        if (test > 0.499)
        { // singularity at north pole
            heading = 2 * Math.atan2(quat.x, quat.w);
            attitude = Math.PI / 2;
            bank = 0;
        }
        else if (test < -0.499)
        { // singularity at south pole
            heading = -2 * Math.atan2(quat.x, quat.w);
            attitude = -Math.PI / 2;
            bank = 0;
        }
        else
        {
            double sqx = quat.x * quat.x;
            double sqy = quat.y * quat.y;
            double sqz = quat.z * quat.z;
            heading = Math.atan2(2 * quat.y * quat.w - 2 * quat.x * quat.z, 1 - 2 * sqy - 2 * sqz);
            attitude = Math.asin(2 * test);
            bank = Math.atan2(2 * quat.x * quat.w - 2 * quat.y * quat.z, 1 - 2 * sqx - 2 * sqz);
        }
        euler.x = bank * 180 / Math.PI;
        euler.y = heading * 180 / Math.PI;
        euler.z = attitude * 180 / Math.PI;
    }

    private static void eulerToQuat(Tuple3d euler, Quat4d quat) {
        // Implementation based on:
        // http://ntrs.nasa.gov/archive/nasa/casi.ntrs.nasa.gov/19770024290.pdf
        // Rotation sequence: 231 (YZX)
        double t1 = euler.y * Math.PI / 180;
        double t2 = euler.z * Math.PI / 180;
        double t3 = euler.x * Math.PI / 180;
        double c1 = Math.cos(t1/2);
        double s1 = Math.sin(t1/2);
        double c2 = Math.cos(t2/2);
        double s2 = Math.sin(t2/2);
        double c3 = Math.cos(t3/2);
        double s3 = Math.sin(t3/2);
        double c1_c2 = c1*c2;
        double s2_s3 = s2*s3;
        quat.w = -s1*s2_s3 + c1_c2*c3;
        quat.x =  s1*s2*c3 + s3*c1_c2;
        quat.y =  s1*c2*c3 + s2_s3*c1;
        quat.z = -s1*s3*c2 + s2*c1*c3;
        quat.normalize();
    }

    private static void transformTemplateChild(NodeDesc.Builder b, NodeDesc parentNode, HashMap<String, NodeDesc> defaultNodeMap, HashMap<String, NodeDesc> nodeMap) {
        // Apply parent layer if node layer isn't set and parent has it's set it should simply set the parents as this is otherwise inherited.
        if((!b.hasLayer() || b.getLayer().isEmpty()) && (parentNode.hasLayer() && !parentNode.getLayer().isEmpty())) {
            b.setLayer(parentNode.getLayer());
        }

        // Apply inherited alpha. Also set state of parent inherit alpha, otherwise we won't inherit from parents parent.
        if(b.getInheritAlpha()) {
            b.setAlpha(b.getAlpha() * parentNode.getAlpha());
            b.setInheritAlpha(parentNode.getInheritAlpha());
        }

        // Apply Enabled. We want to disable the node if the parent Template is disabled.
        if(b.getEnabled()) {
            b.setEnabled(parentNode.getEnabled());
        }

        // Apply parent scale
        Vector3d parentScale = new Vector3d(parentNode.getScale().getX(), parentNode.getScale().getY(), parentNode.getScale().getZ());
        Point3d scale = new Point3d(b.getScale().getX(), b.getScale().getY(), b.getScale().getZ());
        scale.set(scale.getX() * parentScale.getX(), scale.getY() * parentScale.getY(), scale.getZ() * parentScale.getZ());
        b.setScale(Vector4One.newBuilder().setX((float) scale.getX()).setY((float) scale.getY()).setZ((float) scale.getZ()).setW(1.0f).build());

        // Apply parent position
        Vector3d parentRot = new Vector3d(parentNode.getRotation().getX(), parentNode.getRotation().getY(), parentNode.getRotation().getZ());
        Quat4d pRQ = new Quat4d();
        eulerToQuat(parentRot, pRQ);
        Point3d pos = new Point3d(b.getPosition().getX(), b.getPosition().getY(), b.getPosition().getZ());
        pos.set(parentScale.getX() * pos.getX(), parentScale.getY() * pos.getY(), parentScale.getZ() * pos.getZ());
        MathUtil.rotate(pRQ, pos);
        pos.add(new Point3d(parentNode.getPosition().getX(), parentNode.getPosition().getY(), parentNode.getPosition().getZ()));
        b.setPosition(Vector4.newBuilder().setX((float) pos.getX()).setY((float) pos.getY()).setZ((float) pos.getZ()).setW(1.0f).build());

        // Apply parent rotation
        Point3d rot = new Point3d(b.getRotation().getX(), b.getRotation().getY(), b.getRotation().getZ());
        Quat4d rQ = new Quat4d();
        eulerToQuat(rot, rQ);
        rQ.mul(pRQ, rQ);
        quatToEuler(rQ, rot);
        b.setRotation(Vector4.newBuilder().setX((float) rot.getX()).setY((float) rot.getY()).setZ((float) rot.getZ()).setW(0.0f).build());

        // resolve parent and do recursive transform
        b.setParent(parentNode.getParent());
        if(!parentNode.getParent().isEmpty()) {
            parentNode = nodeMap.containsKey(parentNode.getParent()) ? nodeMap.get(parentNode.getParent()) : defaultNodeMap.get(parentNode.getParent());
            if(parentNode.getType() == Type.TYPE_TEMPLATE) {
                transformTemplateChild(b, parentNode, defaultNodeMap, nodeMap);
            }
        }
    }

    private static void flattenTemplates(HashMap<String, ArrayList<NodeDesc>> scene, HashMap<String, HashMap<String, NodeDesc>> nodeMap) {
        for(String layout : scene.keySet()) {
            ArrayList<NodeDesc> nodes = scene.get(layout);
            ArrayList<NodeDesc> newNodes = new ArrayList<NodeDesc>(nodes.size());
            HashMap<String, NodeDesc> defaultLayoutNodeMap = nodeMap.get("");
            HashMap<String, NodeDesc> layoutNodeMap = nodeMap.get(layout);

            for(NodeDesc node : nodes) {
                if(node.getType() == Type.TYPE_TEMPLATE) {
                    continue;
                }
                if(!node.getParent().isEmpty()) {
                    NodeDesc parent = layoutNodeMap.containsKey(node.getParent()) ? layoutNodeMap.get(node.getParent()) : defaultLayoutNodeMap.get(node.getParent());
                    if(parent.getType() == Type.TYPE_TEMPLATE) {
                        NodeDesc.Builder b = node.toBuilder();
                        transformTemplateChild(b, parent, defaultLayoutNodeMap, layoutNodeMap);
                        node = b.build();
                    }
                }
                newNodes.add(node);
            }
            scene.put(layout, newNodes);
        }
    }

    private static void validateCustomNodeProperties(NodeDesc n, GuiBuilder builder, String input, String layoutName, Set<String> resourceNames) throws CompileExceptionError {
        List<String> nodeResources = new ArrayList<>();

        if (n.getType() == Type.TYPE_CUSTOM) {
            GuiCustomTypeRegistry.Type customType = builder.project.getGuiCustomTypeRegistry().getByHash(n.getCustomType());
            if (customType != null) {
                for (Property property : n.getCustomPropertiesList()) {
                    GuiCustomTypeRegistry.Property propertyDefinition = property.hasId() && !property.getId().isEmpty()
                            ? customType.getProperty(property.getId())
                            : customType.getProperty(property.getIdHash());

                    if (propertyDefinition != null && property.getType() != propertyDefinition.getPropertyType()) {
                        throw new CompileExceptionError(builder.project.getResource(input), 0,
                                GuiBuildContext.formatNodeLocation(n, layoutName) + "Property '" + propertyDefinition.getName()
                                + "' must have type " + propertyDefinition.getPropertyType() + " (got " + property.getType() + ")");
                    }
                    if (propertyDefinition != null && propertyDefinition.isResource() && property.hasString() && !property.getString().isEmpty()) {
                        nodeResources.add(property.getString());
                    }
                }
            }
        }

        for (String resource : nodeResources) {
            if (!resourceNames.contains(resource)) {
                throw new CompileExceptionError(builder.project.getResource(input), 0,
                        GuiBuildContext.formatNodeLocation(n, layoutName) + BobNLS.bind(Messages.BuilderUtil_MISSING_RESOURCE, "resource", resource));
            }
        }
    }

    private static void validateNodeResources(NodeDesc n, GuiBuilder builder, String input, Set<String> fontNames, Set<String> particlefxNames, Set<String> textureNames, Set<String> layerNames, Set<String> materialNames) throws CompileExceptionError {
        if (n.hasTexture() && !n.getTexture().isEmpty()) {
            if (!textureNames.contains(n.getTexture().split("/")[0])) {
                throw new CompileExceptionError(builder.project.getResource(input), 0, BobNLS.bind(Messages.GuiBuilder_MISSING_TEXTURE, n.getTexture().split("/")[0]));
            }
        }
        if (n.hasMaterial() && !n.getMaterial().isEmpty()) {
            if (!materialNames.contains(n.getMaterial())) {
                throw new CompileExceptionError(builder.project.getResource(input), 0, BobNLS.bind(Messages.GuiBuilder_MISSING_MATERIAL, n.getMaterial()));
            }
        }
        if (n.hasFont() && !n.getFont().isEmpty()) {
            if (!fontNames.contains(n.getFont())) {
                throw new CompileExceptionError(builder.project.getResource(input), 0, BobNLS.bind(Messages.GuiBuilder_MISSING_FONT, n.getFont()));
            }
        }
        if (n.hasParticlefx() && !n.getParticlefx().isEmpty()) {
            if (!particlefxNames.contains(n.getParticlefx())) {
                throw new CompileExceptionError(builder.project.getResource(input), 0, BobNLS.bind(Messages.GuiBuilder_MISSING_PARTICLEFX, n.getParticlefx()));
            }
        }
        if (n.hasLayer() && !n.getLayer().isEmpty()) {
            if (!layerNames.contains(n.getLayer())) {
                throw new CompileExceptionError(builder.project.getResource(input), 0, BobNLS.bind(Messages.GuiBuilder_MISSING_LAYER, n.getLayer()));
            }
        }
        if (n.getType() == Type.TYPE_TEMPLATE && n.getTemplate().isEmpty()) {
            throw new CompileExceptionError(builder.project.getResource(input), 0, BobNLS.bind(Messages.BuilderUtil_EMPTY_RESOURCE, "template", input));
        }
    }

    private static String replaceTextureName(String str) throws CompileExceptionError {
        String out = str;
        String ext = "." + FilenameUtils.getExtension(str);
        try {
            if (TextureUtil.isAtlasFileType(ext)) {
                out = ProtoBuilders.replaceTextureSetName(str);
            } else {
                out = ProtoBuilders.replaceTextureName(str);
            }
        } catch (Exception e) {
            throw new CompileExceptionError(null, -1, e.getMessage(), e);
        }
        return out;
    }

    private static HashMap<String, HashMap<String, NodeDesc>> createNodeMap(HashMap<String, ArrayList<NodeDesc>> newScene) {
        HashMap<String, HashMap<String, NodeDesc>> map = new HashMap<String, HashMap<String, NodeDesc>>(newScene.size());
        for(String layout : newScene.keySet()) {
            ArrayList<NodeDesc> nodeArray = newScene.get(layout);
            HashMap<String, NodeDesc> nodeMap = new HashMap<String, NodeDesc>(nodeArray.size());
            for(NodeDesc node : nodeArray) {
                nodeMap.put(node.getId(), node);
            }
            map.put(layout, nodeMap);
        }
        return map;
    }

    private static NodeDesc.Builder ApplyOverriddenFieldValues(NodeDesc.Builder targetBuilder, NodeDesc overrideNode) {
        Descriptors.Descriptor typeDesc = NodeDesc.getDescriptor();

        if (overrideNode.getCustomPropertiesCount() != 0) {
            mergeCustomPropertiesByIdHash(targetBuilder, overrideNode);
        }

        for (int fieldNumber : overrideNode.getOverriddenFieldsList()) {
            if (fieldNumber == NodeDesc.CUSTOM_PROPERTIES_FIELD_NUMBER) {
            } else {
                FieldDescriptor fieldDesc = typeDesc.findFieldByNumber(fieldNumber);
                assert fieldDesc != null;
                targetBuilder.setField(fieldDesc, overrideNode.getField(fieldDesc));
            }
        }

        return targetBuilder;
    }

    private static long hashPropertyName(String name) {
        return MurmurHash.hash64(name);
    }

    private static Property.Builder buildCustomProperty(String name, Object value, PropertyType propertyType) {
        Property.Builder propertyBuilder = Property.newBuilder();
        propertyBuilder.setIdHash(hashPropertyName(name));
        propertyBuilder.setType(propertyType);

        switch (propertyType) {
        case TYPE_BOOLEAN:
            if (!(value instanceof Boolean)) {
                throw new IllegalArgumentException("Custom gui property '" + name + "' must have a boolean default value");
            }
            propertyBuilder.setBoolean((Boolean) value);
            break;
        case TYPE_NUMBER:
            if (!(value instanceof Number)) {
                throw new IllegalArgumentException("Custom gui property '" + name + "' must have a numeric default value");
            }
            propertyBuilder.setNumber(((Number) value).floatValue());
            break;
        case TYPE_HASH:
            if (value instanceof Number) {
                propertyBuilder.setHash(((Number) value).longValue());
            } else {
                propertyBuilder.setHash(hashPropertyName(value == null ? "" : value.toString()));
            }
            break;
        case TYPE_STRING:
            propertyBuilder.setString(value == null ? "" : value.toString());
            break;
        case TYPE_VECTOR3:
            if (!(value instanceof Vector3)) {
                throw new IllegalArgumentException("Custom gui property '" + name + "' must have a Vector3 default value");
            }
            propertyBuilder.setVector3((Vector3) value);
            break;
        case TYPE_VECTOR4:
            if (!(value instanceof Vector4)) {
                throw new IllegalArgumentException("Custom gui property '" + name + "' must have a Vector4 default value");
            }
            propertyBuilder.setVector4((Vector4) value);
            break;
        case TYPE_QUAT:
            if (!(value instanceof Quat)) {
                throw new IllegalArgumentException("Custom gui property '" + name + "' must have a Quat default value");
            }
            propertyBuilder.setQuat((Quat) value);
            break;
        default:
            throw new IllegalArgumentException("Unsupported custom gui property type '" + propertyType + "' for property '" + name + "'");
        }

        return propertyBuilder;
    }

    private static Property canonicalizeCustomProperty(Property property) {
        Property.Builder propertyBuilder = property.toBuilder();

        if (property.hasId() && !property.getId().isEmpty()) {
            propertyBuilder.setIdHash(hashPropertyName(property.getId()));
        }
        if (property.getType() == PropertyType.TYPE_HASH && property.hasString()) {
            propertyBuilder.setHash(hashPropertyName(property.getString()));
        }

        propertyBuilder.clearId();
        return propertyBuilder.build();
    }

    private static void mergeCustomPropertiesByIdHash(NodeDesc.Builder targetBuilder, NodeDesc overrideNode) {
        Map<Long, Integer> customPropertyIndices = new HashMap<>();
        List<Property> customProperties = new ArrayList<>(targetBuilder.getCustomPropertiesCount() + overrideNode.getCustomPropertiesCount());

        for (Property property : targetBuilder.getCustomPropertiesList()) {
            Property canonicalProperty = canonicalizeCustomProperty(property);
            if (canonicalProperty.hasIdHash()) {
                customPropertyIndices.put(canonicalProperty.getIdHash(), customProperties.size());
            }
            customProperties.add(canonicalProperty);
        }

        for (Property property : overrideNode.getCustomPropertiesList()) {
            Property canonicalProperty = canonicalizeCustomProperty(property);
            if (canonicalProperty.hasIdHash()) {
                Integer propertyIndex = customPropertyIndices.get(canonicalProperty.getIdHash());
                if (propertyIndex != null) {
                    customProperties.set(propertyIndex, canonicalProperty);
                } else {
                    customPropertyIndices.put(canonicalProperty.getIdHash(), customProperties.size());
                    customProperties.add(canonicalProperty);
                }
            } else {
                customProperties.add(canonicalProperty);
            }
        }

        targetBuilder.clearCustomProperties();
        targetBuilder.addAllCustomProperties(customProperties);
    }

    private static void validateNodeType(NodeDesc node, IResource resource) throws CompileExceptionError {
        switch (node.getType()) {
            case TYPE_BOX, TYPE_TEXT, TYPE_PIE, TYPE_TEMPLATE, TYPE_PARTICLEFX, TYPE_CUSTOM:
                return;
            default:
                throw new CompileExceptionError(resource, 0,
                        GuiBuildContext.formatNodeLocation(node, "") + "unsupported GUI node type '" + node.getType()
                        + "'. Install or update the plugin that migrates this node type.");
        }
    }

    private static void validateMigratedField(FieldDescriptor field, IResource resource, String location) throws CompileExceptionError {
        if (field != null && field.getOptions().getDeprecated()) {
            throw new CompileExceptionError(resource, 0, location + "Legacy GUI field '" + field.getName()
                    + "' was not migrated. Install or update the GUI plugin that handles this field.");
        }
    }

    private static void validateMigratedFields(MessageOrBuilder message, IResource resource, String location) throws CompileExceptionError {
        for (FieldDescriptor field : message.getAllFields().keySet()) {
            validateMigratedField(field, resource, location);
        }
    }

    private static void validateSourceNode(NodeDesc node, IResource resource, String layoutName) throws CompileExceptionError {
        validateNodeType(node, resource);
        String location = GuiBuildContext.formatNodeLocation(node, layoutName);
        validateMigratedFields(node, resource, location);
        // Sparse overrides can select a default without storing the field value.
        for (int fieldNumber : node.getOverriddenFieldsList()) {
            validateMigratedField(NodeDesc.getDescriptor().findFieldByNumber(fieldNumber), resource, location);
        }
    }

    private static NodeDesc resolveCustomNode(GuiBuilder builder, String input, NodeDesc node) throws CompileExceptionError {
        if (node.getType() != Type.TYPE_CUSTOM) {
            return node;
        }
        GuiCustomTypeRegistry customTypeRegistry = builder.project.getGuiCustomTypeRegistry();
        GuiCustomTypeRegistry.Type customType = node.hasCustomTypeName() && !node.getCustomTypeName().isEmpty()
                ? customTypeRegistry.getByName(node.getCustomTypeName())
                : customTypeRegistry.getByHash(node.getCustomType());

        if (customType == null) {
            String customTypeName = node.getCustomTypeName();
            String message = customTypeName.isEmpty()
                    ? "Unknown GUI custom node type hash '" + Integer.toUnsignedLong(node.getCustomType()) + "' for node '" + node.getId() + "'"
                    : "Unknown GUI custom node type '" + customTypeName + "' for node '" + node.getId() + "'";
            IResource resource = builder.project.getResource(input);
            throw new CompileExceptionError(resource, 0, message);
        }

        var migratedProperties = new LinkedHashMap<String, Object>();
        customType.migrateProperties(migratedProperties);

        var overriddenFields = new ArrayList<Integer>(node.getOverriddenFieldsCount());
        for (int fieldNumber : node.getOverriddenFieldsList()) {
            if (fieldNumber != NodeDesc.CUSTOM_PROPERTIES_FIELD_NUMBER) {
                overriddenFields.add(fieldNumber);
            }
        }

        var existingPropertyHashes = new HashSet<Long>();
        var customProperties = new ArrayList<Property>(node.getCustomPropertiesCount() + customType.getProperties().size());
        for (Property property : node.getCustomPropertiesList()) {
            var canonicalProperty = canonicalizeCustomProperty(property);
            if (canonicalProperty.hasIdHash()) {
                existingPropertyHashes.add(canonicalProperty.getIdHash());
            }
            customProperties.add(canonicalProperty);
        }

        for (GuiCustomTypeRegistry.Property property : customType.getProperties()) {
            var propertyNameHash = property.getNameHash();
            if (existingPropertyHashes.contains(propertyNameHash)) {
                continue;
            }

            var value = migratedProperties.containsKey(property.getName()) ? migratedProperties.get(property.getName()) : property.getDefaultValue();
            customProperties.add(buildCustomProperty(property.getName(), value, property.getPropertyType()).build());
            existingPropertyHashes.add(propertyNameHash);
        }

        var nodeBuilder = node.toBuilder();
        nodeBuilder.setType(Type.TYPE_CUSTOM);
        nodeBuilder.setCustomType(customType.getNameHash());
        nodeBuilder.clearCustomTypeName();
        nodeBuilder.clearCustomProperties();
        nodeBuilder.addAllCustomProperties(customProperties);
        nodeBuilder.clearOverriddenFields();
        nodeBuilder.addAllOverriddenFields(overriddenFields);
        return nodeBuilder.build();
    }

    private static NodeDesc clearEditorOnlyCustomNodeFields(NodeDesc node) {
        if (!node.hasCustomTypeName()) {
            return node;
        }

        return node.toBuilder().clearCustomTypeName().build();
    }

    private static void clearEditorOnlyCustomNodeFields(SceneDesc.Builder sceneBuilder) {
        for (int i = 0, len = sceneBuilder.getNodesCount(); i < len; ++i) {
            sceneBuilder.setNodes(i, clearEditorOnlyCustomNodeFields(sceneBuilder.getNodes(i)));
        }

        for (LayoutDesc.Builder layoutBuilder : sceneBuilder.getLayoutsBuilderList()) {
            for (int i = 0, len = layoutBuilder.getNodesCount(); i < len; ++i) {
                layoutBuilder.setNodes(i, clearEditorOnlyCustomNodeFields(layoutBuilder.getNodes(i)));
            }
        }
    }

    private static void ApplyLayoutOverrides(NodeDesc.Builder builder, HashMap<String, NodeDesc> nodeMapDefault, HashMap<String, NodeDesc> nodeMap) {
        NodeDesc parentSceneNode = (nodeMap == null) ? null : nodeMap.get(builder.getId());
        if((parentSceneNode == null) && (nodeMapDefault != null)) {
            parentSceneNode = nodeMapDefault.get(builder.getId());
        }
        if(parentSceneNode != null && (parentSceneNode.getOverriddenFieldsCount() != 0 || parentSceneNode.getCustomPropertiesCount() != 0)) {
            ApplyOverriddenFieldValues(builder, parentSceneNode);
        }
        // opt fields ignored by run-time
        builder.clearOverriddenFields();
    }

    private static ArrayList<NodeDesc> mergeNodes(GuiBuilder builder, String input, NodeDesc parentNode, List<NodeDesc> nodes, HashMap<String, NodeDesc> layoutNodes, HashMap<String, HashMap<String, NodeDesc>> parentSceneNodeMap, String layout, boolean applyDefaultLayout) throws CompileExceptionError {
        ArrayList<NodeDesc> newNodes = new ArrayList<NodeDesc>(nodes.size());
        for(NodeDesc n : nodes) {
            // pick default node if no layout version exist
            NodeDesc node = (layoutNodes == null) ? n : (layoutNodes.containsKey(n.getId()) ? layoutNodes.get(n.getId()) : n);
            NodeDesc.Builder b = node.toBuilder();

            // insert parentNode as prefix to parent and node id
            if(b.getParent().isEmpty()) {
                b.setParent(parentNode.getId());
            } else {
                b.setParent(parentNode.getId() + "/" + b.getParent());
            }
            b.setId(parentNode.getId() + "/" + b.getId());

            // apply overridden fields from super-node to node, if there are any
            // For defaut layout first
            if (applyDefaultLayout) {
                HashMap<String, NodeDesc> nodeMapDefault = parentSceneNodeMap.get("");
                ApplyLayoutOverrides(b, nodeMapDefault, nodeMapDefault);
            }

            // For custom layout
            if (layoutNodes != null) {
                ApplyLayoutOverrides(b, layoutNodes, parentSceneNodeMap.get(layout));
            }
            newNodes.add(resolveCustomNode(builder, input, b.build()));
        }
        return newNodes;
    }

    public static HashMap<String, HashMap<String, NodeDesc>> createNodeMap(String templateId, SceneDesc.Builder sceneBuilder) {
        HashMap<String, HashMap<String, NodeDesc>> map = new HashMap<String, HashMap<String, NodeDesc>>(sceneBuilder.getLayoutsCount()+1);
        HashMap<String, NodeDesc> nodeMap = new HashMap<String, NodeDesc>(sceneBuilder.getNodesCount());
        map.put("", nodeMap);
        for(NodeDesc node : sceneBuilder.getNodesList()) {
            nodeMap.put(templateId + node.getId(), node);
        }
        for(LayoutDesc layout : sceneBuilder.getLayoutsList()) {
            nodeMap = new HashMap<String, NodeDesc>(layout.getNodesCount());
            map.put(layout.getName(), nodeMap);
            for(NodeDesc node : layout.getNodesList()) {
                nodeMap.put(templateId + node.getId(), node);
            }
        }
        return map;
    }

    public interface ISceneBuilderIO {
        public SceneDesc.Builder readScene(String resourcePath) throws IOException, CompileExceptionError;
    }

    private record SceneResolution(SceneDesc.Builder scene, Map<String, IResource> resources) {
    }

    public static SceneDesc.Builder transformScene(GuiBuilder builder, String input, SceneDesc.Builder sceneBuilder, ISceneBuilderIO sceneIO, boolean flattenTemplates) throws IOException, CompileExceptionError {
        return resolveScene(builder, input, sceneBuilder, sceneIO, flattenTemplates).scene();
    }

    private static SceneResolution resolveScene(GuiBuilder builder, String input, SceneDesc.Builder sceneBuilder, ISceneBuilderIO sceneIO, boolean flattenTemplates) throws IOException, CompileExceptionError {
        // register resources
        Set<String> fontNames = new HashSet<String>();
        List<FontDesc> newFontList = new ArrayList<FontDesc>();
        Set<String> particlefxNames = new HashSet<String>();
        List<ParticleFXDesc> newParticleFXList = new ArrayList<ParticleFXDesc>();
        Set<String> textureNames = new HashSet<String>();
        List<TextureDesc> newTextureList = new ArrayList<TextureDesc>();
        Set<String> layerNames = new HashSet<String>();

        Map<String, IResource> sourceResources = new LinkedHashMap<>();
        List<ResourceDesc> newResourcesList = new ArrayList<>();

        Set<String> materialNames          = new HashSet<String>();
        List<MaterialDesc> newMaterialList = new ArrayList<MaterialDesc>();


        if(builder != null) {
            // transform and register scene external resources (if compiling)
            String scriptPath = sceneBuilder.getScript();
            String suffix = BuilderUtil.getSuffix(scriptPath);
            if (!suffix.isEmpty() && !suffix.equals("gui_script"))
            {
                 throw new CompileExceptionError(builder.project.getResource(input), 0, BobNLS.bind(Messages.BuilderUtil_WRONG_RESOURCE_TYPE,
                         new String[] { scriptPath, suffix, "gui_script" } ));
            }
            sceneBuilder.setScript(ResourceUtil.minifyPathAndReplaceExt(scriptPath, ".gui_script", ".gui_scriptc"));
            sceneBuilder.setMaterial(ResourceUtil.minifyPathAndReplaceExt(sceneBuilder.getMaterial(), ".material", ".materialc"));

            for (FontDesc f : sceneBuilder.getFontsList()) {
                if (fontNames.contains(f.getName())) {
                    throw new CompileExceptionError(builder.project.getResource(input), 0, BobNLS.bind(Messages.GuiBuilder_DUPLICATED_FONT,
                            f.getName()));
                }
                fontNames.add(f.getName());
                String compiledPath = ResourceUtil.minifyPathAndReplaceExt(f.getFont(), ".font", ".fontc");
                builder.fontStyleNames.put(compiledPath, FontStyles.readStyleNames(builder.project.getResource(f.getFont())));
                newFontList.add(FontDesc.newBuilder().mergeFrom(f).setFont(ResourceUtil.minifyPathAndReplaceExt(f.getFont(), ".font", ".fontc")).build());
            }

            for (ParticleFXDesc f : sceneBuilder.getParticlefxsList()) {
                if (particlefxNames.contains(f.getName())) {
                    throw new CompileExceptionError(builder.project.getResource(input), 0, BobNLS.bind(Messages.GuiBuilder_DUPLICATED_PARTICLEFX,
                            f.getName()));
                }
                particlefxNames.add(f.getName());
                newParticleFXList.add(ParticleFXDesc.newBuilder().mergeFrom(f).setParticlefx(ResourceUtil.minifyPathAndReplaceExt(f.getParticlefx(), ".particlefx", ".particlefxc")).build());
            }

            for (TextureDesc f : sceneBuilder.getTexturesList()) {
                if (textureNames.contains(f.getName())) {
                    throw new CompileExceptionError(builder.project.getResource(input), 0, BobNLS.bind(Messages.GuiBuilder_DUPLICATED_TEXTURE,
                            f.getName()));
                }
                textureNames.add(f.getName());
                newTextureList.add(TextureDesc.newBuilder().mergeFrom(f).setTexture(replaceTextureName(f.getTexture())).build());
            }

            for (MaterialDesc f : sceneBuilder.getMaterialsList()) {
                if (materialNames.contains(f.getName())) {
                    throw new CompileExceptionError(builder.project.getResource(input), 0, BobNLS.bind(Messages.GuiBuilder_DUPLICATED_MATERIAL,
                            f.getName()));
                }
                materialNames.add(f.getName());
                newMaterialList.add(MaterialDesc.newBuilder().mergeFrom(f).setMaterial(ResourceUtil.minifyPathAndReplaceExt(f.getMaterial(), ".material", ".materialc")).build());
            }

            for (ResourceDesc f : sceneBuilder.getResourcesList()) {
                if (sourceResources.containsKey(f.getName())) {
                    throw new CompileExceptionError(builder.project.getResource(input), 0, BobNLS.bind(Messages.BuilderUtil_DUPLICATE_RESOURCE,
                            f.getName()));
                }
                sourceResources.put(f.getName(), builder.project.getResource(f.getPath()));
                String resourceSuffix = ResourceUtil.getSuffix(f.getPath());
                String buildPath = resourceSuffix == null
                        ? ResourceUtil.minifyPath(f.getPath())
                        : ResourceUtil.minifyPathAndReplaceExt(f.getPath(), resourceSuffix, ResourceUtil.getOutputExt(resourceSuffix));
                newResourcesList.add(ResourceDesc.newBuilder().mergeFrom(f).setPath(buildPath).build());
            }

            // transform scene internal resources
            for (LayerDesc f : sceneBuilder.getLayersList()) {
                if (layerNames.contains(f.getName())) {
                    throw new CompileExceptionError(builder.project.getResource(input), 0, BobNLS.bind(Messages.GuiBuilder_DUPLICATED_LAYER,
                            f.getName()));
                }
                layerNames.add(f.getName());
            }
        } else {
            // register scene external resources
            for (FontDesc f : sceneBuilder.getFontsList()) {
                fontNames.add(f.getName());
                newFontList.add(f);
            }
            for (ParticleFXDesc f : sceneBuilder.getParticlefxsList()) {
                particlefxNames.add(f.getName());
                newParticleFXList.add(f);
            }
            for (TextureDesc f : sceneBuilder.getTexturesList()) {
                textureNames.add(f.getName());
                newTextureList.add(f);
            }
        }

        // setup layouts
        List<String> layouts = new ArrayList<String>(sceneBuilder.getLayoutsCount());
        HashMap<String, ArrayList<NodeDesc>> newScene = new HashMap<String, ArrayList<NodeDesc>>(sceneBuilder.getLayoutsCount());
        newScene.put("", new ArrayList<NodeDesc>(sceneBuilder.getNodesCount()));
        for(LayoutDesc layout : sceneBuilder.getLayoutsList()) {
            newScene.put(layout.getName(), new ArrayList<NodeDesc>(layout.getNodesCount()));
            layouts.add(layout.getName());
        }

        // read and transform scene nodes, including reading template scenes recursively
        HashMap<String, HashMap<String, NodeDesc>> nodeMap = createNodeMap("", sceneBuilder);
        for(NodeDesc node : sceneBuilder.getNodesList()) {
            // ignore child hierarchy of template nodes
            if(node.getTemplateNodeChild()) {
                continue;
            }

            node = resolveCustomNode(builder, input, node);

            // add current scene nodes
            newScene.get("").add(node);
            validateNodeResources(node, builder, input, fontNames, particlefxNames, textureNames, layerNames, materialNames);
            for(String layout : layouts) {
                NodeDesc n = nodeMap.get(layout).get(node.getId());
                if(n != null) {
                    n = resolveCustomNode(builder, input, n);
                    validateNodeResources(n, builder, input, fontNames, particlefxNames, textureNames, layerNames, materialNames);
                    newScene.get(layout).add(n);
                }
            }

            // read in template scene (text version) and transform recursively
            if(node.getType() == Type.TYPE_TEMPLATE) {
                SceneDesc.Builder templateBuilder = sceneIO.readScene(node.getTemplate());
                SceneResolution template = resolveScene(builder, node.getTemplate(), templateBuilder, sceneIO, false);
                templateBuilder = template.scene();

                // merge template scene nodes with overrides of current scene
                List<NodeDesc> nodes = mergeNodes(builder, input, node, templateBuilder.getNodesList(), null, nodeMap, "", true);
                newScene.get("").addAll(nodes);

                List<String> templateLayouts = new ArrayList<String>(templateBuilder.getLayoutsCount());
                for(LayoutDesc layout : templateBuilder.getLayoutsList()) {
                    templateLayouts.add(layout.getName());
                }
                for(LayoutDesc layout : sceneBuilder.getLayoutsList()) {
                    String templateLayoutName = null;
                    HashMap<String, NodeDesc> layoutNodes = null;
                    if(templateLayouts.contains(layout.getName()))
                    {
                        for(LayoutDesc tl : templateBuilder.getLayoutsList()) {
                            if(tl.getName().equals(layout.getName())) {
                                templateLayoutName = tl.getName();
                                layoutNodes = new HashMap<String, NodeDesc>(tl.getNodesCount());
                                for(NodeDesc n : tl.getNodesList()) {
                                    layoutNodes.put(n.getId(), n);
                                }
                                break;
                            }
                        }
                        nodes = mergeNodes(builder, input, node, templateBuilder.getNodesList(), layoutNodes, nodeMap, templateLayoutName, false);
                    } else {
                        templateLayoutName = "";
                        layoutNodes = new HashMap<String, NodeDesc>(templateBuilder.getNodesCount());
                        for(NodeDesc n : templateBuilder.getNodesList()) {
                            layoutNodes.put(n.getId(), n);
                        }
                        nodes = mergeNodes(builder, input, node, templateBuilder.getNodesList(), layoutNodes, nodeMap, layout.getName(), true);
                    }

                    ArrayList<NodeDesc> layoutNodeList = newScene.get(layout.getName());
                    if(layoutNodeList != null) {
                        layoutNodeList.addAll(nodes);
                    }
                }

                // add template scene resources if not already existing in current scene
                for (FontDesc f : templateBuilder.getFontsList()) {
                    if (fontNames.contains(f.getName())) {
                        continue;
                    }
                    fontNames.add(f.getName());
                    newFontList.add(f);
                }
                for (ParticleFXDesc f : templateBuilder.getParticlefxsList()) {
                    if (particlefxNames.contains(f.getName())) {
                        continue;
                    }
                    particlefxNames.add(f.getName());
                    newParticleFXList.add(f);
                }
                for (TextureDesc f : templateBuilder.getTexturesList()) {
                    if (textureNames.contains(f.getName())) {
                        continue;
                    }
                    textureNames.add(f.getName());
                    newTextureList.add(f);
                }
                for (MaterialDesc f : templateBuilder.getMaterialsList()) {
                    if (materialNames.contains(f.getName())) {
                        continue;
                    }
                    materialNames.add(f.getName());
                    newMaterialList.add(f);
                }
                for (ResourceDesc f : templateBuilder.getResourcesList()) {
                    if (sourceResources.containsKey(f.getName())) {
                        continue;
                    }
                    // Select source and compiled paths together, with local resources
                    // followed by template resources in node order.
                    sourceResources.put(f.getName(), template.resources().get(f.getName()));
                    newResourcesList.add(f);
                }

            } else if(node.getType() == Type.TYPE_PARTICLEFX) {
                if (builder != null) {
                    String particlefxId = node.getParticlefx();
                    String particleFxPath = null;
                    for (ParticleFXDesc p : sceneBuilder.getParticlefxsList()) {
                        if (p.getName().equals(particlefxId)) {
                            particleFxPath = p.getParticlefx();
                            break;
                        }
                    }
                    if (particleFxPath == null) {
                        throw new CompileExceptionError(builder.project.getResource(input), 0, "Could not build particlefx node from invalid particlefx scene resource: " + particlefxId);
                    }
                }
            }
        }

        if(flattenTemplates) {
            nodeMap = createNodeMap(newScene);
            flattenTemplates(newScene, nodeMap);
        }

        // clear current scene nodes and resources, replace and trimmed with merged versions
        sceneBuilder.clearNodes();
        sceneBuilder.addAllNodes(newScene.get(""));

        ArrayList<LayoutDesc> layoutDescList = new ArrayList<LayoutDesc>(sceneBuilder.getLayoutsCount());
        for(LayoutDesc layout : sceneBuilder.getLayoutsList()) {
            LayoutDesc.Builder b = layout.toBuilder();
            b.clearNodes();
            b.addAllNodes(newScene.get(layout.getName()));
            layoutDescList.add(b.build());
        }
        sceneBuilder.clearLayouts();
        sceneBuilder.addAllLayouts(layoutDescList);

        sceneBuilder.clearFonts();
        sceneBuilder.addAllFonts(newFontList);

        sceneBuilder.clearParticlefxs();
        sceneBuilder.addAllParticlefxs(newParticleFXList);

        sceneBuilder.clearTextures();
        sceneBuilder.addAllTextures(newTextureList);

        sceneBuilder.clearMaterials();
        sceneBuilder.addAllMaterials(newMaterialList);

        sceneBuilder.clearResources();
        sceneBuilder.addAllResources(newResourcesList);

        clearEditorOnlyCustomNodeFields(sceneBuilder);

        if (builder != null) {
            if (flattenTemplates) {
                // Validate custom property types and resources only after parent overrides and
                // inherited resource lists have been resolved.
                for (NodeDesc node : sceneBuilder.getNodesList()) {
                    validateCustomNodeProperties(node, builder, input, "", sourceResources.keySet());
                }
                for (LayoutDesc layout : sceneBuilder.getLayoutsList()) {
                    for (NodeDesc node : layout.getNodesList()) {
                        validateCustomNodeProperties(node, builder, input, layout.getName(), sourceResources.keySet());
                    }
                }
            }
            Map<String, Set<String>> styles = new HashMap<>();
            for (FontDesc font : sceneBuilder.getFontsList()) {
                Set<String> names = builder.fontStyleNames.get(font.getFont());
                if (names != null) styles.put(font.getName(), names);
            }
            for (NodeDesc node : sceneBuilder.getNodesList()) builder.validateStyleSelection(input, node, styles);
            for (LayoutDesc layout : sceneBuilder.getLayoutsList())
                for (NodeDesc node : layout.getNodesList()) builder.validateStyleSelection(input, node, styles);
        }
        return new SceneResolution(sceneBuilder, Collections.unmodifiableMap(sourceResources));
    }

    private static void MergeOriginalValuesIntoLayouts(SceneDesc.Builder sceneBuilder) {
        // Layout nodes will only have valid values for overridden properties.
        // Copy the non-overridden property values from the original scene nodes
        // into the layout nodes.
        if(sceneBuilder.getLayoutsCount() > 0) {
            Map<String, NodeDesc> sceneNodesById = sceneBuilder.getNodesList().stream().collect(Collectors.toMap(NodeDesc::getId, Function.identity()));

            for(LayoutDesc.Builder layoutBuilder : sceneBuilder.getLayoutsBuilderList()) {
                for(int i = 0, len = layoutBuilder.getNodesCount(); i < len; ++i) {
                    NodeDesc layoutNode = layoutBuilder.getNodes(i);

                    // Copy the original property values into our layout node,
                    // but only if it is overriding a node owned by this scene.
                    if (!layoutNode.getTemplateNodeChild()) {
                        NodeDesc sceneNode = sceneNodesById.get(layoutNode.getId());
                        NodeDesc fullyFormedLayoutNode = ApplyOverriddenFieldValues(sceneNode.toBuilder(), layoutNode).build();
                        layoutBuilder.setNodes(i, fullyFormedLayoutNode);
                    }
                }
            }
        }
    }

    private IGuiCustomNode getCustomNode(GuiCustomTypeRegistry.Type type, IResource input) throws CompileExceptionError {
        IGuiCustomNode customNode = customNodes.get(type.getNameHash());
        if (customNode == null) {
            try {
                customNode = type.newNode();
            } catch (ReflectiveOperationException e) {
                throw new CompileExceptionError(input, 0, "Unable to create GUI custom node plugin for type '" + type.getName() + "'", e);
            }
            customNodes.put(type.getNameHash(), customNode);
        }
        return customNode;
    }

    @Override
    protected SceneDesc.Builder getSrcBuilder(IResource input) throws IOException, CompileExceptionError {
        SceneDesc.Builder scene = sourceSceneCache.get(input.getPath());
        if (scene == null) {
            scene = super.getSrcBuilder(input).clone();
            GuiSceneMigrationContext context = new GuiSceneMigrationContext(input, scene, migrationCacheKeys::add);
            for (GuiCustomTypeRegistry.Type type : project.getGuiCustomTypeRegistry().getSceneMigrations()) {
                migrationCacheKeys.add("gui-source-migration-v1:" + type.getName());
                getCustomNode(type, input).migrateScene(context);
            }
            // Validate migrated resources even when unused or restored from cache.
            for (ResourceDesc resource : scene.getResourcesList()) {
                if (resource.getName().isEmpty()) {
                    throw new CompileExceptionError(input, 0,
                            "GUI resource name must not be empty (path: '" + resource.getPath() + "')");
                }
            }
            // Check before task signatures so old cached outputs cannot hide an
            // absent or outdated source-format migration plugin.
            validateMigratedFields(scene, input, "");
            for (NodeDesc node : scene.getNodesList()) {
                validateSourceNode(node, input, "");
                hasNodeCallbacks |= hasCustomNodeCallbacks(node);
            }
            for (LayoutDesc layout : scene.getLayoutsList()) {
                for (NodeDesc node : layout.getNodesList()) {
                    validateSourceNode(node, input, layout.getName());
                    hasNodeCallbacks |= hasCustomNodeCallbacks(node);
                }
            }
            sourceSceneCache.put(input.getPath(), scene);
        }
        // Callers merge layouts and replace resource paths. Keep the cached
        // migrated source untouched so repeated template instances see the same input.
        return scene.clone();
    }

    private class SceneBuilderIO implements ISceneBuilderIO {
        public SceneDesc.Builder readScene(String resourcePath) throws IOException, CompileExceptionError {
            SceneDesc.Builder scene = getSrcBuilder(project.getResource(resourcePath));
            MergeOriginalValuesIntoLayouts(scene);
            return scene;
        }
    }

    private boolean hasCustomNodeCallbacks(NodeDesc node) {
        if (node.getType() != Type.TYPE_CUSTOM) {
            return false;
        }
        GuiCustomTypeRegistry.Type type = node.hasCustomTypeName() && !node.getCustomTypeName().isEmpty()
                ? project.getGuiCustomTypeRegistry().getByName(node.getCustomTypeName())
                : project.getGuiCustomTypeRegistry().getByHash(node.getCustomType());
        return type != null && type.hasNodeCallbacks();
    }

    @Override
    protected void createSubTasks(MessageOrBuilder message, Task.TaskBuilder taskBuilder) throws CompileExceptionError {
        if (!(message instanceof NodeDesc node) || node.getTemplate().isEmpty()) {
            super.createSubTasks(message, taskBuilder);
            return;
        }

        // Templates are flattened from source. Building them as independent GUIs
        // would validate their nodes before the parent can override their values.
        super.createSubTasks(node.toBuilder().clearTemplate(), taskBuilder);
        IResource template = BuilderUtil.checkResource(project, taskBuilder.firstInput(), "template", node.getTemplate());
        taskBuilder.addInput(template);
        String path = template.getPath();
        if (declaredTemplates.contains(path)) {
            return;
        }
        if (!visitingTemplates.add(path)) {
            throw new CompileExceptionError(taskBuilder.firstInput(), 0,
                    "Circular GUI template dependency: " + String.join(" -> ", visitingTemplates) + " -> " + path);
        }
        try {
            SceneDesc.Builder source = new SceneBuilderIO().readScene(node.getTemplate());
            createSubTasks(source, taskBuilder);
            declaredTemplates.add(path);
        } catch (IOException e) {
            throw new CompileExceptionError(template, 0, e.getMessage(), e);
        } finally {
            visitingTemplates.remove(path);
        }
    }

    private void createCustomNodes(IResource input, String layoutName, List<NodeDesc> nodes,
            Task.TaskBuilder<?> taskBuilder) throws IOException, CompileExceptionError {
        for (NodeDesc node : nodes) {
            if (node.getType() != Type.TYPE_CUSTOM) {
                continue;
            }
            GuiCustomTypeRegistry.Type type = project.getGuiCustomTypeRegistry().getByHash(node.getCustomType());
            if (!type.hasNodeCallbacks()) {
                continue;
            }
            IGuiCustomNode customNode = getCustomNode(type, input);
            customNode.create(customNodeContext, taskBuilder, node, layoutName);
        }
    }

    @Override
    public Task create(IResource input) throws IOException, CompileExceptionError {
        Task.TaskBuilder<?> taskBuilder = Task.newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()))
                // Revalidate outputs cached before registered property types were checked.
                .addExtraCacheKey("gui-custom-property-types-v1");
        SceneDesc.Builder sourceScene = getSrcBuilder(input);
        createSubTasks(sourceScene, taskBuilder);
        for (String key : migrationCacheKeys) {
            taskBuilder.addExtraCacheKey(key);
        }
        if (!hasNodeCallbacks) {
            // GUIs without callbacks can still defer transformation until build,
            // avoiding that work entirely when their outputs are cached.
            return taskBuilder.build();
        }
        // Resolve once, before dependency declarations, and retain these exact
        // values for build. Never transform already compiled resource paths again.
        MergeOriginalValuesIntoLayouts(sourceScene);
        SceneResolution resolution = resolveScene(this, input.getPath(), sourceScene, new SceneBuilderIO(), true);
        resolvedScene = resolution.scene().build();
        customNodeContext = new GuiBuildContext(project, input, resolution.resources());

        createCustomNodes(input, "", resolvedScene.getNodesList(), taskBuilder);
        for (LayoutDesc layout : resolvedScene.getLayoutsList()) {
            createCustomNodes(input, layout.getName(), layout.getNodesList(), taskBuilder);
        }
        if (!customNodes.isEmpty()) {
            // Do not reuse outputs produced before lifecycle callbacks existed.
            taskBuilder.addExtraCacheKey("gui-custom-node-lifecycle-v1");
        }
        return taskBuilder.build();
    }

    private void buildCustomNode(Task task, String layoutName, NodeDesc.Builder nodeBuilder) throws IOException, CompileExceptionError {
        if (nodeBuilder.getType() != Type.TYPE_CUSTOM) {
            return;
        }
        IGuiCustomNode customNode = customNodes.get(nodeBuilder.getCustomType());
        if (customNode == null) {
            return;
        }
        customNode.build(customNodeContext, task, nodeBuilder, layoutName);
        for (int i = 0; i < nodeBuilder.getCustomPropertiesCount(); ++i) {
            nodeBuilder.setCustomProperties(i, canonicalizeCustomProperty(nodeBuilder.getCustomProperties(i)));
        }
        nodeBuilder.clearCustomTypeName();
    }

    @Override
    protected SceneDesc.Builder transform(Task task, IResource input, SceneDesc.Builder messageBuilder) throws IOException, CompileExceptionError {
        if (resolvedScene != null) {
            SceneDesc.Builder scene = resolvedScene.toBuilder();
            for (NodeDesc.Builder node : scene.getNodesBuilderList()) {
                buildCustomNode(task, "", node);
            }
            for (LayoutDesc.Builder layout : scene.getLayoutsBuilderList()) {
                for (NodeDesc.Builder node : layout.getNodesBuilderList()) {
                    buildCustomNode(task, layout.getName(), node);
                }
            }
            return scene;
        }

        MergeOriginalValuesIntoLayouts(messageBuilder);
        return transformScene(this, input.getPath(), messageBuilder, new SceneBuilderIO(), true);
    }

    @Override
    public void clearState() {
        super.clearState();
        fontStyleNames.clear();
        customNodes.clear();
        sourceSceneCache.clear();
        migrationCacheKeys.clear();
        declaredTemplates.clear();
        visitingTemplates.clear();
        hasNodeCallbacks = false;
        customNodeContext = null;
        resolvedScene = null;
    }

}
