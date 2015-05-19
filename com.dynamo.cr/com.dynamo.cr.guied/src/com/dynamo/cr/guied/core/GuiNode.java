package com.dynamo.cr.guied.core;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.swt.graphics.RGB;

import com.google.protobuf.Descriptors.EnumValueDescriptor;

import com.dynamo.cr.guied.util.GuiNodeStateBuilder;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.properties.Range;
import com.dynamo.cr.sceneed.core.Identifiable;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.util.LoaderUtil;
import com.dynamo.cr.sceneed.core.validators.Unique;
import com.dynamo.gui.proto.Gui.NodeDesc.AdjustMode;
import com.dynamo.gui.proto.Gui.NodeDesc.BlendMode;
import com.dynamo.gui.proto.Gui.NodeDesc.ClippingMode;
import com.dynamo.gui.proto.Gui.NodeDesc.Pivot;
import com.dynamo.gui.proto.Gui.NodeDesc.XAnchor;
import com.dynamo.gui.proto.Gui.NodeDesc.YAnchor;
import com.dynamo.proto.DdfMath.Vector4;

@SuppressWarnings("serial")
public class GuiNode extends Node implements Identifiable {

    // Fields of the render key
    private enum RenderKeyRange {
        SUB_INDEX(9),
        SUB_LAYER(3),
        INV_CLIPPER_ID(8),
        INDEX(9),
        LAYER(3);

        private final int bitRange;

        RenderKeyRange(int bitRange) {
            this.bitRange = bitRange;
        }

        private int mask() {
            return (1 << this.bitRange) - 1;
        }

        public int shift(int val) {
            int end = this.ordinal();
            int offset = 0;
            int result = val & mask();
            for (int i = 0; i < end; ++i) {
                RenderKeyRange range = values()[i];
                offset += range.bitRange;
            }
            result <<= offset;
            return result;
        }
    }

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

    private transient int renderKey = 0;
    protected GuiNodeStateBuilder nodeStates = new GuiNodeStateBuilder();

    public GuiNode() {
        super();
        setTransformable(true);
        setFlags(Flags.COMPONENT_SCALABLE);
    }

    @Override
    public void setTranslation(Point3d translation) {
        super.setTranslation(translation);
        GuiNodeStateBuilder.setField(this, "Position", LoaderUtil.toVector4(translation));
    }

    public void resetTranslation() {
        super.setTranslation(LoaderUtil.toPoint3d((Vector4)GuiNodeStateBuilder.resetField(this, "Position")));
    }

    public void resetPosition() {
        super.setTranslation(LoaderUtil.toPoint3d((Vector4)GuiNodeStateBuilder.resetField(this, "Position")));
    }

