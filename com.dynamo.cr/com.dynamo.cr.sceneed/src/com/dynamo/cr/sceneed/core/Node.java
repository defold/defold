package com.dynamo.cr.sceneed.core;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.EnumSet;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.media.opengl.GL2;
import javax.vecmath.Matrix3d;
import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Tuple3d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.MultiStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.swt.SWT;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.properties.Entity;
import com.dynamo.cr.properties.GreaterThanZero;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.PropertyIntrospector;
import com.dynamo.cr.properties.PropertyIntrospectorModel;
import com.dynamo.cr.sceneed.Activator;

@SuppressWarnings("serial")
@Entity(commandFactory = SceneUndoableCommandFactory.class)
public abstract class Node implements IAdaptable, Serializable {

    public enum Flags {
        TRANSFORMABLE,
        SCALABLE,
 COMPONENT_SCALABLE, LOCKED,
        NO_INHERIT_ROTATION,
        NO_INHERIT_SCALE,
        INVISIBLE
    }

    private transient ISceneModel model;
    private transient List<Node> children = new ArrayList<Node>();
    private transient Node parent;
    private EnumSet<Flags> flags = EnumSet.noneOf(Flags.class);

    private AABB aabb = new AABB();
    private AABB worldAABB = new AABB();
    private boolean worldAABBDirty = true;

    @Property(displayName="position")
    protected Point3d translation = new Point3d(0, 0, 0);

    private Quat4d rotation = new Quat4d(0, 0, 0, 1);

    @Property(displayName="rotation")
    protected Vector3d euler = new Vector3d(0, 0, 0);

    @Property
    private Vector3d scale = new Vector3d(1.0, 1.0, 1.0);

    @Property(displayName = "scale")
    private Vector3d componentScale = new Vector3d(1.0, 1.0, 1.0);

    // Used to preserve order when adding/removing child nodes
    private transient int childIndex = -1;
    // Cached status
    private transient IStatus status = Status.OK_STATUS;

    private static Map<Class<? extends Node>, PropertyIntrospector<Node, ISceneModel>> introspectors =
            new HashMap<Class<? extends Node>, PropertyIntrospector<Node, ISceneModel>>();

    public void dispose(GL2 gl) {
        for (Node child : this.children) {
            child.dispose(gl);
        }
    }

    public Node() {
    }

    public boolean isVisible() {
        return !flags.contains(Flags.INVISIBLE);
    }

    public void setVisible(boolean visible) {
        if (visible) {
            flags.remove(Flags.INVISIBLE);
        } else {
            flags.add(Flags.INVISIBLE);
        }
    }

    protected void setAABBDirty() {
        Node p = this;
        while (p != null) {
            p.worldAABBDirty = true;
            p = p.getParent();
        }
    }

    protected void transformChanged() {
    }

    public boolean isFlagSet(Flags flag) {
        return flags.contains(flag);
    }

    public void setFlags(Flags flag) {
        flags.add(flag);
    }

    public void clearFlags(Flags flag) {
        flags.remove(flag);
    }

    public void setFlagsRecursively(Flags flag) {
        flags.add(flag);
        for (Node child : this.children) {
            child.setFlagsRecursively(flag);
        }
    }

    public void clearFlagsRecursively(Flags flag) {
        flags.remove(flag);
        for (Node child : this.children) {
            child.clearFlagsRecursively(flag);
        }
    }

    public final boolean isTransformable() {
        return flags.contains(Flags.TRANSFORMABLE);
    }

    public boolean isTranslationVisible() {
        return isTransformable();
    }

    public boolean isEulerVisible() {
        return isTransformable();
    }

    public final boolean isScalable() {
        return flags.contains(Flags.SCALABLE);
    }

    public boolean isScaleVisible() {
        return isScalable();
    }

    public final boolean isComponentScalable() {
        return flags.contains(Flags.COMPONENT_SCALABLE);
    }

    public boolean isComponentScaleVisible() {
        return isComponentScalable();
    }

    public final boolean isEditable() {
        return !flags.contains(Flags.LOCKED);
    }

    public void getAABB(AABB aabb) {
        aabb.set(this.aabb);
    }

    public final void setAABB(AABB aabb) {
        this.aabb.set(aabb);
        setAABBDirty();
    }

    private static void getAABBRecursively(AABB aabb, Node node)
    {
        AABB tmp = new AABB();
        Matrix4d t = new Matrix4d();

        node.getAABB(tmp);
        node.getWorldTransform(t);

        tmp.transform(t);
        aabb.union(tmp);

        for (Node n : node.getChildren()) {
            getAABBRecursively(aabb, n);
        }
    }

    public void getWorldAABB(AABB aabb) {
        if (this.worldAABBDirty) {
            this.worldAABB.setIdentity();
            getAABBRecursively(this.worldAABB, this);
            this.worldAABBDirty = false;
        }
        aabb.set(this.worldAABB);
    }

