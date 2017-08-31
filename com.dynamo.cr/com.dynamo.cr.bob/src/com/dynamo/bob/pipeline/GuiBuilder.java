package com.dynamo.bob.pipeline;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Tuple3d;
import javax.vecmath.Vector3d;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.Task;
import com.dynamo.bob.Task.TaskBuilder;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.textureset.TextureSetGenerator.UVTransform;
import com.dynamo.bob.util.BobNLS;
import com.dynamo.bob.util.MathUtil;
import com.dynamo.bob.util.RigUtil;
import com.dynamo.bob.util.RigUtil.LoadException;
import com.dynamo.bob.util.RigUtil.UVTransformProvider;
import com.dynamo.bob.util.SpineSceneUtil;
import com.dynamo.proto.DdfMath.Vector4;
import com.dynamo.gui.proto.Gui.NodeDesc;
import com.dynamo.gui.proto.Gui.NodeDesc.AdjustMode;
import com.dynamo.gui.proto.Gui.NodeDesc.Type;
import com.dynamo.gui.proto.Gui.SceneDesc;
import com.dynamo.gui.proto.Gui.SceneDesc.FontDesc;
import com.dynamo.gui.proto.Gui.SceneDesc.ParticleFXDesc;
import com.dynamo.gui.proto.Gui.SceneDesc.SpineSceneDesc;
import com.dynamo.gui.proto.Gui.SceneDesc.LayerDesc;
import com.dynamo.gui.proto.Gui.SceneDesc.LayoutDesc;
import com.dynamo.gui.proto.Gui.SceneDesc.TextureDesc;
import com.google.protobuf.Descriptors.FieldDescriptor;
import com.google.protobuf.TextFormat;


@ProtoParams(messageClass = SceneDesc.class)
@BuilderParams(name="Gui", inExts=".gui", outExt=".guic")
public class GuiBuilder extends ProtoBuilder<SceneDesc.Builder> {

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        SceneDesc.Builder builder = SceneDesc.newBuilder();
        ProtoUtil.merge(input, builder);

