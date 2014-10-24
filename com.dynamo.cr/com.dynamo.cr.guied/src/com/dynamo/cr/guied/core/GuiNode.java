package com.dynamo.cr.guied.core;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.swt.graphics.RGB;

import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.properties.Range;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.validators.Unique;
import com.dynamo.gui.proto.Gui.NodeDesc.AdjustMode;
import com.dynamo.gui.proto.Gui.NodeDesc.BlendMode;
import com.dynamo.gui.proto.Gui.NodeDesc.Pivot;
import com.dynamo.gui.proto.Gui.NodeDesc.XAnchor;
import com.dynamo.gui.proto.Gui.NodeDesc.YAnchor;

@SuppressWarnings("serial")
public class GuiNode extends Node {

    // Fields of the render key
    private static final int ORDER_SHIFT = 0;
    private static final int LAYER_SHIFT = ORDER_SHIFT + 8;

    @Property
    private Vector3d size = new Vector3d(0.0, 0.0, 0.0);

    @Property
    @NotEmpty(severity = IStatus.ERROR)
    @Unique(scope = GuiNode.class, base = NodesNode.class)
    private String id = "";

    @Property
    private RGB color = new RGB(255, 255, 255);

    @Property
    @Range(min = 0.0, max = 1.0)
    private double alpha = 1.0;

    @Property
    private boolean inheritAlpha = true;

    @Property
    private BlendMode blendMode = BlendMode.BLEND_MODE_ALPHA;

    @Property
    private Pivot pivot = Pivot.PIVOT_CENTER;

    @Property
    private XAnchor xanchor = XAnchor.XANCHOR_NONE;

    @Property
    private YAnchor yanchor = YAnchor.YANCHOR_NONE;

    @Property
    private AdjustMode adjustMode = AdjustMode.ADJUST_MODE_FIT;

    @Property(editorType = EditorType.DROP_DOWN)
    private String layer = "";

    @Property
    private Vector4d slice9 = new Vector4d(0,0,0,0);

    private transient int renderKey = 0;

    public GuiNode() {
        super();
        setTransformable(true);
        setFlags(Flags.COMPONENT_SCALABLE);
    }

    public String getId() {
        return this.id;
    }

    public void setId(String id) {
        this.id = id;
    }

    public Vector3d getSize() {
        return this.size;
    }

    public void setSize(Vector3d size) {
        this.size = size;
    }

    public RGB getColor() {
        return this.color;
    }

    public void setColor(RGB color) {
        this.color.red = color.red;
        this.color.green = color.green;
        this.color.blue = color.blue;
    }

    public double getAlpha() {
        return this.alpha;
    }

    public void setAlpha(double alpha) {
        this.alpha = alpha;
    }

    public BlendMode getBlendMode() {
        return this.blendMode;
    }

    public void setBlendMode(BlendMode blendMode) {
        this.blendMode = blendMode;
    }

    public Pivot getPivot() {
        return this.pivot;
    }

    public void setPivot(Pivot pivot) {
        this.pivot = pivot;
    }

    public Vector4d getSlice9()
    {
        return slice9;
    }

    public void setSlice9(Vector4d slice9)
    {
        this.slice9 = slice9;
    }

    public XAnchor getXanchor() {
        return this.xanchor;
    }

    public void setXanchor(XAnchor xAnchor) {
        this.xanchor = xAnchor;
    }

    public YAnchor getYanchor() {
        return this.yanchor;
    }

    public void setYanchor(YAnchor yAnchor) {
        this.yanchor = yAnchor;
    }

    public AdjustMode getAdjustMode() {
        return this.adjustMode;
    }

    public void setAdjustMode(AdjustMode adjustMode) {
        this.adjustMode = adjustMode;
    }

    public String getLayer() {
        return this.layer;
    }

    public void setLayer(String layer) {
        this.layer = layer;
    }

    public boolean isInheritAlpha() {
        return this.inheritAlpha;
    }

    public void setInheritAlpha(boolean inheritAlpha) {
        this.inheritAlpha = inheritAlpha;
    }

    public Object[] getLayerOptions() {
        List<Node> layerNodes = getScene().getLayersNode().getChildren();
        List<String> layers = new ArrayList<String>(layerNodes.size());
        for (Node n : layerNodes) {
            layers.add(((LayerNode) n).getId());
        }
        return layers.toArray();
    }

    protected GuiSceneNode getScene() {
        Node parent = getParent();
        while (parent.getParent() != null) {
            parent = parent.getParent();
            if (parent instanceof GuiSceneNode) {
                return (GuiSceneNode) parent;
            }
        }
        return null;
    }

    public int getRenderKey() {
        return this.renderKey;
    }

    public int getLayerIndex(Map<String, Integer> layersToIndexMap) {
        if (!this.layer.isEmpty()) {
            return layersToIndexMap.containsKey(this.layer) ? layersToIndexMap.get(this.layer) : 0;
        } else if (getParent() != null && getParent() instanceof GuiNode) {
            return ((GuiNode) getParent()).getLayerIndex(layersToIndexMap);
        } else {
            return 0;
        }
    }

    public void setRenderKey(int order, Map<String, Integer> layersToIndexMap) {
        int layer = getLayerIndex(layersToIndexMap);
        this.renderKey = (order << ORDER_SHIFT) | (layer << LAYER_SHIFT);
    }

    @Override
    public String toString() {
        return this.id;
    }

    public float[] calcNormRGBA() {
        float[] rgba = new float[] {1.0f, 1.0f, 1.0f, 1.0f};
        GuiNode node = this;
        float inv = 1.0f / 255.0f;
        RGB rgb = node.getColor();
        rgba[0] *= rgb.red * inv;
        rgba[1] *= rgb.green * inv;
        rgba[2] *= rgb.blue * inv;
        rgba[3] *= (float) node.getAlpha();

        while (node != null) {
            Node parent = node.getParent();
            if (node.isInheritAlpha() && parent != null && parent instanceof GuiNode) {
                node = (GuiNode)parent;
                rgba[3] *= (float) node.getAlpha();
            } else {
                node = null;
            }
        }
        return rgba;
    }
}