    protected final void setTransformable(boolean transformable) {
        if (transformable)
            flags.add(Flags.TRANSFORMABLE);
        else
            flags.remove(Flags.TRANSFORMABLE);
    }

    public Node(Vector4d translation, Quat4d rotation) {
        setTranslation(new Point3d(translation.getX(), translation.getY(), translation.getZ()));
        setRotation(rotation);
    }

    public boolean isOverridable() {
        return false;
    }

    public void applyTranslation(Point3d translation) {
        this.translation.set(translation);
        setAABBDirty();
        transformChanged();
    }

    public void setTranslation(Point3d translation) {
        this.applyTranslation(translation);
    }

    public Point3d getTranslation() {
        return new Point3d(this.translation);
    }

    public void applyRotation(Quat4d rotation) {
        this.rotation.set(rotation);
        this.rotation.normalize();
        quatToEuler(this.rotation, euler);
        setAABBDirty();
        transformChanged();
    }

    public void setRotation(Quat4d rotation) {
        this.applyRotation(rotation);
    }

    public Quat4d getRotation() {
        return new Quat4d(rotation);
    }

    public void setEuler(Vector3d euler) {
        this.euler = new Vector3d(euler);
        eulerToQuat(euler, rotation);
        setAABBDirty();
        transformChanged();
    }

    public Vector3d getEuler() {
        return new Vector3d(euler);
    }

    public void setScale(Vector3d scale) {
        if (scale.x > 0)
            this.scale.x = scale.x;
        if (scale.y > 0)
            this.scale.y = scale.y;
        if (scale.z >= 0)
            this.scale.z = scale.z;
        setAABBDirty();
        transformChanged();
    }

    public Vector3d getScale() {
        return this.scale;
    }

    public Vector3d getComponentScale() {
        return this.componentScale;
    }

    public void setComponentScale(Vector3d componentScale) {
        this.componentScale = componentScale;
        setAABBDirty();
        transformChanged();
    }

    public static void eulerToQuat(Tuple3d euler, Quat4d quat) {
        // Implementation based on:
        // http://ntrs.nasa.gov/archive/nasa/casi.ntrs.nasa.gov/19770024290.pdf
        // Rotation sequence: 231 (YZX)
        double t1 = euler.y * Math.PI / 180;
        double t2 = euler.z * Math.PI / 180;
        double t3 = euler.x * Math.PI / 180;

        double c1 = Math.cos(t1/2);
        double s1 = Math.sin(t1/2);
        double c2 = Math.cos(t2/2);
        double s2 = Math.sin(t2/2);
        double c3 = Math.cos(t3/2);
        double s3 = Math.sin(t3/2);
        double c1_c2 = c1*c2;
        double s2_s3 = s2*s3;

        quat.w = -s1*s2_s3 + c1_c2*c3;
        quat.x =  s1*s2*c3 + s3*c1_c2;
        quat.y =  s1*c2*c3 + s2_s3*c1;
        quat.z = -s1*s3*c2 + s2*c1*c3;

        quat.normalize();
    }

    public static void quatToEuler(Quat4d quat, Tuple3d euler) {
        double heading;
        double attitude;
        double bank;

        double test = quat.x * quat.y + quat.z * quat.w;
        if (test > 0.499)
        { // singularity at north pole
            heading = 2 * Math.atan2(quat.x, quat.w);
            attitude = Math.PI / 2;
            bank = 0;
        }
        else if (test < -0.499)
        { // singularity at south pole
            heading = -2 * Math.atan2(quat.x, quat.w);
            attitude = -Math.PI / 2;
            bank = 0;
        }
        else
        {
            double sqx = quat.x * quat.x;
            double sqy = quat.y * quat.y;
            double sqz = quat.z * quat.z;
            heading = Math.atan2(2 * quat.y * quat.w - 2 * quat.x * quat.z, 1 - 2 * sqy - 2 * sqz);
            attitude = Math.asin(2 * test);
            bank = Math.atan2(2 * quat.x * quat.w - 2 * quat.y * quat.z, 1 - 2 * sqx - 2 * sqz);
        }

        euler.x = bank * 180 / Math.PI;
        euler.y = heading * 180 / Math.PI;
        euler.z = attitude * 180 / Math.PI;
    }

    public void getLocalTransform(Matrix4d transform)
    {
        transform.setIdentity();
        transform.set(new Vector3d(translation.x, translation.y, translation.z));
        Matrix3d rotationScale = new Matrix3d();
        rotationScale.set(rotation);
        Matrix3d scale = new Matrix3d();
        scale.setIdentity();
        scale.setElement(0, 0, this.scale.x * this.componentScale.getX());
        scale.setElement(1, 1, this.scale.y * this.componentScale.getY());
        scale.setElement(2, 2, this.scale.z * this.componentScale.getZ());
        rotationScale.mul(scale);
        transform.setRotationScale(rotationScale);
    }

