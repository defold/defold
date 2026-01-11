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

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
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
import com.dynamo.bob.util.BobNLS;
import com.dynamo.bob.util.MathUtil;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.bob.util.TextureUtil;
import com.dynamo.proto.DdfMath.Vector4;
import com.dynamo.proto.DdfMath.Vector4One;
import com.dynamo.gamesys.proto.Gui.NodeDesc;
import com.dynamo.gamesys.proto.Gui.NodeDesc.Type;
import com.dynamo.gamesys.proto.Gui.SceneDesc;
import com.dynamo.gamesys.proto.Gui.SceneDesc.FontDesc;
import com.dynamo.gamesys.proto.Gui.SceneDesc.ParticleFXDesc;
import com.dynamo.gamesys.proto.Gui.SceneDesc.SpineSceneDesc;
import com.dynamo.gamesys.proto.Gui.SceneDesc.LayerDesc;
import com.dynamo.gamesys.proto.Gui.SceneDesc.LayoutDesc;
import com.dynamo.gamesys.proto.Gui.SceneDesc.TextureDesc;
import com.dynamo.gamesys.proto.Gui.SceneDesc.MaterialDesc;
import com.dynamo.gamesys.proto.Gui.SceneDesc.ResourceDesc;
import com.google.protobuf.Descriptors;
import com.google.protobuf.Descriptors.FieldDescriptor;
import com.google.protobuf.TextFormat;

import org.apache.commons.io.FilenameUtils;

@ProtoParams(srcClass = SceneDesc.class, messageClass = SceneDesc.class)
@BuilderParams(name="Gui", inExts=".gui", outExt=".guic")
public class GuiBuilder extends ProtoBuilder<SceneDesc.Builder> {

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

    private static void validateNodeResources(NodeDesc n, GuiBuilder builder, String input, Set<String> resourceNames, Set<String> fontNames, Set<String> particlefxNames, Set<String> textureNames, Set<String> layerNames, Set<String> materialNames) throws CompileExceptionError {
        if(builder == null) {
            return;
        }

        List<String> nodeResources = new ArrayList<>();

        // TODO: Do resource validation in the plugin. I.e. how to get the resources?
        // Perhaps "getResourceProperties()" or "getResources()"
        if (n.hasSpineScene() && !n.getSpineScene().isEmpty()){
            nodeResources.add(n.getSpineScene());
        }

        for (String resource : nodeResources) {
            if (!resourceNames.contains(resource)) {
                throw new CompileExceptionError(builder.project.getResource(input), 0, BobNLS.bind(Messages.BuilderUtil_MISSING_RESOURCE, resource));
            }
        }

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
            throw new CompileExceptionError(builder.project.getResource(input), 0, BobNLS.bind(Messages.BuilderUtil_EMPTY_RESOURCE, "template"));
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

        for (int fieldNumber : overrideNode.getOverriddenFieldsList()) {
            FieldDescriptor fieldDesc = typeDesc.findFieldByNumber(fieldNumber);
            assert fieldDesc != null;
            targetBuilder.setField(fieldDesc, overrideNode.getField(fieldDesc));
        }

        return targetBuilder;
    }

    private static void ApplyLayoutOverrides(NodeDesc.Builder builder, HashMap<String, NodeDesc> nodeMapDefault, HashMap<String, NodeDesc> nodeMap) {
        NodeDesc parentSceneNode = (nodeMap == null) ? null : nodeMap.get(builder.getId());
        if((parentSceneNode == null) && (nodeMapDefault != null)) {
            parentSceneNode = nodeMapDefault.get(builder.getId());
        }
        if(parentSceneNode != null && parentSceneNode.getOverriddenFieldsCount() != 0) {
            ApplyOverriddenFieldValues(builder, parentSceneNode);
        }
        // opt fields ignored by run-time
        builder.clearOverriddenFields();
    }

