package com.dynamo.cr.guied.core;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import javax.vecmath.Vector3d;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.StructuredSelection;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.IFileEditorInput;
import org.eclipse.ui.statushandlers.StatusManager;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.guied.Activator;
import com.dynamo.cr.guied.core.TemplateNode;
import com.dynamo.cr.guied.operations.AddFontsOperation;
import com.dynamo.cr.guied.operations.AddGuiNodeOperation;
import com.dynamo.cr.guied.operations.AddLayersOperation;
import com.dynamo.cr.guied.operations.AddLayoutOperation;
import com.dynamo.cr.guied.operations.AddParticleFXScenesOperation;
import com.dynamo.cr.guied.operations.AddSpineScenesOperation;
import com.dynamo.cr.guied.operations.AddTexturesOperation;
import com.dynamo.cr.guied.util.GuiNodeStateBuilder;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.operations.SelectOperation;

public class GuiScenePresenter implements ISceneView.INodePresenter<GuiSceneNode> {
    private static Logger logger = LoggerFactory.getLogger(GuiScenePresenter.class);

    private GuiSceneNode findSceneFromSelection(IStructuredSelection selection) {
        Object[] nodes = selection.toArray();
        GuiSceneNode parent = null;
        for (Object node : nodes) {
            if (node instanceof GuiSceneNode) {
                parent = (GuiSceneNode) node;
                break;
            } else if (node instanceof Node) {
                Node root = ((Node) node).getParent();
                while (root.getParent() != null) {
                    root = root.getParent();
                }
                if (root instanceof GuiSceneNode) {
                    parent = (GuiSceneNode) root;
                    break;
                }
            }
        }
        return parent;
    }

    private Node findGuiNodeParentFromSelection(IStructuredSelection selection) {
        Object[] nodes = selection.toArray();
        Node parent = null;
        for (Object node : nodes) {
            if (node instanceof GuiSceneNode) {
                parent = ((GuiSceneNode) node).getNodesNode();
                break;
            } else if (node instanceof NodesNode || node instanceof GuiNode) {
                return (Node) node;
            }
        }
        return parent;
    }

    public void onAddBoxNode(IPresenterContext context) {
        Node scene = findGuiNodeParentFromSelection(context.getSelection());
        BoxNode node = new BoxNode();
        node.setId("box");
        context.executeOperation(new AddGuiNodeOperation(scene, node, context));
    }

    public void onAddPieNode(IPresenterContext context) {
        Node scene = findGuiNodeParentFromSelection(context.getSelection());
        PieNode node = new PieNode();
        node.setId("pie");
        context.executeOperation(new AddGuiNodeOperation(scene, node, context));
    }

    public void onAddTextNode(IPresenterContext context) {
        Node scene = findGuiNodeParentFromSelection(context.getSelection());
        TextNode node = new TextNode();
        node.setId("text");
        context.executeOperation(new AddGuiNodeOperation(scene, node, context));
    }

    public void onAddTemplateNode(IPresenterContext context, IEditorPart editorPart) {
        String parentScenePath = ((IFileEditorInput)editorPart.getEditorInput()).getFile().getFullPath().toString();

        Node scene = findGuiNodeParentFromSelection(context.getSelection());
        String file = context.selectFile("Add Gui Scene", new String[] {"gui"});
        String filePath = scene.getModel().getContentRoot().getFullPath() + file;
        if(filePath.equals(parentScenePath)) {
            Status status = new Status(IStatus.CANCEL, Activator.PLUGIN_ID, "Unable to add template scene '" + file + "'. Recursive hierarchies not supported", null);
            StatusManager.getManager().handle(status, StatusManager.SHOW);
            return;
        }

        if (file != null) {
            TemplateNode templateScene = new TemplateNode();
            try {
                templateScene.setTemplate(file);
                templateScene.setId(new File(file).getName().replace(".gui", ""));
            } catch (Exception e) {
                logger.error("Error occurred while adding gui scene", e);
                return;
            }
            context.executeOperation(new AddGuiNodeOperation(scene, templateScene, context));
        }
    }
    