    protected boolean scaleAlongZ() {
        if (this.parent != null) {
            return parent.scaleAlongZ();
        } else {
            return true;
        }
    }

    public void getWorldTransform(Matrix4d transform) {
        boolean noInheritRotation = flags.contains(Flags.NO_INHERIT_ROTATION);
        boolean noInheritScale = flags.contains(Flags.NO_INHERIT_SCALE);
        boolean noScaleAlongZ = !scaleAlongZ();
        Node n = getParent();
        if (n != null) {
            n.getWorldTransform(transform);
        } else {
            transform.setIdentity();
        }
        if (noInheritRotation || noInheritScale) {
            Matrix3d rotationScale = new Matrix3d();
            transform.getRotationScale(rotationScale);
            if (noInheritRotation && noInheritScale) {
                rotationScale.setIdentity();
            } else if (noInheritRotation) {
                double scale = rotationScale.getScale();
                rotationScale.setIdentity();
                rotationScale.mul(scale);
            } else {
                rotationScale.normalize();
            }
            transform.setRotationScale(rotationScale);
        }
        Matrix4d local = new Matrix4d();
        if (noScaleAlongZ) {
            Matrix3d rotationScale = new Matrix3d();
            transform.getRotationScale(rotationScale);
            Vector3d z = new Vector3d();
            rotationScale.getColumn(2, z);
            z.normalize();
            rotationScale.setColumn(2, z);
            Vector3d translation = new Vector3d(this.translation);
            rotationScale.transform(translation);
            local.setIdentity();
            Matrix3d localRotationScale = new Matrix3d();
            localRotationScale.set(rotation);
            Matrix3d scale = new Matrix3d();
            scale.setIdentity();
            scale.setElement(0, 0, this.scale.x * this.componentScale.getX());
            scale.setElement(1, 1, this.scale.y * this.componentScale.getY());
            scale.setElement(2, 2, this.scale.z * this.componentScale.getZ());
            localRotationScale.mul(scale);
            local.setRotationScale(localRotationScale);
            transform.mul(local);
            transform.setM03(transform.getM03() + translation.x);
            transform.setM13(transform.getM13() + translation.y);
            transform.setM23(transform.getM23() + translation.z);
        } else {
            getLocalTransform(local);
            transform.mul(local);
        }
    }

    public final ISceneModel getModel() {
        return this.model;
    }

    public void setModel(ISceneModel model) {
        if (this.model != model) {
            this.model = model;
            for (Node child : this.children) {
                child.setModel(model);
            }
        }
    }

    public final List<Node> getChildren() {
        return this.children;
    }

    public final boolean hasChildren() {
        return !this.children.isEmpty();
    }

    private void doAddChild(int index, Node child) {
        child.childIndex = index;
        children.add(index, child);
        child.setParent(this);
        childAdded(child);
        setAABBDirty();
    }

    public final void addChild(Node child) {
        if (child != null && !this.children.contains(child)) {
            int index = child.childIndex;
            if (index < 0 || index >= children.size()) {
                index = children.size();
            }
            doAddChild(index, child);
        }
    }

    public final void addChild(int index, Node child) {
        if (child != null && !this.children.contains(child)) {
            doAddChild(index, child);
        }
    }

    protected void childAdded(Node child) {

    }

    public final void removeChild(Node child) {
        if (child != null && this.children.contains(child)) {
            child.childIndex = this.children.indexOf(child);
            children.remove(child);
            child.setParent(null);
            childRemoved(child);
            setAABBDirty();
        }
    }

    protected void childRemoved(Node child) {

    }

    protected final void clearChildren() {
        List<Node> oldChildren = new ArrayList<Node>(this.children);
        this.children.clear();
        for (Node child : oldChildren) {
            childRemoved(child);
        }
    }

    protected final void sortChildren(Comparator<? super Node> comparator) {
        List<Node> sortedChildren = new ArrayList<Node>(this.children);
        Collections.sort(sortedChildren, comparator);
        if (!sortedChildren.equals(this.children)) {
            this.children = sortedChildren;
        }

        int i = 0;
        for (Node child : children) {
            child.childIndex = i++;
        }
    }

    public final Node getParent() {
        return this.parent;
    }

    private final void setParent(Node parent) {
        this.parent = parent;
        if (parent != null) {
            setModel(parent.getModel());
        } else {
            setModel(null);
        }
        parentSet();
    }

    public void parentSet() {

    }

    public Image getIcon() {
        if (this.model != null) {
            return this.model.getIcon(getClass());
        } else {
            return null;
        }
    }

