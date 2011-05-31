package com.dynamo.cr.guieditor.scene;

import java.awt.geom.Rectangle2D;

import javax.vecmath.Vector4d;

import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.swt.graphics.RGB;
import org.eclipse.ui.views.properties.IPropertySource;

import com.dynamo.cr.guieditor.DrawContext;
import com.dynamo.cr.guieditor.property.Property;
import com.dynamo.cr.guieditor.property.PropertyIntrospectorSource;
import com.dynamo.cr.guieditor.property.Vector4dEmbeddedSource;
import com.dynamo.gui.proto.Gui.NodeDesc;
import com.dynamo.gui.proto.Gui.NodeDesc.BlendMode;
import com.dynamo.proto.DdfMath.Vector4;

public abstract class GuiNode implements IAdaptable {

    protected GuiScene scene;
    protected NodeDesc nodeDesc;

    @Property(commandFactory = UndoableCommandFactory.class, embeddedSource = Vector4dEmbeddedSource.class)
    protected Vector4d position;

    @Property(commandFactory = UndoableCommandFactory.class, embeddedSource = Vector4dEmbeddedSource.class)
    protected Vector4d rotation;

    @Property(commandFactory = UndoableCommandFactory.class, embeddedSource = Vector4dEmbeddedSource.class)
    protected Vector4d scale;

    @Property(commandFactory = UndoableCommandFactory.class, embeddedSource = Vector4dEmbeddedSource.class)
    protected Vector4d size;

    @Property(commandFactory = UndoableCommandFactory.class)
    protected RGB color;

    @Property(commandFactory = UndoableCommandFactory.class)
    private double alpha;

    @Property(commandFactory = UndoableCommandFactory.class)
    private String texture;

    @Property(commandFactory = UndoableCommandFactory.class)
    private BlendMode blendMode;

    @Property(commandFactory = UndoableCommandFactory.class)
    private String id;

    private PropertyIntrospectorSource<GuiNode, GuiScene> propertySource;

    public Vector4d getRotation() {
        return new Vector4d(rotation);
    }

    public void setRotation(Vector4d rotation) {
        this.rotation.set(rotation);
    }

    public Vector4d getScale() {
        return new Vector4d(scale);
    }

    public void setScale(Vector4d scale) {
        this.scale.set(scale);
    }

    public Vector4d getSize() {
        return new Vector4d(size);
    }

    public void setSize(Vector4d size) {
        this.size.set(size);
    }

    public RGB getColor() {
        return new RGB(color.red, color.green, color.blue);
    }

    public void setColor(RGB color) {
        this.color.red = color.red;
        this.color.green = color.green;
        this.color.blue = color.blue;
    }

    public double getAlpha() {
        return alpha;
    }

    public void setAlpha(double alpha) {
        this.alpha = alpha;
    }

    public String getTexture() {
        return texture;
    }

    public void setTexture(String texture) {
        this.texture = texture;
    }

    public BlendMode getBlendMode() {
        return blendMode;
    }

    public void setBlendMode(BlendMode blendMode) {
        this.blendMode = blendMode;
    }

    public String getId() {
        return id;
    }

    public void setId(String id) {
        this.id = id;
    }

    public void setScene(GuiScene scene) {
        this.scene = scene;
    }

    private Vector4d toVector4d(Vector4 vector) {
        Vector4d ret = new Vector4d();
        ret.setX(vector.getX());
        ret.setY(vector.getY());
        ret.setZ(vector.getZ());
        ret.setW(vector.getW());
        return ret;
    }

    public GuiNode(GuiScene scene, NodeDesc nodeDesc) {
        this.scene = scene;
        this.nodeDesc = nodeDesc;
        this.position = toVector4d(nodeDesc.getPosition());
        this.rotation = toVector4d(nodeDesc.getRotation());
        this.scale = toVector4d(nodeDesc.getScale());
        this.size = toVector4d(nodeDesc.getSize());
        this.color = new RGB((int) (nodeDesc.getColor().getX() * 255),
                             (int) (nodeDesc.getColor().getY() * 255),
                             (int) (nodeDesc.getColor().getZ() * 255));
        this.alpha = nodeDesc.getColor().getW();
        this.texture = nodeDesc.getTexture();
        this.blendMode = nodeDesc.getBlendMode();
        this.id = nodeDesc.getId();
    }

    public abstract Rectangle2D getBounds();

    public abstract void draw(DrawContext context);
    public abstract void drawSelect(DrawContext context);

    public void translate(double dx, double dy) {
        position.x += dx;
        position.y += dy;
        scene.propertyChanged(this, "position");
    }

    @SuppressWarnings({ "unchecked", "rawtypes" })
    @Override
    public Object getAdapter(Class adapter) {
        if (adapter == IPropertySource.class) {
            if (this.propertySource == null) {
                this.propertySource = new PropertyIntrospectorSource(this, getScene(), getScene().getContentRoot());
            }
            return this.propertySource;
        }
        return null;
    }

    public GuiScene getScene() {
        return scene;
    }

    public Vector4d getPosition() {
        return new Vector4d(position);
    }

    public void setPosition(Vector4d newPosition) {
        position.setX(newPosition.getX());
        position.setY(newPosition.getY());
        position.setZ(newPosition.getZ());
    }

    Vector4.Builder buildVector4(Vector4d v) {
        return Vector4.newBuilder()
            .setX((float) v.x)
            .setY((float) v.y)
            .setZ((float) v.z)
            .setW((float) v.w);
    }

    public final NodeDesc buildNodeDesc() {
        Vector4 color4 = Vector4.newBuilder()
            .setX(color.red / 255.0f )
            .setY(color.green / 255.0f )
            .setZ(color.blue / 255.0f)
            .setW((float) alpha).build();
        NodeDesc.Builder builder = NodeDesc.newBuilder().mergeFrom(nodeDesc);
        builder.setPosition(buildVector4(position));
        builder.setRotation(buildVector4(rotation));
        builder.setScale(buildVector4(scale));
        builder.setSize(buildVector4(size));
        builder.setColor(color4);
        builder.setTexture(texture);
        builder.setBlendMode(blendMode);
        builder.setId(id);
        doBuildNodeDesc(builder);
        return builder.build();
    }

    public abstract void doBuildNodeDesc(NodeDesc.Builder builder);

    @Override
    public final int hashCode() {
        // NOTE: Do not override this method. We have data-structures that rely on referential equivalence, eg nodeToIndex in GuiScene
        return super.hashCode();
    }

    @Override
    public final boolean equals(Object obj) {
        // NOTE: Do not override this method. We have data-structures that rely on referential equivalence, eg nodeToIndex in GuiScene
        return super.equals(obj);
    }


}
