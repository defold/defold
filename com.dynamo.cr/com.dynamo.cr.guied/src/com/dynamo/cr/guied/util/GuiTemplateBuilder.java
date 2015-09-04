package com.dynamo.cr.guied.util;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.pipeline.GuiBuilder;
import com.dynamo.bob.pipeline.Messages;
import com.dynamo.bob.util.BobNLS;
import com.dynamo.cr.guied.core.GuiNode;
import com.dynamo.cr.guied.core.GuiSceneLoader;
import com.dynamo.cr.guied.core.GuiSceneNode;
import com.dynamo.cr.guied.core.TemplateNode;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.gui.proto.Gui.NodeDesc;
import com.dynamo.gui.proto.Gui.SceneDesc;
import com.dynamo.gui.proto.Gui.SceneDesc.LayoutDesc;
import com.google.protobuf.TextFormat;

public class GuiTemplateBuilder  {
    private static Logger logger = LoggerFactory.getLogger(GuiTemplateBuilder.class);

    private static void nodeStatesToStateMap(HashMap<String, GuiNodeStateBuilder> stateMap, List<Node> nodes) {
        for(Node node : nodes) {
            GuiNode guiNode = (GuiNode) node;
            if(GuiNodeStateBuilder.getDefaultBuilders(guiNode.getStateBuilder()) != null) {
                GuiNodeStateBuilder.storeState(guiNode);
            }
            stateMap.put(guiNode.getId(), guiNode.getStateBuilder());
            nodeStatesToStateMap(stateMap, node.getChildren());
        }
    }

    private static void stateMapToNodeStates(HashMap<String, GuiNodeStateBuilder> stateMap, List<Node> nodes) {
        for(Node node : nodes) {
            GuiNode guiNode = (GuiNode) node;
            GuiNodeStateBuilder oldBuilder = stateMap.getOrDefault(guiNode.getId(), null);
            if(oldBuilder != null) {
                guiNode.setStateBuilder(oldBuilder);
            } else {
                guiNode.setStateBuilder(new GuiNodeStateBuilder());
            }
            stateMapToNodeStates(stateMap, node.getChildren());
        }
    }

    private static void createDefaultStates(String templateId, List<Node> nodes, HashMap<String, HashMap<String, NodeDesc>> nodeMap, ArrayList<String> layouts) {
        for(Node node : nodes) {
            GuiNode n = (GuiNode) node;
            HashMap<String, NodeDesc.Builder> stateMap = new HashMap<String, NodeDesc.Builder>();
            HashMap<String, NodeDesc> layoutNodeMap = nodeMap.get("");
            NodeDesc defaultDesc = layoutNodeMap.get( n.getId() );
            if(defaultDesc != null) {
                stateMap.put(GuiNodeStateBuilder.getDefaultStateId(),  defaultDesc.toBuilder());
            }
            for(String layout : layouts) {
                layoutNodeMap = nodeMap.get(layout);
                NodeDesc desc = layoutNodeMap.get( n.getId() );
                if(desc != null) {
                    stateMap.put(layout, desc.toBuilder());
                }
            }
            GuiNodeStateBuilder.setDefaultBuilders(n.getStateBuilder(), stateMap);
            createDefaultStates(templateId, node.getChildren(), nodeMap, layouts);
        }
    }

    private static TemplateNode ValidateTemplateRecursion(TemplateNode root, List<Node> nodes) {
        for(Node node : nodes) {
            TemplateNode templateNode;
            if(node instanceof TemplateNode) {
                templateNode = (TemplateNode) node;
                String template = templateNode.getTemplate();
                if(template.equals(root.getTemplate())) {
                    return templateNode;
                }
            }
            templateNode = ValidateTemplateRecursion(root, node.getChildren());
            if(templateNode != null) {
                return templateNode;
            }
        }
        return null;
    }