    private static ArrayList<NodeDesc> mergeNodes(NodeDesc parentNode, List<NodeDesc> nodes, HashMap<String, NodeDesc> layoutNodes, HashMap<String, HashMap<String, NodeDesc>> parentSceneNodeMap, String layout, boolean applyDefaultLayout) {
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
            newNodes.add(b.build());
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
        public SceneDesc.Builder readScene(String resourcePath, HashMap<String, SceneDesc.Builder> sceneResourceCache) throws IOException, CompileExceptionError;
    }

    public static SceneDesc.Builder transformScene(GuiBuilder builder, String input, SceneDesc.Builder sceneBuilder, ISceneBuilderIO sceneIO, HashMap<String, SceneDesc.Builder> sceneResourceCache, boolean flattenTemplates) throws IOException, CompileExceptionError {
        // register resources
        Set<String> fontNames = new HashSet<String>();
        List<FontDesc> newFontList = new ArrayList<FontDesc>();
        Set<String> spineSceneNames = new HashSet<String>();
        List<SpineSceneDesc> newSpineSceneList = new ArrayList<SpineSceneDesc>();
        Set<String> particlefxNames = new HashSet<String>();
        List<ParticleFXDesc> newParticleFXList = new ArrayList<ParticleFXDesc>();
        Set<String> textureNames = new HashSet<String>();
        List<TextureDesc> newTextureList = new ArrayList<TextureDesc>();
        Set<String> layerNames = new HashSet<String>();

        Set<String> resourceNames = new HashSet<String>();
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
            sceneBuilder.setScript(BuilderUtil.replaceExt(scriptPath, ".gui_script", ".gui_scriptc"));
            sceneBuilder.setMaterial(BuilderUtil.replaceExt(sceneBuilder.getMaterial(), ".material", ".materialc"));

            for (FontDesc f : sceneBuilder.getFontsList()) {
                if (fontNames.contains(f.getName())) {
                    throw new CompileExceptionError(builder.project.getResource(input), 0, BobNLS.bind(Messages.GuiBuilder_DUPLICATED_FONT,
                            f.getName()));
                }
                fontNames.add(f.getName());
                newFontList.add(FontDesc.newBuilder().mergeFrom(f).setFont(BuilderUtil.replaceExt(f.getFont(), ".font", ".fontc")).build());
            }

            for (SpineSceneDesc f : sceneBuilder.getSpineScenesList()) {
                if (resourceNames.contains(f.getName())) {
                    throw new CompileExceptionError(builder.project.getResource(input), 0, BobNLS.bind(Messages.BuilderUtil_DUPLICATE_RESOURCE,
                            f.getName()));
                }
                resourceNames.add(f.getName());
                ResourceDesc desc = ResourceDesc.newBuilder().setName(f.getName()).setPath(BuilderUtil.replaceExt(f.getSpineScene(), ".spinescene", ".spinescenec")).build();
                newResourcesList.add(desc);
            }

            for (ParticleFXDesc f : sceneBuilder.getParticlefxsList()) {
                if (particlefxNames.contains(f.getName())) {
                    throw new CompileExceptionError(builder.project.getResource(input), 0, BobNLS.bind(Messages.GuiBuilder_DUPLICATED_PARTICLEFX,
                            f.getName()));
                }
                particlefxNames.add(f.getName());
                newParticleFXList.add(ParticleFXDesc.newBuilder().mergeFrom(f).setParticlefx(BuilderUtil.replaceExt(f.getParticlefx(), ".particlefx", ".particlefxc")).build());
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
                newMaterialList.add(MaterialDesc.newBuilder().mergeFrom(f).setMaterial(BuilderUtil.replaceExt(f.getMaterial(), ".material", ".materialc")).build());
            }

            for (ResourceDesc f : sceneBuilder.getResourcesList()) {
                if (resourceNames.contains(f.getName())) {
                    throw new CompileExceptionError(builder.project.getResource(input), 0, BobNLS.bind(Messages.BuilderUtil_DUPLICATE_RESOURCE,
                            f.getName()));
                }
                // TODO: use the plugin for this
                resourceNames.add(f.getName());
                newResourcesList.add(ResourceDesc.newBuilder().mergeFrom(f).setPath(BuilderUtil.replaceExt(f.getPath(), ".spinescene", ".spinescenec")).build());
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

            // backwards compatibility
            if(node.getType() == Type.TYPE_SPINE) {
                NodeDesc.Builder newNode = node.toBuilder();
                newNode.setType(Type.TYPE_CUSTOM);
                newNode.setCustomType(MurmurHash.hash32("Spine"));
                node = newNode.build();
            }

            // add current scene nodes
            newScene.get("").add(node);
            validateNodeResources(node, builder, input, resourceNames, fontNames, particlefxNames, textureNames, layerNames, materialNames);
            for(String layout : layouts) {
                NodeDesc n = nodeMap.get(layout).get(node.getId());
                if(n != null) {
                    validateNodeResources(n, builder, input, resourceNames, fontNames, particlefxNames, textureNames, layerNames, materialNames);
                    newScene.get(layout).add(n);
                }
            }

            // read in template scene (text version) and transform recursively
            if(node.getType() == Type.TYPE_TEMPLATE) {
                SceneDesc.Builder templateBuilder = sceneIO.readScene(node.getTemplate(), sceneResourceCache);
                templateBuilder = transformScene(builder, node.getTemplate(), templateBuilder, sceneIO, sceneResourceCache, false);

                // merge template scene nodes with overrides of current scene
                List<NodeDesc> nodes = mergeNodes(node, templateBuilder.getNodesList(), null, nodeMap, "", true);
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
                        nodes = mergeNodes(node, templateBuilder.getNodesList(), layoutNodes, nodeMap, templateLayoutName, false);
                    } else {
                        templateLayoutName = "";
                        layoutNodes = new HashMap<String, NodeDesc>(templateBuilder.getNodesCount());
                        for(NodeDesc n : templateBuilder.getNodesList()) {
                            layoutNodes.put(n.getId(), n);
                        }
                        nodes = mergeNodes(node, templateBuilder.getNodesList(), layoutNodes, nodeMap, layout.getName(), true);
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
                    if (resourceNames.contains(f.getName())) {
                        continue;
                    }
                    resourceNames.add(f.getName());
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

        sceneBuilder.clearSpineScenes();

        sceneBuilder.clearParticlefxs();
        sceneBuilder.addAllParticlefxs(newParticleFXList);

        sceneBuilder.clearTextures();
        sceneBuilder.addAllTextures(newTextureList);

        sceneBuilder.clearMaterials();
        sceneBuilder.addAllMaterials(newMaterialList);

        sceneBuilder.clearResources();
        sceneBuilder.addAllResources(newResourcesList);

        return sceneBuilder;
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

    private class SceneBuilderIO implements ISceneBuilderIO {
        com.dynamo.bob.Project project;
        SceneBuilderIO(com.dynamo.bob.Project project) {
            this.project = project;
        }

        public SceneDesc.Builder readScene(String resourcePath, HashMap<String, SceneDesc.Builder> resourceCache) throws IOException {
            SceneDesc.Builder sceneBuilder = resourceCache.get(resourcePath);
            if(sceneBuilder == null) {
                IResource templateSceneResource = this.project.getResource(resourcePath);
                InputStreamReader reader = new InputStreamReader(new ByteArrayInputStream(templateSceneResource.getContent()), "ASCII");
                sceneBuilder = SceneDesc.newBuilder();
                TextFormat.merge(reader, sceneBuilder);
                MergeOriginalValuesIntoLayouts(sceneBuilder);
                resourceCache.put(resourcePath, sceneBuilder);
            }
            return sceneBuilder.clone();
        }
    }

    @Override()
    protected SceneDesc.Builder transform(Task task, IResource input, SceneDesc.Builder messageBuilder) throws IOException, CompileExceptionError {
        HashMap<String, SceneDesc.Builder> sceneResourceCache = new HashMap<String, SceneDesc.Builder>(32);
        MergeOriginalValuesIntoLayouts(messageBuilder);
        return transformScene(this, input.getPath(), messageBuilder, new SceneBuilderIO(this.project), sceneResourceCache, true);
    }

}
