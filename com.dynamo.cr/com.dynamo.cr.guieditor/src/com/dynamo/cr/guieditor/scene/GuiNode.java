package com.dynamo.cr.guieditor.scene;

import java.awt.geom.Rectangle2D;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.vecmath.Matrix4d;
import javax.vecmath.Quat4d;
import javax.vecmath.Tuple3d;
import javax.vecmath.Vector3d;

import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.swt.graphics.RGB;

import com.dynamo.cr.guieditor.Activator;
import com.dynamo.cr.guieditor.DrawContext;
import com.dynamo.cr.properties.Entity;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.properties.PropertyIntrospector;
import com.dynamo.cr.properties.PropertyIntrospectorModel;
import com.dynamo.gui.proto.Gui.NodeDesc;
import com.dynamo.gui.proto.Gui.NodeDesc.AdjustMode;
import com.dynamo.gui.proto.Gui.NodeDesc.BlendMode;
import com.dynamo.gui.proto.Gui.NodeDesc.Pivot;
import com.dynamo.gui.proto.Gui.NodeDesc.XAnchor;
import com.dynamo.gui.proto.Gui.NodeDesc.YAnchor;
import com.dynamo.proto.DdfMath.Vector4;

@Entity(commandFactory = UndoableCommandFactory.class, accessor = GuiBeanPropertyAccessor.class)
public abstract class GuiNode implements IAdaptable {

    protected GuiScene scene;
    protected NodeDesc nodeDesc;

    @Property
    protected Vector3d position;

    @Property
    protected Vector3d rotation;

    @Property
    protected Vector3d scale;

    @Property
    protected Vector3d size;

    @Property()
    protected RGB color;

    @Property
    private double alpha;

    @Property(editorType = EditorType.DROP_DOWN)
    private String texture;

    @Property()
    private BlendMode blendMode;

    @Property()
    private String id;

    @Property()
    private Pivot pivot;

    @Property()
    private XAnchor xanchor;

    @Property()
    private YAnchor yanchor;

    @Property()
    private AdjustMode adjustMode;

    protected Map<String, IStatus> statusMap = new HashMap<String, IStatus>();

    private static Map<Class<? extends GuiNode>, PropertyIntrospector<GuiNode, GuiScene>> introspectors =
            new HashMap<Class<? extends GuiNode>, PropertyIntrospector<GuiNode, GuiScene>>();

    protected void verify() {
        statusMap.clear();
        if (alpha < 0 || alpha > 1.0) {
            IStatus status = new Status(IStatus.ERROR, Activator.PLUGIN_ID, "alpha value must between 0 and 1");
            statusMap.put("alpha", status);
        }
    }

    public Vector3d getRotation() {
        return new Vector3d(rotation);
    }

    public void setRotation(Vector3d rotation) {
        this.rotation.set(rotation);
    }

    public Vector3d getScale() {
        return new Vector3d(scale);
    }

    public void setScale(Vector3d scale) {
        this.scale.set(scale);
    }

    public Vector3d getSize() {
        return new Vector3d(size);
    }

    public void setSize(Vector3d size) {
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
        verify();
    }

    public String getTexture() {
        return texture;
    }

    public void setTexture(String texture) {
        this.texture = texture;
    }