    public void onAddSpineNode(IPresenterContext context) {
        Node scene = findGuiNodeParentFromSelection(context.getSelection());
        SpineNode node = new SpineNode();
        node.setId("spine");
        context.executeOperation(new AddGuiNodeOperation(scene, node, context));
    }
    
    public void onAddParticleFXNode(IPresenterContext context) {
        Node scene = findGuiNodeParentFromSelection(context.getSelection());
        ParticleFXNode node = new ParticleFXNode();
        node.setId("particlefx");
        context.executeOperation(new AddGuiNodeOperation(scene, node, context));
    }

    public void onAddTextureNode(IPresenterContext context) {
        String[] textures = context.selectFiles("Add Textures", new String[] { "atlas", "tilesource" });
        if (textures != null) {
            GuiSceneNode scene = findSceneFromSelection(context.getSelection());
            List<Node> nodes = new ArrayList<Node>(textures.length);
            for (String texture : textures) {
                nodes.add(new TextureNode(texture));
            }
            context.executeOperation(new AddTexturesOperation(scene, nodes, context));
        }
    }

    public void onAddFontNode(IPresenterContext context) {
        String[] fonts = context.selectFiles("Add Fonts", new String[] { "font" });
        if (fonts != null) {
            GuiSceneNode scene = findSceneFromSelection(context.getSelection());
            List<Node> nodes = new ArrayList<Node>(fonts.length);
            for (String font : fonts) {
                nodes.add(new FontNode(font));
            }
            context.executeOperation(new AddFontsOperation(scene, nodes, context));
        }
    }
    
    public void onAddSpineSceneNode(IPresenterContext context) {
        String[] spineScenes = context.selectFiles("Add Spine Scenes", new String[] { "spinescene" });
        if (spineScenes != null) {
            GuiSceneNode scene = findSceneFromSelection(context.getSelection());
            List<Node> nodes = new ArrayList<Node>(spineScenes.length);
            for (String spineScene : spineScenes) {
                nodes.add(new SpineSceneNode(spineScene));
            }
            context.executeOperation(new AddSpineScenesOperation(scene, nodes, context));
        }
    }
    
    public void onAddParticleFXSceneNode(IPresenterContext context) {
        String[] particlefxs = context.selectFiles("Add ParticleFX", new String[] { "particlefx" });
        if (particlefxs != null) {
            GuiSceneNode scene = findSceneFromSelection(context.getSelection());
            List<Node> nodes = new ArrayList<Node>(particlefxs.length);
            for (String particlefx : particlefxs) {
                nodes.add(new ParticleFXSceneNode(particlefx));
            }
            context.executeOperation(new AddParticleFXScenesOperation(scene, nodes, context));
        }
    }

    public void onAddLayerNode(IPresenterContext context) {
        GuiSceneNode scene = findSceneFromSelection(context.getSelection());
        List<Node> nodes = Collections.singletonList((Node) new LayerNode("layer"));
        context.executeOperation(new AddLayersOperation(scene, nodes, context));
    }

    public void onSelectLayoutNode(IPresenterContext context, String id) {
        GuiSceneNode scene = findSceneFromSelection(context.getSelection());
        LayoutsNode layoutsNode = ((LayoutsNode)scene.getLayoutsNode());
        List<String> layouts = layoutsNode.getLayouts();
        if(layouts.contains(id)) {
            context.executeOperation(new SelectOperation(context.getSelection(), new StructuredSelection(((LayoutsNode) scene.getLayoutsNode()).getLayoutNode(id)), context));
        } else {
            List<Node> nodes = Collections.singletonList((Node) new LayoutNode(id));
            Node nodesNode = scene.getNodesNode();
            GuiNodeStateBuilder.storeStates(nodesNode.getChildren());
            context.executeOperation(new AddLayoutOperation(scene, nodes, context));
            GuiNodeStateBuilder.cloneStates(nodesNode.getChildren(), id, scene.getCurrentLayout().getId());
            scene.setCurrentLayout(id);
        }

    }

}
