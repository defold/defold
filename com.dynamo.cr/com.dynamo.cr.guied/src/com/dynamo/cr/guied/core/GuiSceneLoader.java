package com.dynamo.cr.guied.core;

import java.io.File;
import java.io.FileInputStream;
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

import com.dynamo.cr.editor.core.EditorUtil;
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
import com.dynamo.gui.proto.Gui.SceneDesc.TextureDesc;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class GuiSceneLoader implements INodeLoader<GuiSceneNode> {

    private static GuiNode loadNode(NodeDesc desc) {
        GuiNode node = null;
        if (desc.getType() == Type.TYPE_BOX) {
            BoxNode boxNode = new BoxNode();
            boxNode.setTexture(desc.getTexture());
            boxNode.setSlice9(LoaderUtil.toVector4(desc.getSlice9()));
            node = boxNode;
        } else if (desc.getType() == Type.TYPE_PIE) {
            PieNode pieNode = new PieNode();
            pieNode.setTexture(desc.getTexture());
            pieNode.setPerimeterVertices(desc.getPerimeterVertices());
            pieNode.setOuterBounds(desc.getOuterBounds());
            pieNode.setInnerRadius(desc.getInnerRadius());
            pieNode.setPieFillAngle(desc.getPieFillAngle());
            node = pieNode;
        } else if (desc.getType() == Type.TYPE_TEXT) {
            TextNode textNode = new TextNode();
            textNode.setText(desc.getText());
            textNode.setFont(desc.getFont());
            textNode.setOutline(LoaderUtil.toRGB(desc.getOutline()));
            textNode.setOutlineAlpha(desc.getOutline().getW());
            textNode.setShadow(LoaderUtil.toRGB(desc.getShadow()));
            textNode.setShadowAlpha(desc.getShadow().getW());
            textNode.setLineBreak(desc.getLineBreak());
            node = textNode;
        }
        node.setId(desc.getId());
        node.setTranslation(LoaderUtil.toPoint3d(desc.getPosition()));
        node.setEuler(LoaderUtil.toVector3(desc.getRotation()));
        node.setScale(LoaderUtil.toVector3(desc.getScale()));
        node.setSize(LoaderUtil.toVector3(desc.getSize()));
        node.setColor(LoaderUtil.toRGB(desc.getColor()));
        node.setAlpha(desc.getColor().getW());
        node.setBlendMode(desc.getBlendMode());
        node.setPivot(desc.getPivot());
        node.setXanchor(desc.getXanchor());
        node.setYanchor(desc.getYanchor());
        node.setAdjustMode(desc.getAdjustMode());
        node.setLayer(desc.getLayer());
        node.setInheritAlpha(desc.getInheritAlpha());
        node.setClippingMode(desc.getClippingMode());
        node.setClippingVisible(desc.getClippingVisible());
        node.setClippingInverted(desc.getClippingInverted());
        return node;
    }

    @Override
    public GuiSceneNode load(ILoaderContext context, InputStream contents) throws IOException, CoreException {
        InputStreamReader reader = new InputStreamReader(contents);
        com.dynamo.gui.proto.Gui.SceneDesc.Builder builder = SceneDesc.newBuilder();
        TextFormat.merge(reader, builder);
        SceneDesc sceneDesc = builder.build();
        GuiSceneNode node = new GuiSceneNode();
        node.setScript(sceneDesc.getScript());
        node.setMaterial(sceneDesc.getMaterial());
        if (sceneDesc.hasBackgroundColor()) {
            node.setBackgroundColor(LoaderUtil.toRGB(sceneDesc.getBackgroundColor()));
        }

        Map<String, GuiNode> idToInstance = new HashMap<String, GuiNode>();
        int n = sceneDesc.getNodesCount();
        List<GuiNode> remainingInstances = new ArrayList<GuiNode>(n);
        for (int i = 0; i < n; ++i) {
            NodeDesc nodeDesc = sceneDesc.getNodes(i);
            GuiNode guiNode = loadNode(nodeDesc);
            idToInstance.put(nodeDesc.getId(), guiNode);
            remainingInstances.add(guiNode);
        }
        for (int i = 0; i < n; ++i) {
            NodeDesc nodeDesc = sceneDesc.getNodes(i);
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

        n = sceneDesc.getFontsCount();
        Node fontsNode = node.getFontsNode();
        for (FontDesc f : sceneDesc.getFontsList()) {
            FontNode font = new FontNode();
            font.setId(f.getName());
            font.setFont(f.getFont());
            fontsNode.addChild(font);
        }

        n = sceneDesc.getTexturesCount();
        Node texturesNode = node.getTexturesNode();
        for (TextureDesc t : sceneDesc.getTexturesList()) {
            TextureNode texture = new TextureNode();
            texture.setId(t.getName());
            texture.setTexture(t.getTexture());
            texturesNode.addChild(texture);
        }

        n = sceneDesc.getLayersCount();
        Node layersNode = node.getLayersNode();
        for (LayerDesc l : sceneDesc.getLayersList()) {
            LayerNode layer = new LayerNode();
            layer.setId(l.getName());
            layersNode.addChild(layer);
        }

        // Projects are not available when running tests
        IProject project = EditorUtil.getProject();
        if (project != null) {
            URI projectPropertiesLocation = EditorUtil.getContentRoot(project).getFile("game.project")
                    .getRawLocationURI();
            File localProjectPropertiesFile = EFS.getStore(projectPropertiesLocation).toLocalFile(0,
                    new NullProgressMonitor());
            if (localProjectPropertiesFile.isFile()) {
                // in cr.integrationstest the root isn't /content and the
                // file doesn't exists. That's the reason we accept missing game.project
                FileInputStream in = new FileInputStream(localProjectPropertiesFile);
                node.loadProjectProperties(in);
                IOUtils.closeQuietly(in);
            }
        }

        return node;
    }

    private NodeDesc.Builder buildNode(GuiNode node) {
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
            builder.setOutline(LoaderUtil.toVector4(text.getOutline(), text.getOutlineAlpha()));
            builder.setShadow(LoaderUtil.toVector4(text.getShadow(), text.getShadowAlpha()));
            builder.setLineBreak(text.isLineBreak());
        }
        builder.setId(node.getId());
        builder.setPosition(LoaderUtil.toVector4(node.getTranslation()));
        builder.setRotation(LoaderUtil.toVector4(node.getEuler()));
        builder.setScale(LoaderUtil.toVector4(node.getScale()));
        builder.setSize(LoaderUtil.toVector4(node.getSize()));
        builder.setColor(LoaderUtil.toVector4(node.getColor(), node.getAlpha()));
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

    private void collectNodes(Builder b, GuiNode node, String parent) {
        NodeDesc.Builder builder = buildNode(node);
        if (parent != null) {
            builder.setParent(parent);
        }
        b.addNodes(builder);
        for (Node n : node.getChildren()) {
            collectNodes(b, (GuiNode) n, node.getId());
        }
    }

    @Override
    public Message buildMessage(ILoaderContext context, GuiSceneNode node, IProgressMonitor monitor)
                                                                                                    throws IOException,
                                                                                                    CoreException {
        Builder b = SceneDesc.newBuilder();
        b.setScript(node.getScript());
        b.setMaterial(node.getMaterial());
        b.setBackgroundColor(LoaderUtil.toVector4(node.getBackgroundColor(), 1.0));
        for (Node n : node.getNodesNode().getChildren()) {
            collectNodes(b, (GuiNode) n, null);
        }
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
        for (Node n : node.getLayersNode().getChildren()) {
            LayerNode layerNode = (LayerNode) n;
            LayerDesc.Builder builder = LayerDesc.newBuilder();
            builder.setName(layerNode.getId());
            b.addLayers(builder);
        }
        return b.build();
    }

}