    public boolean isTranslationOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "Position", LoaderUtil.toVector4(translation));
    }

    @Override
    public void setRotation(Quat4d rotation) {
        super.setRotation(rotation);
        GuiNodeStateBuilder.setField(this, "Rotation", LoaderUtil.toVector4(this.getEuler()));
    }

    @Override
    public void setEuler(Vector3d euler) {
        super.setEuler(euler);
        GuiNodeStateBuilder.setField(this, "Rotation", LoaderUtil.toVector4(euler));
    }

    public void resetEuler() {
        super.setEuler(LoaderUtil.toVector3((Vector4)GuiNodeStateBuilder.resetField(this, "Rotation")));
    }

    public boolean isEulerOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "Rotation", LoaderUtil.toVector4(this.getEuler()));
    }

    @Override
    public void setComponentScale(Vector3d componentScale) {
        super.setComponentScale(componentScale);
        GuiNodeStateBuilder.setField(this, "Scale", LoaderUtil.toVector4(componentScale));
    }

    public void resetComponentScale() {
        super.setComponentScale(LoaderUtil.toVector3((Vector4)GuiNodeStateBuilder.resetField(this, "Scale")));
    }

    public boolean isComponentScaleOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "Scale", LoaderUtil.toVector4(this.getComponentScale()));
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
        GuiNodeStateBuilder.setField(this, "Size", LoaderUtil.toVector4(size));
    }

    public void resetSize() {
        this.size = LoaderUtil.toVector3((Vector4)GuiNodeStateBuilder.resetField(this, "Size"));
    }

    public boolean isSizeOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "Size", LoaderUtil.toVector4(this.size));
    }

    public RGB getColor() {
        return new RGB(color.red, color.green, color.blue);
    }

    public void setColor(RGB color) {
        this.color.red = color.red;
        this.color.green = color.green;
        this.color.blue = color.blue;
        GuiNodeStateBuilder.setField(this, "Color", LoaderUtil.toVector4(color, 0.0));
    }

    public void resetColor() {
        this.color = LoaderUtil.toRGB((Vector4)GuiNodeStateBuilder.resetField(this, "Color"));
    }

    public boolean isColorOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "Color", LoaderUtil.toVector4(this.color, 0.0));
    }

    public double getAlpha() {
        return this.alpha;
    }

    public void setAlpha(double alpha) {
        this.alpha = alpha;
        GuiNodeStateBuilder.setField(this, "Alpha", (float)alpha);
    }

    public void resetAlpha() {
        this.alpha = (Float)GuiNodeStateBuilder.resetField(this, "Alpha");
    }

    public boolean isAlphaOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "Alpha", (float)this.alpha);
    }

    public BlendMode getBlendMode() {
        return this.blendMode;
    }

    public void setBlendMode(BlendMode blendMode) {
        this.blendMode = blendMode;
        GuiNodeStateBuilder.setField(this, "BlendMode", blendMode);
    }

    public void resetBlendMode() {
        this.blendMode = BlendMode.valueOf((EnumValueDescriptor)GuiNodeStateBuilder.resetField(this, "BlendMode"));
    }

    public boolean isBlendModeOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "BlendMode", this.blendMode);
    }

    public Pivot getPivot() {
        return this.pivot;
    }

    public void setPivot(Pivot pivot) {
        this.pivot = pivot;
        GuiNodeStateBuilder.setField(this, "Pivot", pivot);
    }

    public void resetPivot() {
        this.pivot = Pivot.valueOf((EnumValueDescriptor)GuiNodeStateBuilder.resetField(this, "Pivot"));
    }

    public boolean isPivotOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "Pivot", this.pivot);
    }

    public XAnchor getXanchor() {
        return this.xanchor;
    }

    public void setXanchor(XAnchor xAnchor) {
        this.xanchor = xAnchor;
        GuiNodeStateBuilder.setField(this, "Xanchor", xAnchor);
    }

    public void resetXanchor() {
        this.xanchor = XAnchor.valueOf((EnumValueDescriptor) GuiNodeStateBuilder.resetField(this, "Xanchor"));
    }

    public boolean isXanchorOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "Xanchor", this.xanchor);
    }

    public YAnchor getYanchor() {
        return this.yanchor;
    }

    public void setYanchor(YAnchor yAnchor) {
        this.yanchor = yAnchor;
        GuiNodeStateBuilder.setField(this, "Yanchor", yAnchor);
    }

    public void resetYanchor() {
        this.yanchor = YAnchor.valueOf((EnumValueDescriptor)GuiNodeStateBuilder.resetField(this, "Yanchor"));
    }

    public boolean isYanchorOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "Yanchor", this.yanchor);
    }

    public AdjustMode getAdjustMode() {
        return this.adjustMode;
    }

    public void setAdjustMode(AdjustMode adjustMode) {
        this.adjustMode = adjustMode;
        GuiNodeStateBuilder.setField(this, "AdjustMode", adjustMode);
    }

    public void resetAdjustMode() {
        this.adjustMode = AdjustMode.valueOf((EnumValueDescriptor)GuiNodeStateBuilder.resetField(this, "AdjustMode"));
    }

    public boolean isAdjustModeOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "AdjustMode", this.adjustMode);
    }

    public String getLayer() {
        return this.layer;
    }

    public void setLayer(String layer) {
        this.layer = layer;
        GuiNodeStateBuilder.setField(this, "Layer", layer);
    }

    public void resetLayer() {
        this.layer = (String)GuiNodeStateBuilder.resetField(this, "Layer");
    }

    public boolean isLayerOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "Layer", this.layer);
    }

    public boolean isInheritAlpha() {
        return this.inheritAlpha;
    }

    public boolean getInheritAlpha() {
        return this.inheritAlpha;
    }

    public void setInheritAlpha(boolean inheritAlpha) {
        this.inheritAlpha = inheritAlpha;
        GuiNodeStateBuilder.setField(this, "InheritAlpha", inheritAlpha);
    }

    public void resetInheritAlpha() {
        this.inheritAlpha = (Boolean)GuiNodeStateBuilder.resetField(this, "InheritAlpha");
    }

    public boolean isInheritAlphaOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "InheritAlpha", this.inheritAlpha);
    }

    public void setClippingMode(ClippingMode mode) {
    }

    public ClippingMode getClippingMode() {
        return ClippingMode.CLIPPING_MODE_NONE;
    }

    public void setClippingVisible(boolean show) {
    }

    public boolean getClippingVisible() {
        return true;
    }

    public void setClippingInverted(boolean inverted) {
    }

    public boolean getClippingInverted() {
        return false;
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

    protected static int calcRenderKey(int layer, int index, int invClipperId, int subLayer, int subIndex) {
        return RenderKeyRange.LAYER.shift(layer)
                | RenderKeyRange.INDEX.shift(index)
                | RenderKeyRange.INV_CLIPPER_ID.shift(invClipperId)
                | RenderKeyRange.SUB_LAYER.shift(subLayer)
                | RenderKeyRange.SUB_INDEX.shift(subIndex);
    }

    public void setRenderKey(int layer, int index, int invClipperId, int subLayer, int subIndex) {
        this.renderKey = calcRenderKey(layer, index, invClipperId, subLayer, subIndex);
    }

    public void setRenderKey(int renderKey) {
        this.renderKey = renderKey;
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

    public ClippingNode getClosestParentClippingNode() {
        Node parent = getParent();
        while (parent != null) {
            if (parent instanceof ClippingNode) {
                ClippingNode clipper = (ClippingNode)parent;
                if (clipper.isClipping()) {
                    return clipper;
                }
            }
            parent = parent.getParent();
        }
        return null;
    }

    public GuiNodeStateBuilder getStateBuilder() {
        return this.nodeStates;
    }

    public void setStateBuilder(GuiNodeStateBuilder nodeStates) {
        this.nodeStates = nodeStates;
    }

    private void writeObject(ObjectOutputStream out) throws IOException {
        GuiNodeStateBuilder.storeState(this);
        out.defaultWriteObject();
    }

    private void readObject(ObjectInputStream in) throws IOException, ClassNotFoundException {
        in.defaultReadObject();
    }

}