    public int getLabelTextStyle() {
        return SWT.NORMAL;
    }

    public final IStatus getStatus() {
        return this.status;
    }

    /**
     * Recursively updates the status of this node and all of its children that are not locked
     * (a locked child can not be remedied anyway).
     */
    public final void updateStatus() {
        if (this.model == null)
            return;

        this.status = Status.OK_STATUS;

        if (!this.children.isEmpty()) {
            MultiStatus childStatuses = new MultiStatus(Activator.PLUGIN_ID, 0, null, null);
            for (Node child : this.children) {
                if (child.isEditable()) {
                    child.updateStatus();
                    childStatuses.merge(child.getStatus());
                }
            }
            if (!childStatuses.isOK()) {
                this.status = childStatuses;
            }
        }

        @SuppressWarnings("unchecked")
        IPropertyModel<? extends Node, ISceneModel> model = (IPropertyModel<? extends Node, ISceneModel>) getAdapter(IPropertyModel.class);
        IStatus ownStatus = model.getStatus();
        if (!ownStatus.isOK()) {
            MultiStatus multiStatus = new MultiStatus(Activator.PLUGIN_ID, 0, null, null);
            multiStatus.merge(ownStatus);
            if (!this.status.isOK()) {
                multiStatus.merge(this.status);
            }
            this.status = multiStatus;
        }

        IStatus nodeStatus = validateNode();
        if (!nodeStatus.isOK()) {
            MultiStatus multiStatus = new MultiStatus(Activator.PLUGIN_ID, 0, null, null);
            multiStatus.merge(nodeStatus);
            if (!this.status.isOK()) {
                multiStatus.merge(this.status);
            }
            this.status = multiStatus;
        }
    }

    protected IStatus validateNode() {
        return Status.OK_STATUS;
    }

    @Override
    public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
        PropertyIntrospector<Node, ISceneModel> introspector = introspectors.get(this.getClass());

        if (introspector == null) {
            introspector = new PropertyIntrospector<Node, ISceneModel>(this.getClass());
            introspectors.put(this.getClass(), introspector);
        }

        if (adapter == IPropertyModel.class) {
            return new PropertyIntrospectorModel<Node, ISceneModel>(this, getModel(), introspector);
        } else if (adapter == PropertyIntrospector.class) {
            return introspector;
        }
        return null;
    }

    /**
     * Override to handle reload
     * @param file
     * @return True if anything was reloaded
     */
    public boolean handleReload(IFile file, boolean childWasReloaded) {
        return false;
    }

    @Override
    public String toString() {
        if (this.model != null) {
            String typeName = this.model.getTypeName(getClass());
            if (typeName != null) {
                return typeName;
            }
        }
        return super.toString();
    }

    public static Vector3d get3dScaleFromMatrix(Matrix4d mtx) {
        Vector4d tmp = new Vector4d();
        Vector3d out = new Vector3d();
        mtx.getColumn(0, tmp);
        out.x = tmp.length();
        mtx.getColumn(1, tmp);
        out.y = tmp.length();
        mtx.getColumn(2, tmp);
        out.z = tmp.length();
        return out;
    }

    public void setLocalTransform(Matrix4d transform) {
        Vector3d translation = new Vector3d();
        transform.get(translation);
        this.translation.set(translation);
        this.scale = get3dScaleFromMatrix(transform);
        transform.get(this.rotation);
        quatToEuler(this.rotation, euler);
        setAABBDirty();
        transformChanged();
    }

    private void writeObject(ObjectOutputStream out) throws IOException {
        out.writeObject(this.children);
        out.writeObject(this.flags);
        out.writeObject(this.aabb);
        out.writeObject(this.worldAABB);
        out.writeBoolean(this.worldAABBDirty);
        out.writeObject(this.translation);
        out.writeObject(this.rotation);
        out.writeObject(this.euler);
        out.writeObject(this.scale);
        out.writeObject(this.componentScale);
    }

    @SuppressWarnings("unchecked")
    private void readObject(ObjectInputStream in) throws IOException, ClassNotFoundException {
        List<Node> children = (List<Node>)in.readObject();
        this.children = new ArrayList<Node>(children.size());
        for (Node child : children) {
            addChild(child);
        }
        this.status = Status.OK_STATUS;
        this.childIndex = -1;
        this.flags = (EnumSet<Flags>)in.readObject();
        this.aabb = (AABB)in.readObject();
        this.worldAABB = (AABB)in.readObject();
        this.worldAABBDirty = in.readBoolean();
        this.translation = (Point3d)in.readObject();
        this.rotation = (Quat4d)in.readObject();
        this.euler = (Vector3d)in.readObject();
        this.scale = (Vector3d)in.readObject();
        this.componentScale = (Vector3d) in.readObject();
    }
}