        TaskBuilder<Void> taskBuilder = Task.<Void>newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()));

        List<String> templateList = new ArrayList<>();
        for (NodeDesc n : builder.getNodesList()) {
            if(n.getType() == Type.TYPE_TEMPLATE) {
                if(!n.getTemplate().isEmpty() && !templateList.contains(n.getTemplate())) {
                    templateList.add(n.getTemplate());
                    taskBuilder.addInput(this.project.getResource(n.getTemplate()));
                }
            }
        }

        List<String> spineSceneList = new ArrayList<>();
        for (SpineSceneDesc f : builder.getSpineScenesList()) {
            if(!f.getSpineScene().isEmpty() && !spineSceneList.contains(f.getSpineScene())) {
                spineSceneList.add(f.getSpineScene());
                taskBuilder.addInput(this.project.getResource(f.getSpineScene()));
            }
        }
        
        List<String> particlefxSceneList = new ArrayList<>();
        for (ParticleFXDesc p : builder.getParticlefxsList()) {
            if (!p.getParticlefx().isEmpty() && particlefxSceneList.contains(p.getParticlefx())) {
                particlefxSceneList.add(p.getParticlefx());
                taskBuilder.addInput(this.project.getResource(p.getParticlefx()));
            }
        }

        return taskBuilder.build();
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

        // Apply parent scale
        Vector3d parentScale = new Vector3d(parentNode.getScale().getX(), parentNode.getScale().getY(), parentNode.getScale().getZ());
        Point3d scale = new Point3d(b.getScale().getX(), b.getScale().getY(), b.getScale().getZ());
        scale.set(scale.getX() * parentScale.getX(), scale.getY() * parentScale.getY(), scale.getZ() * parentScale.getZ());
        b.setScale(Vector4.newBuilder().setX((float) scale.getX()).setY((float) scale.getY()).setZ((float) scale.getZ()).setW(1.0f).build());

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
        b.setRotation(Vector4.newBuilder().setX((float) rot.getX()).setY((float) rot.getY()).setZ((float) rot.getZ()).setW(1.0f).build());

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

    private static void validateNodeResources(NodeDesc n, GuiBuilder builder, String input, Set<String> fontNames, Set<String> spineSceneNames, Set<String> particlefxNames, Set<String> textureNames, Set<String> layerNames) throws CompileExceptionError {
        if(builder == null) {
            return;
        }
        if (n.hasTexture() && !n.getTexture().isEmpty()) {
            if (!textureNames.contains(n.getTexture().split("/")[0])) {
                throw new CompileExceptionError(builder.project.getResource(input), 0, BobNLS.bind(Messages.GuiBuilder_MISSING_TEXTURE, n.getTexture().split("/")[0]));
            }
        }
        if (n.hasFont() && !n.getFont().isEmpty()) {
            if (!fontNames.contains(n.getFont())) {
                throw new CompileExceptionError(builder.project.getResource(input), 0, BobNLS.bind(Messages.GuiBuilder_MISSING_FONT, n.getFont()));
            }
        }
        if (n.hasSpineScene() && !n.getSpineScene().isEmpty()) {
            if (!spineSceneNames.contains(n.getSpineScene())) {
                throw new CompileExceptionError(builder.project.getResource(input), 0, BobNLS.bind(Messages.GuiBuilder_MISSING_SPINESCENE, n.getSpineScene()));
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

    private static String replaceTextureName(String str) {
        String out = str;
        if(str.endsWith(".atlas")) {
            out = BuilderUtil.replaceExt(out, ".atlas", ".texturesetc");
        } else if(str.endsWith(".tilesource")) {
            out = BuilderUtil.replaceExt(out, ".tilesource", ".texturesetc");
        } else {
            out = ProtoBuilders.replaceTextureName(str);
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

    private static ArrayList<NodeDesc> mergeNodes(NodeDesc parentNode, List<NodeDesc> nodes, HashMap<String, NodeDesc> layoutNodes, HashMap<String, HashMap<String, NodeDesc>> parentSceneNodeMap, String layout) {
        HashMap<String, NodeDesc> nodeMapDefault = layoutNodes == null ? parentSceneNodeMap.get("") : layoutNodes;
        HashMap<String, NodeDesc> nodeMap = parentSceneNodeMap.get(layout);
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
            NodeDesc parentSceneNode = (nodeMap == null) ? null : nodeMap.get(b.getId());
            if((parentSceneNode == null) && (nodeMapDefault != null)) {
                parentSceneNode = nodeMapDefault.get(b.getId());
            }
            if(parentSceneNode != null && parentSceneNode.getOverriddenFieldsCount() != 0) {
                List<Integer> overriddenFields = parentSceneNode.getOverriddenFieldsList();
                for(int fieldNumber : overriddenFields) {
                    FieldDescriptor fieldDesc = parentSceneNode.getDescriptorForType().findFieldByNumber(fieldNumber);
                    if(fieldDesc != null) {
                       b.setField(fieldDesc, parentSceneNode.getField(fieldDesc));
                    }
                }
            }

            // opt fields ignored by run-time
            b.clearOverriddenFields();
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

        if(builder != null) {
            // transform and register scene external resources (if compiling)
            sceneBuilder.setScript(BuilderUtil.replaceExt(sceneBuilder.getScript(), ".gui_script", ".gui_scriptc"));
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
                if (spineSceneNames.contains(f.getName())) {
                    throw new CompileExceptionError(builder.project.getResource(input), 0, BobNLS.bind(Messages.GuiBuilder_DUPLICATED_SPINESCENE,
                            f.getName()));
                }
                spineSceneNames.add(f.getName());
                newSpineSceneList.add(SpineSceneDesc.newBuilder().mergeFrom(f).setSpineScene(BuilderUtil.replaceExt(f.getSpineScene(), ".spinescene", ".rigscenec")).build());
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
            for (SpineSceneDesc f : sceneBuilder.getSpineScenesList()) {
                spineSceneNames.add(f.getName());
                newSpineSceneList.add(f);
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
            if(!node.hasAlpha()) {
                // We copy the color Vector4 W component from the old gui file format to the new separate alpha fields for color, outline and shadow.
                // They need to be separate fields as they can be separately overridden from their corresponding color property.
                // We distinct the new color/alpha separation by the fact that the alpha field (color alpha) is always present in the new format.
                // Scenes containing layouts won't need this conversion (they will have been saved with separate fields), so this is a good place to do it in code.
                // Templates are updated recursively, so conversion point will always be applied to default layout nodes.
                NodeDesc.Builder newNode = node.toBuilder();
                newNode.setAlpha(newNode.getColor().getW());
                newNode.setShadowAlpha(newNode.getShadow().getW());
                newNode.setOutlineAlpha(newNode.getOutline().getW());
                node = newNode.build();
            }

            // add current scene nodes
            newScene.get("").add(node);
            validateNodeResources(node, builder, input, fontNames, spineSceneNames, particlefxNames, textureNames, layerNames);
            for(String layout : layouts) {
                NodeDesc n = nodeMap.get(layout).get(node.getId());
                if(n != null) {
                    validateNodeResources(n, builder, input, fontNames, spineSceneNames, particlefxNames, textureNames, layerNames);
                    newScene.get(layout).add(n);
                }
            }

            // read in template scene (text version) and transform recursively
            if(node.getType() == Type.TYPE_TEMPLATE) {
                SceneDesc.Builder templateBuilder = sceneIO.readScene(node.getTemplate(), sceneResourceCache);
                templateBuilder = transformScene(builder, node.getTemplate(), templateBuilder, sceneIO, sceneResourceCache, false);

                // merge template scene nodes with overrides of current scene
                List<NodeDesc> nodes = mergeNodes(node, templateBuilder.getNodesList(), null, nodeMap, "");
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
                        nodes = mergeNodes(node, templateBuilder.getNodesList(), layoutNodes, nodeMap, templateLayoutName);
                    } else {
                        templateLayoutName = "";
                        layoutNodes = new HashMap<String, NodeDesc>(templateBuilder.getNodesCount());
                        for(NodeDesc n : templateBuilder.getNodesList()) {
                            layoutNodes.put(n.getId(), n);
                        }
                        nodes = mergeNodes(node, templateBuilder.getNodesList(), layoutNodes, nodeMap, layout.getName());
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
                for (SpineSceneDesc f : templateBuilder.getSpineScenesList()) {
                    if (spineSceneNames.contains(f.getName())) {
                        continue;
                    }
                    spineSceneNames.add(f.getName());
                    newSpineSceneList.add(f);
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
            else if(node.getType() == Type.TYPE_SPINE) {

                // If compiling we need to add child nodes for all bones
                if (builder != null) {

                    // Expand spine node
                    String spineNodeId = node.getId();
                    String spineSceneId = node.getSpineScene();
                    String spineScenePath = null;
                    for (SpineSceneDesc f : sceneBuilder.getSpineScenesList()) {
                        if (f.getName().equals(spineSceneId)) {
                            spineScenePath = f.getSpineScene();
                            break;
                        }
                    }
                    if (spineScenePath == null) {
                        throw new CompileExceptionError(builder.project.getResource(input), 0, "Could not generate bone nodes from invalid spine scene: " + spineSceneId);
                    }

                    // Need to parse the spine JSON
                    com.dynamo.spine.proto.Spine.SpineSceneDesc.Builder spineSceneBuilder = com.dynamo.spine.proto.Spine.SpineSceneDesc.newBuilder();
                    IResource spineSceneRes = builder.project.getResource(spineScenePath);
                    TextFormat.merge(new InputStreamReader(new ByteArrayInputStream(spineSceneRes.getContent()), "ASCII"), spineSceneBuilder);

                    IResource jsonRes = builder.project.getResource(spineSceneBuilder.getSpineJson());
                    try {
                        SpineSceneUtil rigScene = SpineSceneUtil.loadJson(new ByteArrayInputStream(jsonRes.getContent()), new UVTransformProvider() {
                            @Override
                            public UVTransform getUVTransform(String animId) {
                                return new UVTransform();
                            }
                        });

                        Vector4 oneV4 = Vector4.newBuilder().setX(1.0f).setY(1.0f).setZ(1.0f).setW(0.0f).build();
                        Vector4 zeroV4 = Vector4.newBuilder().setX(0.0f).setY(0.0f).setZ(0.0f).setW(0.0f).build();

                        HashMap<RigUtil.Bone,String> boneToId = new HashMap<RigUtil.Bone, String>();
                        for (int b = 0; b < rigScene.bones.size(); b++) {
                            RigUtil.Bone bone = rigScene.bones.get(b);
                            NodeDesc.Builder boneNodeBuilder = NodeDesc.newBuilder();

                            String id = spineNodeId + "/" + bone.name;
                            boneToId.put(bone, id);

                            boneNodeBuilder.setId(id);
                            boneNodeBuilder.setSpineNodeChild(true);
                            boneNodeBuilder.setSize(zeroV4);
                            boneNodeBuilder.setPosition(zeroV4);
                            boneNodeBuilder.setType(Type.TYPE_BOX);
                            boneNodeBuilder.setAdjustMode(node.getAdjustMode());
                            boneNodeBuilder.setScale(oneV4);

                            String parentId = boneToId.get(bone.parent);
                            if (b == 0) {
                                parentId = spineNodeId;
                            }
                            boneNodeBuilder.setParent(parentId);

                            NodeDesc boneNode = boneNodeBuilder.build();
                            newScene.get("").add(boneNode);
                            for(LayoutDesc layout : sceneBuilder.getLayoutsList()) {
                                newScene.get(layout.getName()).add(boneNode);
                            }
                        }

                    } catch (SpineSceneUtil.LoadException e) {
                        throw new CompileExceptionError(builder.project.getResource(input), 0, "Error while loading spine scene when building GUI: " + e.getLocalizedMessage());
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
        sceneBuilder.addAllSpineScenes(newSpineSceneList);
        
        sceneBuilder.clearParticlefxs();
        sceneBuilder.addAllParticlefxs(newParticleFXList);

        sceneBuilder.clearTextures();
        sceneBuilder.addAllTextures(newTextureList);

        return sceneBuilder;
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
                resourceCache.put(resourcePath, sceneBuilder);
            }
            return sceneBuilder.clone();
        }
    }

    @Override()
    protected SceneDesc.Builder transform(Task<Void> task, IResource input, SceneDesc.Builder messageBuilder) throws IOException, CompileExceptionError {
        HashMap<String, SceneDesc.Builder> sceneResourceCache = new HashMap<String, SceneDesc.Builder>(32);
        SceneDesc.Builder builder = null;
        builder = transformScene(this, input.getPath(), messageBuilder, new SceneBuilderIO(this.project), sceneResourceCache, true);
        return builder;
    }

}

