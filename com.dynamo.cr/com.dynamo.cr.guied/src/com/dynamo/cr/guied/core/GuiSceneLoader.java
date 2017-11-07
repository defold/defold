package com.dynamo.cr.guied.core;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.URI;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.apache.commons.io.IOUtils;
import org.eclipse.core.filesystem.EFS;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.guied.util.GuiNodeStateBuilder;
import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.util.LoaderUtil;
import com.dynamo.gui.proto.Gui.NodeDesc;
import com.dynamo.gui.proto.Gui.NodeDesc.Type;
import com.dynamo.gui.proto.Gui.SceneDesc;
import com.dynamo.gui.proto.Gui.SceneDesc.Builder;
import com.dynamo.gui.proto.Gui.SceneDesc.FontDesc;
import com.dynamo.gui.proto.Gui.SceneDesc.LayerDesc;
import com.dynamo.gui.proto.Gui.SceneDesc.LayoutDesc;
import com.dynamo.gui.proto.Gui.SceneDesc.ParticleFXDesc;
import com.dynamo.gui.proto.Gui.SceneDesc.SpineSceneDesc;
import com.dynamo.gui.proto.Gui.SceneDesc.TextureDesc;
import com.google.protobuf.Descriptors.FieldDescriptor;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class GuiSceneLoader implements INodeLoader<GuiSceneNode> {
    private static Logger logger = LoggerFactory.getLogger(GuiSceneLoader.class);

    public static void builderToNode(GuiNode node, NodeDesc.Builder builder) {
        if (builder.getType() == Type.TYPE_BOX) {
            BoxNode boxNode = (BoxNode) node;
            boxNode.setTexture(builder.getTexture());
            boxNode.setSlice9(LoaderUtil.toVector4(builder.getSlice9()));
            node = boxNode;
        } else if (builder.getType() == Type.TYPE_PIE) {
            PieNode pieNode = (PieNode) node;
            pieNode.setTexture(builder.getTexture());
            pieNode.setPerimeterVertices(builder.getPerimeterVertices());
            pieNode.setOuterBounds(builder.getOuterBounds());
            pieNode.setInnerRadius(builder.getInnerRadius());
            pieNode.setPieFillAngle(builder.getPieFillAngle());
            node = pieNode;
        } else if (builder.getType() == Type.TYPE_TEXT) {
            TextNode textNode = (TextNode) node;
            textNode.setText(builder.getText());
            textNode.setFont(builder.getFont());
            textNode.setOutline(LoaderUtil.toRGB(builder.getOutline()));
            textNode.setOutlineAlpha(builder.hasOutlineAlpha() ? builder.getOutlineAlpha() : builder.getOutline().getW());
            textNode.setShadow(LoaderUtil.toRGB(builder.getShadow()));
            textNode.setShadowAlpha(builder.hasShadowAlpha() ? builder.getShadowAlpha() : builder.getShadow().getW());
            textNode.setLineBreak(builder.getLineBreak());
            textNode.setLeading(builder.getTextLeading());
            textNode.setTracking(builder.getTextTracking());
            node = textNode;
        } else if (builder.getType() == Type.TYPE_TEMPLATE) {
            TemplateNode templateNode = (TemplateNode) node;
            templateNode.setTemplatePath(builder.getTemplate());
        } else if (builder.getType() == Type.TYPE_SPINE) {
            SpineNode spineNode = (SpineNode) node;
            spineNode.setSpineScene(builder.getSpineScene());
            spineNode.setSkin(builder.getSpineSkin());
            spineNode.setSpineDefaultAnimation(builder.getSpineDefaultAnimation());
            node = spineNode;
        } else if (builder.getType() == Type.TYPE_PARTICLEFX) {
            ParticleFXNode pfxNode = (ParticleFXNode) node;
            pfxNode.setParticlefx(builder.getParticlefx());
            node = pfxNode;
        }
        node.setSizeMode(builder.getSizeMode());
        node.setId(builder.getId());
        node.setTranslation(LoaderUtil.toPoint3d(builder.getPosition()));
        node.setEuler(LoaderUtil.toVector3(builder.getRotation()));
        node.setScale(LoaderUtil.toVector3(builder.getScale()));
        node.setSize(LoaderUtil.toVector3(builder.getSize()));
        node.setColor(LoaderUtil.toRGB(builder.getColor()));
        node.setAlpha(builder.hasAlpha() ? builder.getAlpha() : builder.getColor().getW());
        node.setBlendMode(builder.getBlendMode());
        node.setPivot(builder.getPivot());
        node.setXanchor(builder.getXanchor());
        node.setYanchor(builder.getYanchor());
        node.setAdjustMode(builder.getAdjustMode());
        node.setLayer(builder.getLayer());
        node.setInheritAlpha(builder.getInheritAlpha());
        node.setClippingMode(builder.getClippingMode());
        node.setClippingVisible(builder.getClippingVisible());
        node.setClippingInverted(builder.getClippingInverted());
    }

    public static NodeDesc.Builder nodeToBuilder(GuiNode node) {
        NodeDesc.Builder builder = NodeDesc.newBuilder();
        if (node instanceof BoxNode) {
            builder.setType(NodeDesc.Type.TYPE_BOX);
            BoxNode box = (BoxNode)node;
            builder.setTexture(box.getTexture());
            builder.setSlice9(LoaderUtil.toVector4(box.getSlice9()));
        } else if (node instanceof PieNode) {
            builder.setType(NodeDesc.Type.TYPE_PIE);
            PieNode box = (PieNode)node;
            builder.setTexture(box.getTexture());
            builder.setPerimeterVertices(box.getPerimeterVertices());
            builder.setInnerRadius(box.getInnerRadius());
            builder.setOuterBounds(box.getOuterBounds());
            builder.setPieFillAngle(box.getPieFillAngle());
        } else if (node instanceof TextNode) {
            builder.setType(NodeDesc.Type.TYPE_TEXT);
            TextNode text = (TextNode)node;
            builder.setText(text.getText());
            builder.setFont(text.getFont());
            builder.setOutline(LoaderUtil.toVector4(text.getOutline(), 1));
            builder.setOutlineAlpha((float)text.getOutlineAlpha());
            builder.setShadow(LoaderUtil.toVector4(text.getShadow(), 1));
            builder.setShadowAlpha((float)text.getShadowAlpha());
            builder.setLineBreak(text.isLineBreak());
            builder.setTextLeading((float)text.getLeading());
            builder.setTextTracking((float)text.getTracking());
        } else if (node instanceof TemplateNode) {
            builder.setType(NodeDesc.Type.TYPE_TEMPLATE);
            TemplateNode templateNode = (TemplateNode) node;
            builder.setTemplate(templateNode.getTemplatePath());
        } else if (node instanceof SpineNode) {
            builder.setType(NodeDesc.Type.TYPE_SPINE);
            SpineNode spine = (SpineNode)node;
            builder.setSpineScene(spine.getSpineScene());
            builder.setSpineSkin(spine.getSkin());
            builder.setSpineDefaultAnimation(spine.getSpineDefaultAnimation());
        } else if (node instanceof ParticleFXNode) {
            builder.setType(NodeDesc.Type.TYPE_PARTICLEFX);
            ParticleFXNode pfxNode = (ParticleFXNode) node;
            builder.setParticlefx(pfxNode.getParticlefx());
        }
        builder.setSizeMode(node.getSizeMode());
        builder.setId(node.getId());
        builder.setPosition(LoaderUtil.toVector4(node.getTranslation()));
        builder.setRotation(LoaderUtil.toVector4(node.getEuler()));
        builder.setScale(LoaderUtil.toVector4(node.getScale()));
        builder.setSize(LoaderUtil.toVector4(node.getSize()));
        builder.setColor(LoaderUtil.toVector4(node.getColor(), 1));
        builder.setAlpha((float)node.getAlpha());
        builder.setBlendMode(node.getBlendMode());
        builder.setPivot(node.getPivot());
        builder.setXanchor(node.getXanchor());
        builder.setYanchor(node.getYanchor());
        builder.setAdjustMode(node.getAdjustMode());
        builder.setLayer(node.getLayer());
        builder.setInheritAlpha(node.isInheritAlpha());
        builder.setClippingMode(node.getClippingMode());
        builder.setClippingVisible(node.getClippingVisible());
        builder.setClippingInverted(node.getClippingInverted());
        return builder;
    }

    private static GuiNode loadNode(NodeDesc.Builder descBuilder) {
        GuiNode node = null;
        if (descBuilder.getType() == Type.TYPE_BOX) {
            BoxNode boxNode = new BoxNode();
            builderToNode(boxNode, descBuilder);
            node = boxNode;
        } else if (descBuilder.getType() == Type.TYPE_PIE) {
            PieNode pieNode = new PieNode();
            builderToNode(pieNode, descBuilder);
            node = pieNode;
        } else if (descBuilder.getType() == Type.TYPE_TEXT) {
            TextNode textNode = new TextNode();
            builderToNode(textNode, descBuilder);
            node = textNode;
        } else if (descBuilder.getType() == Type.TYPE_TEMPLATE) {
            TemplateNode templateNode = new TemplateNode();
            builderToNode(templateNode, descBuilder);
            node = templateNode;
        } else if (descBuilder.getType() == Type.TYPE_SPINE) {
            SpineNode spineNode = new SpineNode();
            builderToNode(spineNode, descBuilder);
            node = spineNode;
        } else if (descBuilder.getType() == Type.TYPE_PARTICLEFX) {
            ParticleFXNode pfxNode = new ParticleFXNode();
            builderToNode(pfxNode, descBuilder);
            node = pfxNode;
        }
        return node;
    }

    @Override
    public GuiSceneNode load(ILoaderContext context, InputStream contents) throws IOException, CoreException {
        InputStreamReader reader = new InputStreamReader(contents);
        com.dynamo.gui.proto.Gui.SceneDesc.Builder builder = SceneDesc.newBuilder();
        TextFormat.merge(reader, builder);
        return load(builder);
    }

    static public GuiSceneNode load(SceneDesc.Builder sceneBuilder) {
        GuiSceneNode node = new GuiSceneNode();
        node.setScript(sceneBuilder.getScript());
        node.setMaterial(sceneBuilder.getMaterial());
        node.setAdjustReference(sceneBuilder.getAdjustReference());
        node.setMaxNodes(sceneBuilder.getMaxNodes());
        if (sceneBuilder.hasBackgroundColor()) {
            node.setBackgroundColor(LoaderUtil.toRGB(sceneBuilder.getBackgroundColor()));
        }

        Node layoutsNode = node.getLayoutsNode();
        layoutsNode.addChild(new LayoutNode(GuiNodeStateBuilder.getDefaultStateId()));
        HashMap<String, GuiNodeStateBuilder> nodeStatesMap = new HashMap<String, GuiNodeStateBuilder>(sceneBuilder.getNodesCount());
        for (LayoutDesc l : sceneBuilder.getLayoutsList()) {
            LayoutNode layout = new LayoutNode();
            layout.setId(l.getName());
            layoutsNode.addChild(layout);

            for(NodeDesc nodeDesc : l.getNodesList()) {
                GuiNodeStateBuilder nodeStates = nodeStatesMap.getOrDefault(nodeDesc.getId(), null);
                if(nodeStates == null) {
                    nodeStates = new GuiNodeStateBuilder();
                    nodeStatesMap.put(nodeDesc.getId(), nodeStates);
                }
                NodeDesc.Builder nodeBuilder = nodeDesc.toBuilder();
                List<Integer> overriddenFieldNumbers = nodeBuilder.getOverriddenFieldsList();
                for(FieldDescriptor field : nodeBuilder.getAllFields().keySet()) {
                    if(overriddenFieldNumbers.contains(field.getNumber())) {
                        continue;
                    }
                    nodeBuilder.clearField(field);
                }
                GuiNodeStateBuilder.getBuilders(nodeStates).put(l.getName(), nodeBuilder);
            }
        }

        Map<String, GuiNode> idToInstance = new HashMap<String, GuiNode>();
        int n = sceneBuilder.getNodesCount();
        List<GuiNode> remainingInstances = new ArrayList<GuiNode>(n);
        for (int i = 0; i < n; ++i) {
            NodeDesc.Builder descBuilder = sceneBuilder.getNodes(i).toBuilder();
            GuiNode guiNode = loadNode(descBuilder);
            idToInstance.put(descBuilder.getId(), guiNode);
            remainingInstances.add(guiNode);
            GuiNodeStateBuilder nodeStates = nodeStatesMap.getOrDefault(descBuilder.getId(), new GuiNodeStateBuilder());

            guiNode.setStateBuilder(nodeStates);
            GuiNodeStateBuilder.storeState(guiNode);
            if(descBuilder.getTemplateNodeChild()) {
                HashMap<String, NodeDesc.Builder> newDefaultStates = new HashMap<String, NodeDesc.Builder>();
                NodeDesc.Builder newDefaultBuilder = descBuilder.clone();
                newDefaultBuilder.clearOverriddenFields();
                newDefaultStates.put(GuiNodeStateBuilder.getDefaultStateId(), newDefaultBuilder);
                GuiNodeStateBuilder.setDefaultBuilders(guiNode.getStateBuilder(), newDefaultStates);

                NodeDesc.Builder defaultBuilder = GuiNodeStateBuilder.getBuilders(guiNode.getStateBuilder()).get(GuiNodeStateBuilder.getDefaultStateId());
                if(descBuilder.getOverriddenFieldsCount() != 0) {
                    List<Integer> ofList = descBuilder.getOverriddenFieldsList();
                    for(FieldDescriptor field : defaultBuilder.getAllFields().keySet()) {
                        if(ofList.contains(field.getNumber())) {
                            continue;
                        }
                        defaultBuilder.clearField(field);
                    }
                } else {
                    defaultBuilder.clear();
                }
            } else {

            }
        }
        for (int i = 0; i < n; ++i) {
            NodeDesc nodeDesc = sceneBuilder.getNodes(i);
            GuiNode guiNode = idToInstance.get(nodeDesc.getId());
            if (!nodeDesc.getParent().isEmpty()) {
                GuiNode parent = idToInstance.get(nodeDesc.getParent());
                parent.addChild(guiNode);
                remainingInstances.remove(guiNode);
            }
        }
        Node nodesNode = node.getNodesNode();
        for (GuiNode guiNode : remainingInstances) {
            nodesNode.addChild(guiNode);
        }

        Node fontsNode = node.getFontsNode();
        for (FontDesc f : sceneBuilder.getFontsList()) {
            FontNode font = new FontNode();
            font.setId(f.getName());
            font.setFont(f.getFont());
            fontsNode.addChild(font);
        }

        Node texturesNode = node.getTexturesNode();
        for (TextureDesc t : sceneBuilder.getTexturesList()) {
            TextureNode texture = new TextureNode();
            texture.setId(t.getName());
            texture.setTexture(t.getTexture());
            texturesNode.addChild(texture);
        }

        Node spineScenesNode = node.getSpineScenesNode();
        for (SpineSceneDesc s : sceneBuilder.getSpineScenesList()) {
            SpineSceneNode spineScene = new SpineSceneNode();
            spineScene.setId(s.getName());
            spineScene.setSpineScene(s.getSpineScene());
            spineScenesNode.addChild(spineScene);
        }
        
        Node pfxScenesNode = node.getParticleFXScenesNode();
        for (ParticleFXDesc p : sceneBuilder.getParticlefxsList()) {
            ParticleFXSceneNode pfxScene = new ParticleFXSceneNode();
            pfxScene.setId(p.getName());
            pfxScene.setParticlefx(p.getParticlefx());
            pfxScenesNode.addChild(pfxScene);
        }

        Node layersNode = node.getLayersNode();
        for (LayerDesc l : sceneBuilder.getLayersList()) {
            LayerNode layer = new LayerNode();
            layer.setId(l.getName());
            layersNode.addChild(layer);
        }

        // Projects are not available when running tests
        IProject project = EditorUtil.getProject();
        if (project != null) {
            URI projectPropertiesLocation = EditorUtil.getContentRoot(project).getFile("game.project").getRawLocationURI();
            File localProjectPropertiesFile = null;
            try {
                localProjectPropertiesFile = EFS.getStore(projectPropertiesLocation).toLocalFile(0, new NullProgressMonitor());
            } catch (CoreException e) {
                logger.error("Failed loading " + projectPropertiesLocation.getPath(), e);
            }
            if (localProjectPropertiesFile.isFile()) {
                // in cr.integrationstest the root isn't /content and the
                // file doesn't exists. That's the reason we accept missing game.project
                try {
                    FileInputStream in = new FileInputStream(localProjectPropertiesFile);
                    node.loadProjectProperties(in);
                    IOUtils.closeQuietly(in);
                } catch (FileNotFoundException e) {
                    logger.error("File not found: " + localProjectPropertiesFile.getPath(), e);
                }
            }

        }

        return node;
    }

    private static void buildNodes(Builder b, GuiNode node, String parent, HashMap<String, LayoutDesc.Builder> layoutBuilderMap) {
        Map<String, NodeDesc.Builder> nodeTopLevelStateMap = GuiNodeStateBuilder.getBuilders(node.getStateBuilder());
        GuiNodeStateBuilder nodeStateBuilder = node.getStateBuilder();
        Map<String, NodeDesc.Builder> nodeDefaultStateMap = GuiNodeStateBuilder.getDefaultBuilders(nodeStateBuilder) == null ? null : GuiNodeStateBuilder.getDefaultBuilders(nodeStateBuilder);
        Map<String, NodeDesc.Builder> nodeStateMap = nodeDefaultStateMap == null ? nodeTopLevelStateMap : nodeDefaultStateMap;

        NodeDesc.Builder defaultBuilder = nodeStateMap.get(GuiNodeStateBuilder.getDefaultStateId());
        Map<FieldDescriptor, Object> defaultBuilderFields = nodeStateMap.get(GuiNodeStateBuilder.getDefaultStateId()).getAllFields();
        for(String layoutId : layoutBuilderMap.keySet()) {
            NodeDesc.Builder builder = nodeStateMap.getOrDefault(layoutId, null);
            NodeDesc.Builder topLeveLBuilder = nodeTopLevelStateMap.getOrDefault(layoutId, null);

            if(builder == null) {
                if(nodeDefaultStateMap == null || topLeveLBuilder == null) {
                    continue;
                }
                builder = topLeveLBuilder;
            }
            builder = builder.clone();
            builder.clearOverriddenFields();

            if(layoutId.equals(GuiNodeStateBuilder.getDefaultStateId())) {
                if (parent != null) {
                    builder.setParent(parent);
                }

                if(nodeDefaultStateMap != null && topLeveLBuilder != null) {
                    for(FieldDescriptor field : topLeveLBuilder.getAllFields().keySet()) {
                        if(topLeveLBuilder.hasField(field)) {
                            builder.setField(field, topLeveLBuilder.getField(field));
                            builder.addOverriddenFields(field.getNumber());
                        }
                    }
                }
                builder.setId(node.getId());
                builder.setTemplateNodeChild(node.isTemplateNodeChild());
                b.addNodes(builder);

            } else {
                builder.clearId();
                builder.clearOverriddenFields();

                Map<FieldDescriptor, Object> builderFields = builder.getAllFields();
                if(builderFields.isEmpty()) {
                    continue;
                }
                if(nodeDefaultStateMap == null) {
                    for(FieldDescriptor field : defaultBuilderFields.keySet()) {
                        if(builder.hasField(field)) {
                            builder.addOverriddenFields(field.getNumber());
                        } else {
                            builder.setField(field, defaultBuilder.getField(field));
                        }
                    }
                } else {
                    for(FieldDescriptor field : defaultBuilderFields.keySet()) {
                        if(topLeveLBuilder != null && topLeveLBuilder.hasField(field)) {
                            builder.addOverriddenFields(field.getNumber());
                            builder.setField(field, topLeveLBuilder.getField(field));
                        } else if(!builder.hasField(field)) {
                            builder.setField(field, defaultBuilder.getField(field));
                        }
                    }
                }
                LayoutDesc.Builder layoutBuilder = layoutBuilderMap.get(layoutId);
                if (parent != null) {
                    builder.setParent(parent);
                }
                builder.setId(node.getId());
                builder.setTemplateNodeChild(node.isTemplateNodeChild());
                layoutBuilder.addNodes(builder);
            }
        }
    }

    private void collectNodes(Builder b, GuiNode node, String parent, HashMap<String, LayoutDesc.Builder> layoutBuilderMap) {
        buildNodes(b, node, parent, layoutBuilderMap);
        for (Node n : node.getChildren()) {
            collectNodes(b, (GuiNode) n, node.getId(), layoutBuilderMap);
        }
    }

    @Override
    public Message buildMessage(ILoaderContext context, GuiSceneNode node, IProgressMonitor monitor)
                                                                                                    throws IOException,
                                                                                                    CoreException {
        GuiNodeStateBuilder.storeStates(node.getNodesNode().getChildren());

        Builder b = SceneDesc.newBuilder();
        b.setScript(node.getScript());
        b.setMaterial(node.getMaterial());
        b.setAdjustReference(node.getAdjustReference());
        b.setMaxNodes(node.getMaxNodes());
        b.setBackgroundColor(LoaderUtil.toVector4(node.getBackgroundColor(), 1.0));
        for (Node n : node.getTexturesNode().getChildren()) {
            TextureNode texNode = (TextureNode) n;
            TextureDesc.Builder builder = TextureDesc.newBuilder();
            builder.setName(texNode.getId());
            builder.setTexture(texNode.getTexture());
            b.addTextures(builder);
        }
        for (Node n : node.getFontsNode().getChildren()) {
            FontNode fontNode = (FontNode) n;
            FontDesc.Builder builder = FontDesc.newBuilder();
            builder.setName(fontNode.getId());
            builder.setFont(fontNode.getFont());
            b.addFonts(builder);
        }
        for (Node n : node.getSpineScenesNode().getChildren()) {
            SpineSceneNode spineSceneNode = (SpineSceneNode) n;
            SpineSceneDesc.Builder builder = SpineSceneDesc.newBuilder();
            builder.setName(spineSceneNode.getId());
            builder.setSpineScene(spineSceneNode.getSpineScene());
            b.addSpineScenes(builder);
        }
        for (Node n : node.getParticleFXScenesNode().getChildren()) {
            ParticleFXSceneNode particlefxSceneNode = (ParticleFXSceneNode) n;
            ParticleFXDesc.Builder builder = ParticleFXDesc.newBuilder();
            builder.setName(particlefxSceneNode.getId());
            builder.setParticlefx(particlefxSceneNode.getParticlefx());
            b.addParticlefxs(builder);
        }
        for (Node n : node.getLayersNode().getChildren()) {
            LayerNode layerNode = (LayerNode) n;
            LayerDesc.Builder builder = LayerDesc.newBuilder();
            builder.setName(layerNode.getId());
            b.addLayers(builder);
        }
        HashMap<String, LayoutDesc.Builder> layoutBuilderMap = new HashMap<String, LayoutDesc.Builder>(node.getLayoutsNode().getChildren().size());
        for (Node n : node.getLayoutsNode().getChildren()) {
            LayoutNode layoutNode = (LayoutNode) n;
            LayoutDesc.Builder builder = LayoutDesc.newBuilder();
            builder.setName(layoutNode.getId());
            layoutBuilderMap.put(layoutNode.getId(), builder);
        }
        for (Node n : node.getNodesNode().getChildren()) {
            collectNodes(b, (GuiNode) n, null, layoutBuilderMap);
        }
        for(LayoutDesc.Builder builder : layoutBuilderMap.values()) {
            if(builder.getName().compareTo(GuiNodeStateBuilder.getDefaultStateId())!=0) {
                b.addLayouts(builder);
            }
        }
        return b.build();
    }

}
