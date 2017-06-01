package com.dynamo.cr.guied.core;

import java.io.InputStream;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.apache.commons.io.IOUtils;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.Path;
import org.eclipse.swt.graphics.RGB;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.core.ProjectProperties;
import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.guied.util.Layouts;
import com.dynamo.cr.guied.util.GuiNodeStateBuilder;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Range;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.gui.proto.Gui.SceneDesc.AdjustReference;

@SuppressWarnings("serial")
public class GuiSceneNode extends ComponentTypeNode {

    private static Logger logger = LoggerFactory.getLogger(GuiSceneNode.class);

    @Property(editorType = EditorType.RESOURCE, extensions = { "gui_script" })
    private String script;

    @Property(editorType = EditorType.RESOURCE, extensions = { "material" })
    private String material;

    @Property(editorType = EditorType.DROP_DOWN, displayName = "Adjust Mode")
    private AdjustReference adjustReference = AdjustReference.ADJUST_REFERENCE_PARENT;

    @Property(displayName = "Max Nodes")
    @Range(min = 1, max = 1024)
    private int maxNodes = 512;

    private RGB backgroundColor = new RGB(0, 0, 0);

    private LabelNode nodesNode;
    private LabelNode texturesNode;
    private LabelNode fontsNode;
    private LabelNode spineScenesNode;
    private LabelNode particlefxsNode;
    private LabelNode layersNode;
    private LabelNode layoutsNode;
    // Fallback to something sensible if game.project doesn't exists
    // See comment about tests
    private float width = 960;
    private float height = 640;
    private float defaultWidth = 960;
    private float defaultHeight = 640;
    private String displayProfilesPath;
    private Layouts.Layout currentLayout;
    private Layouts.Layout previousLayout;

    public GuiSceneNode() {
        super();

        nodesNode = new NodesNode();
        texturesNode = new TexturesNode();
        fontsNode = new FontsNode();
        spineScenesNode = new SpineScenesNode();
        particlefxsNode = new ParticleFXScenesNode();
        layersNode = new LayersNode();
        layoutsNode = new LayoutsNode();
        addChild(nodesNode);
        addChild(texturesNode);
        addChild(fontsNode);
        addChild(spineScenesNode);
        addChild(particlefxsNode);
        addChild(layersNode);
        addChild(layoutsNode);
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

    public AdjustReference getAdjustReference() {
        return adjustReference;
    }

    public void setAdjustReference( AdjustReference adjustReference) {
        this.adjustReference = adjustReference;
    }

    public int getMaxNodes() {
        return this.maxNodes;
    }

    public void setMaxNodes(int maxNodes) {
        this.maxNodes = maxNodes;
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
    
    public Node getSpineScenesNode() {
        return spineScenesNode;
    }
    
    public Node getParticleFXScenesNode() {
        return particlefxsNode;
    }

    public Node getLayersNode() {
        return layersNode;
    }

    public Node getLayoutsNode() {
        return layoutsNode;
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

    public float getDefaultWidth() {
        return this.defaultWidth;
    }

    public float getDefaultHeight() {
        return this.defaultHeight;
    }

    public void setDimensions(float width, float height) {
        this.width = width;
        this.height = height;
        AABB aabb = new AABB();
        aabb.union(0.0, 0.0, 0.0);
        aabb.union(width, height, 0.0);
        setAABB(aabb);
    }

    public Layouts.Layout getDefaultLayout() {
        return Layouts.getDefaultLayout();
    }

    public void setDefaultLayout() {
        setCurrentLayout(Layouts.getDefaultLayout().getId());
    }

    public boolean isCurrentLayoutDefault() {
        return this.currentLayout.getId().equals(getDefaultLayout().getId());
    }

    public Layouts.Layout getCurrentLayout() {
        return this.currentLayout;
    }

    public boolean setCurrentLayout(String id) {
        Layouts.Layout layout = Layouts.getLayout(id);
        if(layout == null) {
            return false;
        }
        if(this.currentLayout != null) {
            if(this.currentLayout.getId().equals(layout.getId())) {
                return true;
            }
            GuiNodeStateBuilder.storeStates(this.getNodesNode().getChildren());
        }
        this.previousLayout = this.currentLayout;
        this.currentLayout = layout;
        setDimensions(layout.getWidth(), layout.getHeight());
        GuiNodeStateBuilder.setStateId(id);
        GuiNodeStateBuilder.restoreStates(this.getNodesNode().getChildren());
        return true;
    }

    public String getPreviousLayout() {
        if(this.previousLayout == null)
            return null;
        return this.previousLayout.getId();
    }

    public void loadLayouts(IFile file) {
        try {
            InputStream i = file.getContents();
            Layouts.load(i, this.defaultWidth, this.defaultHeight);
            IOUtils.closeQuietly(i);
        } catch (Exception e) {
            logger.error("could not read display profiles", e);
            displayProfilesPath = null;
            return;
        }

        // apply new (matching) state to current and default
        this.previousLayout = null;
        if(this.currentLayout == null) {
            this.currentLayout = this.getDefaultLayout();
        } else {
            if(!this.getDefaultLayout().equals(this.currentLayout)) {
                setDefaultLayout();
            }
        }
    }

    public void loadProjectProperties(InputStream in) {
        ProjectProperties projectProperties = new ProjectProperties();
        try {
            projectProperties.load(in);
        } catch (Exception e) {
            logger.error("could not read game.project", e);
            return;
        }

        this.defaultWidth = this.width = (float) projectProperties.getIntValue("display", "width");
        this.defaultHeight = this.height = (float) projectProperties.getIntValue("display", "height");
        setDimensions(this.defaultWidth, this.defaultHeight);
        try {
            IProject project = EditorUtil.getProject();
            IPath dPPath = new Path(projectProperties.getStringValue("display", "display_profiles", "/builtins/render/default.display_profilesc")).removeFileExtension().addFileExtension("display_profiles");
            IFile dPFile = EditorUtil.getContentRoot(project).getFile(dPPath);
            displayProfilesPath = dPFile.getFullPath().toString();
            if (dPFile.exists()) {
                // in cr.integrationstest the root isn't /content and the
                // file doesn't exists. That's the reason we accept missing display profile file
                loadLayouts(dPFile);
            }
        } catch (Exception e) {
            logger.error("failed to load display profiles", e);
            displayProfilesPath = null;
            return;
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
        } else if(displayProfilesPath != null && file.getFullPath().toString().equals(displayProfilesPath)) {
            try {
                loadLayouts(file);
            } catch (Exception e) {
                logger.error("could not update display profiles used by project (referenced in game.project)", e);
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

    private static long calcRenderKey(Scope scope, int layer, int index) {
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
