package com.dynamo.cr.guied.core;

import java.io.InputStream;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.swt.graphics.RGB;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.editor.core.ProjectProperties;
import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.Node;

@SuppressWarnings("serial")
public class GuiSceneNode extends ComponentTypeNode {

    private static Logger logger = LoggerFactory.getLogger(GuiSceneNode.class);

    @Property(editorType = EditorType.RESOURCE, extensions = { "gui_script" })
    private String script;

    @Property(editorType = EditorType.RESOURCE, extensions = { "material" })
    private String material;

    private RGB backgroundColor = new RGB(0, 0, 0);

    private LabelNode nodesNode;
    private LabelNode texturesNode;
    private LabelNode fontsNode;
    private LabelNode layersNode;
    // Fallback to something sensible if game.project doesn't exists
    // See comment about tests
    private float width = 960;
    private float height = 640;

    public GuiSceneNode() {
        super();

        nodesNode = new NodesNode();
        texturesNode = new TexturesNode();
        fontsNode = new FontsNode();
        layersNode = new LayersNode();
        addChild(nodesNode);
        addChild(texturesNode);
        addChild(fontsNode);
        addChild(layersNode);
    }

    public String getScript() {
        return script;
    }

    public void setScript(String script) {
        this.script = script;
    }

    public String getMaterial() {
        return material;
    }

    public void setMaterial(String material) {
        this.material = material;
    }

    public RGB getBackgroundColor() {
        return this.backgroundColor;
    }

    public void setBackgroundColor(RGB backgroundColor) {
        this.backgroundColor = backgroundColor;
    }

    public Node getNodesNode() {
        return nodesNode;
    }

    public Node getTexturesNode() {
        return texturesNode;
    }

    public Node getFontsNode() {
        return fontsNode;
    }

    public Node getLayersNode() {
        return layersNode;
    }

    public Map<String, Integer> getLayerToIndexMap() {
        Map<String, Integer> result = new HashMap<String, Integer>();
        // layer indices start at 1 to account for the empty layer ""
        int index = 1;
        for (Node n : this.layersNode.getChildren()) {
            String id = ((LayerNode) n).getId();
            result.put(id, index);
            ++index;
        }
        return result;
    }

    public float getWidth() {
        return this.width;
    }

    public float getHeight() {
        return this.height;
    }

    public void setDimensions(float width, float height) {
        this.width = width;
        this.height = height;
        AABB aabb = new AABB();
        aabb.union(0.0, 0.0, 0.0);
        aabb.union(width, height, 0.0);
        setAABB(aabb);
    }

    public void loadProjectProperties(InputStream in) {
        ProjectProperties projectProperties = new ProjectProperties();
        try {
            projectProperties.load(in);
            float width = (float) projectProperties.getIntValue("display", "width");
            float height = (float) projectProperties.getIntValue("display", "height");
            setDimensions(width, height);
        } catch (Exception e) {
            logger.error("could not update gui scene dimensions from game.project", e);
        }
    }

    @Override
    public boolean handleReload(IFile file, boolean childWasReloaded) {
        if (file.getName().equals("game.project")) {
            try {
                loadProjectProperties(file.getContents());
            } catch (CoreException e) {
                logger.error("could not update gui scene dimensions from game.project", e);
            }
        }
        return super.handleReload(file, childWasReloaded);
    }

    @Override
    public void parentSet() {
        // Hack to hide gui in other views
        if (getParent() != null) {
            setFlagsRecursively(Flags.INVISIBLE);
        }
    }

    private static class Scope {
        int index;
        int rootLayer;
        int rootIndex;

        public Scope(int layer, int index) {
            this.index = 1;
            this.rootLayer = layer;
            this.rootIndex = index;
        }

        public void increment() {
            index = Math.min(255, index + 1);
        }
    }

    private static int calcRenderKey(Scope scope, int layer, int index) {
        if (scope != null) {
            return GuiNode.calcRenderKey(scope.rootLayer, scope.rootIndex, scope.index, layer, index);
        } else {
            return GuiNode.calcRenderKey(layer, index, 0, 0, 0);
        }
    }

    private static int updateRenderOrder(int offset, Scope scope, List<Node> nodes, Map<String, Integer> layersToIndexMap) {
        int index = offset;
        for (Node n : nodes) {
            GuiNode node = (GuiNode)n;
            int layer = node.getLayerIndex(layersToIndexMap);
            if (node instanceof ClippingNode) {
                ClippingNode clipper = (ClippingNode)node;
                if (clipper.isClipping()) {
                    boolean rootClipper = scope == null;
                    Scope currentScope = scope;
                    if (currentScope == null) {
                        currentScope = new Scope(0, index++);
                    } else {
                        currentScope.increment();
                    }
                    clipper.setClippingKey(calcRenderKey(currentScope, 0, 0));
                    node.setRenderKey(calcRenderKey(currentScope, layer, 1));
                    updateRenderOrder(2, currentScope, clipper.getChildren(), layersToIndexMap);
                    if (layer > 0) {
                        node.setRenderKey(calcRenderKey(currentScope, layer, 1));
                    }
                    if (!rootClipper) {
                        currentScope.increment();
                    }
                    continue;
                }
            }
            node.setRenderKey(calcRenderKey(scope, layer, index++));
            index = updateRenderOrder(index, scope, node.getChildren(), layersToIndexMap);
        }
        return index;
    }

    public void updateRenderOrder() {
        List<Node> children = this.nodesNode.getChildren();
        updateRenderOrder(0, null, children, getLayerToIndexMap());
    }
}