    public Object[] getTextureOptions() {
        List<String> textures = new ArrayList<String>(16);
        for (EditorTextureDesc t : this.scene.getTextures()) {
            textures.add(t.getName());
        }
        return textures.toArray();
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

    public Pivot getPivot() {
        return pivot;
    }

    public void setPivot(Pivot pivot) {
        this.pivot = pivot;
    }

    public XAnchor getXanchor() {
        return xanchor;
    }

    public void setXanchor(XAnchor xanchor) {
        this.xanchor = xanchor;
    }

    public YAnchor getYanchor() {
        return yanchor;
    }

    public void setYanchor(YAnchor yanchor) {
        this.yanchor = yanchor;
    }

    public AdjustMode getAdjustMode() {
        return adjustMode;
    }

    public void setAdjustMode(AdjustMode adjustMode) {
        this.adjustMode = adjustMode;
    }

    public void setScene(GuiScene scene) {
        this.scene = scene;
    }

    private Vector3d toVector3d(Vector4 vector) {
        Vector3d ret = new Vector3d();
        ret.setX(vector.getX());
        ret.setY(vector.getY());
        ret.setZ(vector.getZ());
        return ret;
    }

    public GuiNode(GuiScene scene, NodeDesc nodeDesc) {
        this.scene = scene;
        this.nodeDesc = nodeDesc;
        this.position = toVector3d(nodeDesc.getPosition());
        this.rotation = toVector3d(nodeDesc.getRotation());
        this.scale = toVector3d(nodeDesc.getScale());
        this.size = toVector3d(nodeDesc.getSize());
        this.color = new RGB((int) (nodeDesc.getColor().getX() * 255),
                (int) (nodeDesc.getColor().getY() * 255),
                (int) (nodeDesc.getColor().getZ() * 255));
        this.alpha = nodeDesc.getColor().getW();
        this.texture = nodeDesc.getTexture();
        this.blendMode = nodeDesc.getBlendMode();
        this.id = nodeDesc.getId();
        this.pivot = nodeDesc.getPivot();
        this.xanchor = nodeDesc.getXanchor();
        this.yanchor = nodeDesc.getYanchor();
        this.adjustMode = nodeDesc.getAdjustMode();
        verify();
    }

    public abstract Rectangle2D getVisualBounds();

    public abstract void draw(DrawContext context);
    public abstract void drawSelect(DrawContext context);

    public void translate(double dx, double dy) {
        position.x += dx;
        position.y += dy;
        scene.propertyChanged(this, "position");
    }

    @Override
    public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
        PropertyIntrospector<GuiNode, GuiScene> introspector = introspectors.get(this.getClass());

        if (introspector == null) {
            introspector = new PropertyIntrospector<GuiNode, GuiScene>(this.getClass());
            introspectors.put(this.getClass(), introspector);
        }

        if (adapter == IPropertyModel.class) {
            return new PropertyIntrospectorModel<GuiNode, GuiScene>(this, getScene(), introspector);
        }
        return null;
    }

    public GuiScene getScene() {
        return scene;
    }

    public Vector3d getPosition() {
        return new Vector3d(position);
    }

    public void setPosition(Vector3d newPosition) {
        position.setX(newPosition.getX());
        position.setY(newPosition.getY());
        position.setZ(newPosition.getZ());
    }

    Vector4.Builder buildVector3(Vector3d v) {
        return Vector4.newBuilder()
                .setX((float) v.x)
                .setY((float) v.y)
                .setZ((float) v.z)
                .setW(0);
    }

    public final NodeDesc buildNodeDesc() {
        Vector4 color4 = Vector4.newBuilder()
                .setX(color.red / 255.0f )
                .setY(color.green / 255.0f )
                .setZ(color.blue / 255.0f)
                .setW((float) alpha).build();
        NodeDesc.Builder builder = NodeDesc.newBuilder().mergeFrom(nodeDesc);
        builder.setPosition(buildVector3(position))
        .setRotation(buildVector3(rotation))
        .setScale(buildVector3(scale))
        .setSize(buildVector3(size))
        .setColor(color4)
        .setTexture(texture)
        .setBlendMode(blendMode)
        .setId(id)
        .setPivot(pivot)
        .setXanchor(xanchor)
        .setYanchor(yanchor)
        .setAdjustMode(adjustMode);
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

    public IStatus getStatus(String property) {
        return statusMap.get(property);
    }

    public void calculateWorldTransform(Matrix4d transform) {
        transform.setIdentity();
        transform.setTranslation(this.position);
        Quat4d rotation = new Quat4d();
        eulerToQuat(this.rotation, rotation);
        // Set scale, the rotation below will not corrupt/nullify the scale
        transform.setElement(0, 0, this.scale.getX());
        transform.setElement(1, 1, this.scale.getY());
        transform.setElement(2, 2, this.scale.getZ());
        transform.setRotation(rotation);
    }

    private static void eulerToQuat(Tuple3d euler, Quat4d quat) {
        double bank = euler.x * Math.PI / 180;
        double heading = euler.y * Math.PI / 180;
        double attitude = euler.z * Math.PI / 180;

        double c1 = Math.cos(heading/2);
        double s1 = Math.sin(heading/2);
        double c2 = Math.cos(attitude/2);
        double s2 = Math.sin(attitude/2);
        double c3 = Math.cos(bank/2);
        double s3 = Math.sin(bank/2);
        double c1c2 = c1*c2;
        double s1s2 = s1*s2;
        double w =c1c2*c3 - s1s2*s3;
        double x =c1c2*s3 + s1s2*c3;
        double y =s1*c2*c3 + c1*s2*s3;
        double z =c1*s2*c3 - s1*c2*s3;

        quat.x = x;
        quat.y = y;
        quat.z = z;
        quat.w = w;
        quat.normalize();
    }

}