    private static boolean loadScene(TemplateNode node, SceneDesc.Builder sceneBuilder) {
        // load the actual gui scene node
        GuiSceneNode templateScene = GuiSceneLoader.load(sceneBuilder);

        // check we aren't attempting to load this scene as a sub-scene  (infinite recursion)
        TemplateNode recursiveNode = ValidateTemplateRecursion(node, templateScene.getNodesNode().getChildren());
        if(recursiveNode != null) {
            logger.error("Template scene recursion: Node '" + node.getId() + "' with template '" + node.getTemplatePath() + "' including node '" + recursiveNode.getId() + "' with template '" + recursiveNode.getTemplatePath() + "'");
            node.clearChildren();
            node.setTemplatePath("");
            return false;
        }
        node.setTemplateScene(templateScene);

        // setup template child attributes. re-parent and sort children from loaded scene to this node
        node.setParentTemplateRecursive(templateScene.getNodesNode().getChildren());
        node.refactorChildHierarchyIds(templateScene.getNodesNode().getChildren(), node.getId(), 0);
        List<Node> templateChildren = new ArrayList<Node>(templateScene.getNodesNode().getChildren());
        for(Node n : templateChildren) {
            templateScene.getNodesNode().removeChild(n);
        }
        for(Node n : templateChildren) {
            node.addChild(n);
        }
        node.sortChildren();

        return true;
    }

    public static boolean build(TemplateNode node) {
        // store current states to be able to restore overridden states
        HashMap<String, GuiNodeStateBuilder> nodeStates = new HashMap<String, GuiNodeStateBuilder>();
        nodeStatesToStateMap(nodeStates, node.getChildren());

        // reset template if empty template property
        if(node.getTemplatePath().isEmpty()) {
            node.clearChildren();
            node.setTemplateScene(null);
            return true;
        }
        // no model, no loading
        if(node.getModel() == null) {
            node.clearChildren();
            node.setTemplateScene(null);
            return false;
        }

        // read scene into builder
        HashMap<String, SceneDesc.Builder> sceneResourceCache = new HashMap<String, SceneDesc.Builder>(32);
        SceneDesc.Builder sceneBuilder = null;
        try {
            SceneBuilderIO sceneBuilderIO = new SceneBuilderIO(node.getModel());
            sceneBuilder = sceneBuilderIO.readScene(node.getTemplate(), sceneResourceCache);
            sceneBuilder = GuiBuilder.transformScene(null, node.getTemplate(), sceneBuilder, sceneBuilderIO, sceneResourceCache, false);
        } catch (CompileExceptionError e) {
            logger.error("Failed creating resource: " + node.getTemplate(), e);
            return false;
        } catch (IOException e) {
            logger.error("Failed reading resource: " + node.getTemplate(), e);
            return false;
        }

        // reset old template
        node.clearChildren();
        node.setTemplateScene(null);
        
        // load the scene from builder
        if(!loadScene(node, sceneBuilder)) {
            return false;
        }

        // restore overridden states
        stateMapToNodeStates(nodeStates, node.getChildren());

        // apply default states
        if(sceneBuilder != null) {
            HashMap<String, HashMap<String, NodeDesc>> nodeMap = GuiBuilder.createNodeMap(node.getId() + "/", sceneBuilder);
            ArrayList<String> layouts = new ArrayList<String>(sceneBuilder.getLayoutsCount());
            for(LayoutDesc layout : sceneBuilder.getLayoutsList()) {
                layouts.add(layout.getName());
            }
            createDefaultStates(node.getId() + "/", node.getChildren(), nodeMap, layouts);
        }

        // restore to current scene node props
        GuiNodeStateBuilder.restoreStates(node.getChildren());
        return true;
    }

    private static class SceneBuilderIO implements GuiBuilder.ISceneBuilderIO {
        ISceneModel model;
        SceneBuilderIO(ISceneModel model) {
            this.model = model;
        }

        public SceneDesc.Builder readScene(String resourcePath, HashMap<String, SceneDesc.Builder> resourceCache) throws IOException {
            SceneDesc.Builder sceneBuilder = resourceCache.getOrDefault(resourcePath, null);
            if(sceneBuilder == null) {
                IFile file = model.getFile(resourcePath);
                InputStream stream = null;
                if(!file.exists()) {
                    throw new IOException(BobNLS.bind(Messages.BuilderUtil_MISSING_RESOURCE, "template", resourcePath));
                }
                try {
                stream = file.getContents();
                } catch (CoreException e) {
                    logger.error("Failed reading resource: " + resourcePath, e);
                    return null;
                }
                InputStreamReader reader = new InputStreamReader(stream);
                sceneBuilder = SceneDesc.newBuilder();
                TextFormat.merge(reader, sceneBuilder);
                resourceCache.put(resourcePath, sceneBuilder);
            }
            return sceneBuilder.clone();
        }
    }

}


