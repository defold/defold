package com.dynamo.cr.scene.graph;

import java.util.ArrayList;
import java.util.List;

import javax.vecmath.Matrix4d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.ui.views.properties.IPropertySource;

import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.PropertyIntrospectorSource;
import com.dynamo.cr.properties.Quat4dEmbeddedSource;
import com.dynamo.cr.properties.Vector4dEmbeddedSource;
import com.dynamo.cr.scene.math.AABB;
import com.dynamo.cr.scene.math.Transform;

public abstract class Node implements IAdaptable
{
    public static final int FLAG_EDITABLE = (1 << 0);
    public static final int FLAG_CAN_HAVE_CHILDREN = (1 << 1);
    public static final int FLAG_TRANSFORMABLE = (1 << 2);
    public static final int FLAG_GHOST = (1 << 3);

    // In order of severity, most severe first
    public static final int ERROR_FLAG_DUPLICATE_ID = (1 << 0);
    public static final int ERROR_FLAG_RECURSION = (1 << 1);
    public static final int ERROR_FLAG_RESOURCE_ERROR = (1 << 2);
    public static final int ERROR_FLAG_CHILD_ERROR = (1 << 3);
    public static final int ERROR_FLAG_COUNT = 4;

    @Property(commandFactory = UndoableCommandFactory.class, embeddedSource = Vector4dEmbeddedSource.class)
    protected Vector4d localTranslation = new Vector4d();

    @Property(commandFactory = UndoableCommandFactory.class, embeddedSource = Quat4dEmbeddedSource.class)
    protected Quat4d localRotation = new Quat4d();

    protected Node m_Parent;
    protected Scene m_Scene;
    private int m_Flags = 0;
    protected AABB m_AABB = new AABB();
    protected AABB m_WorldAABB = new AABB();
    private boolean m_WorldAABBDirty = true;
    private int errorFlags;
    private String[] errorMessages;
    private String identifier;

    // Psuedo states
    @Property(commandFactory = UndoableCommandFactory.class, embeddedSource = Vector4dEmbeddedSource.class)
    private Vector4d worldTranslation = new Vector4d();

    private PropertyIntrospectorSource<Node, Scene> propertySource;

    public Node(String identifier, Scene scene, int flags)
    {
        m_AABB.setIdentity();
        this.identifier = identifier;
        localTranslation.set(0, 0, 0, 0);
        localRotation.set(0, 0, 0, 1);
        m_Parent = null;
        m_Scene = scene;
        m_Flags = flags;
        errorFlags = 0;
        errorMessages = new String[ERROR_FLAG_COUNT];
    }

    @SuppressWarnings({ "rawtypes" })
    @Override
    public Object getAdapter(Class adapter) {
        if (adapter == IPropertySource.class) {
            if (this.propertySource == null) {
                this.propertySource = new PropertyIntrospectorSource<Node, Scene>(this, getScene(), null);
            }
            return this.propertySource;
        }
        return null;
    }

    public Vector4d getWorldTranslation() {
        return new Vector4d(worldTranslation);
    }

    public void setWorldTranslation(Vector4d worldTranslation) {
        Transform t = new Transform();
        getWorldTransform(t);
        t.setTranslation(worldTranslation);
        NodeUtil.setWorldTransform(this, t);
    }

    public final String getIdentifier()
    {
        return this.identifier;
    }

    protected void childIdentifierChanged(Node node, String oldId) {

    }

    public final void setIdentifier(String key)
    {
        if (this.identifier == null || !this.identifier.equals(key)) {
            String oldId = this.identifier;
            this.identifier = key;
            if (m_Parent != null) {
                m_Parent.childIdentifierChanged(this, oldId);
            }
            m_Scene.nodeChanged(this);
        }
    }

    public boolean isChildIdentifierUsed(Node node, String id) {
        return false;
    }

    public String getUniqueChildIdentifier(Node child) {
        return child.getIdentifier();
    }

    public final int getFlags()
    {
        return m_Flags;
    }

    public final  void setFlags(int flags)
    {
        m_Flags = flags;
    }

    public static void orFlags(Node node, int flags) {
        node.setFlags(node.getFlags() | flags);
        for (Node n : node.getChildren()) {
            Node.orFlags(n, flags);
        }
    }

    public static void andFlags(Node node, int flags) {
        node.setFlags(node.getFlags() & flags);
        for (Node n : node.getChildren()) {
            Node.andFlags(n, flags);
        }
    }

    public final boolean hasError(int errorFlag) {
        return (errorFlag & this.errorFlags) != 0;
    }

    public final boolean isOk() {
        return this.errorFlags == 0;
    }

    private void checkChildrenErrors() {
        boolean childError = false;
        for (Node node : this.children) {
            if (!node.isOk()) {
                childError = true;
            }
        }
        if (childError) {
            setError(Node.ERROR_FLAG_CHILD_ERROR);
        } else {
            clearError(Node.ERROR_FLAG_CHILD_ERROR);
        }
    }

    public final void setError(int errorFlag) {
        setError(errorFlag, null);
    }

    public final void setError(int errorFlag, String errorMessage) {
        if (errorFlag != 0) {
            this.errorFlags |= errorFlag;
            if (this.m_Parent != null) {
                this.m_Parent.checkChildrenErrors();
            }
            int index = 0;
            while (errorFlag > 1) {
                errorFlag >>= 1;
                ++index;
            }
            this.errorMessages[index] = errorMessage;
        }
    }

    public final void clearError(int errorFlag) {
        if (errorFlag != 0) {
            this.errorFlags &= ~errorFlag;
            if (this.m_Parent != null) {
                this.m_Parent.checkChildrenErrors();
            }
            int index = 0;
            while (errorFlag > 1) {
                errorFlag >>= 1;
                ++index;
            }
            this.errorMessages[index] = null;
        }
    }

    private void updateWorldTranslationProperty()
    {
        Transform t = new Transform();
        getWorldTransform(t);
        t.getTranslation(worldTranslation);

        for (Node n : getChildren()) {
            n.updateWorldTranslationProperty();
        }
    }

    public Scene getScene()
    {
        return m_Scene;
    }

    public void setScene(Scene scene) {
        m_Scene = scene;
    }

    public Node getParent()
    {
        return m_Parent;
    }

    public boolean isChildOf(Node node) {
        Node n = this;
        while (n != null) {
            if (n == node)
                return true;
            n = n.getParent();
        }
        return false;
    }

    public void setParent(Node parent)
    {
        if (m_Parent == parent)
            return;

        setDirty();

        if (m_Parent != null) {
            m_Parent.removeNode(this);
        }
        m_Parent = parent;
        if (m_Parent != null) {
            m_Parent.addNode(this);
        }
        m_Scene.nodeReparented(this, parent);
    }

    private void setDirty() {
        Node n = this;
        while (n != null) {
            n.m_WorldAABBDirty = true;
            n = n.getParent();
        }
    }

    public final Node[] getChildren() {
        return children.toArray(new Node[children.size()]);
    }

    private final List<Node> children = new ArrayList<Node>();

    protected void nodeAdded(Node node) {

    }

    /*
     * Special hack-function for saving...
     */
    public void addNodeNoSetParent(Node node) {
        if (((m_Flags & FLAG_CAN_HAVE_CHILDREN) == 0) || !acceptsChild(node)) {
            throw new UnsupportedOperationException("addNode is not supported for this node: " + this);
        }
        assert (children.indexOf(node) == -1);
        children.add(node);
        m_Scene.nodeAdded(node);
        m_Scene.nodeChanged(this);
        nodeAdded(node);
    }

    public final void addNode(Node node) {
        if (!acceptsChild(node)) {
            throw new UnsupportedOperationException("addNode is not supported for this node: " + this);
        }
        assert (children.indexOf(node) == -1);
        children.add(node);
        node.m_Parent = this;
        checkChildrenErrors();
        nodeAdded(node);
        m_Scene.nodeAdded(node);
        m_Scene.nodeChanged(this);
    }

    protected void nodeRemoved(Node node) {

    }

    public final void removeNode(Node node)
    {
        if ((m_Flags & FLAG_CAN_HAVE_CHILDREN) == 0)
            throw new UnsupportedOperationException("removeNode is not supported for this node: " + this);
        children.remove(node);
        node.m_Parent = null;
        checkChildrenErrors();
        nodeRemoved(node);
        m_Scene.nodeRemoved(node);
        m_Scene.nodeChanged(this);
    }

    public final void clearNodes() {
        if ((m_Flags & FLAG_CAN_HAVE_CHILDREN) == 0)
            throw new UnsupportedOperationException("removeNode is not supported for this node: " + this);
        for (Node child : children) {
            child.m_Parent = null;
            nodeRemoved(child);
            m_Scene.nodeRemoved(child);
        }
        m_Scene.nodeChanged(this);
        children.clear();
        checkChildrenErrors();
    }
    protected abstract boolean verifyChild(Node child);

    public final boolean acceptsChild(Node child) {
        if ((m_Flags & FLAG_CAN_HAVE_CHILDREN) == 0) {
            return false;
        }
        Node parent = this;
        while (parent != null && parent != child) {
            parent = parent.getParent();
        }
        if (parent == child) {
            return false;
        } else {
            return verifyChild(child);
        }
    }


    public boolean contains(Node node)
    {
        if (this == node)
            return true;

        for (Node n : getChildren())
        {
            if (n.contains(node))
                return true;
        }
        return false;
    }

    public String getLabel() {
        return this.identifier;
    }

    public void getLocalTranslation(Vector4d translation)
    {
        translation.set(this.localTranslation);
    }

    // Bean accessor
    public Vector4d getLocalTranslation() {
        return new Vector4d(localTranslation);
    }

    public void setLocalTranslation(Vector4d translation)
    {
        this.localTranslation.set(translation);
        this.localTranslation.w = 0;
        setDirty();

        updateWorldTranslationProperty();
    }

    public void setLocalRotation(Quat4d rotation)
    {
        this.localRotation.set(rotation);
        this.localRotation.normalize();
        update();
        setDirty();

        updateWorldTranslationProperty();
    }

    public Quat4d getLocalRotation() {
        return new Quat4d(localRotation);
    }

    public void getLocalTransform(Matrix4d transform)
    {
        transform.setIdentity();
        transform.setColumn(3, localTranslation);
        transform.m33 = 1;
        transform.setRotation(localRotation);
    }

    public void getLocalTransform(Transform transform)
    {
        transform.setTranslation(localTranslation);
        transform.setRotation(localRotation);
    }

    private void update()
    {
        Transform t = new Transform();
        getWorldTransform(t);
        t.getTranslation(worldTranslation);
    }

    public void setLocalTransform(Matrix4d transform)
    {
        transform.getColumn(3, localTranslation);
        localTranslation.w = 0;
        localRotation.set(transform);
        //System.out.println(last_rotation + ", " + m_Rotation);
        localRotation.normalize();

        //System.out.println(transform);

        update();
        setDirty();
        updateWorldTranslationProperty();
    }

    public void setLocalTransform(Transform transform)
    {
        transform.getTranslation(localTranslation);
        transform.getRotation(localRotation);
        localRotation.normalize();

        update();
        setDirty();

        updateWorldTranslationProperty();
    }

    public void getWorldTransform(Matrix4d transform)
    {
        Matrix4d tmp = new Matrix4d();
        transform.setIdentity();
        Node n = this;
        while (n != null)
        {
            n.getLocalTransform(tmp);
            transform.mul(tmp, transform);
            n = n.getParent();
        }
    }

    public void getWorldTransform(Transform transform)
    {
        Transform tmp = new Transform();
        transform.setIdentity();
        Node n = this;
        while (n != null)
        {
            n.getLocalTransform(tmp);
            transform.mul(tmp, transform);
            n = n.getParent();
        }

    }

    public void transformLocal(Matrix4d transform)
    {
        Matrix4d local = new Matrix4d();
        getLocalTransform(local);

        local.mul(local, transform);
        setLocalTransform(local);
        // NOTE: setLocalTransform will send appropriate change event
    }

    public void transformLocal(Transform transform)
    {
        Transform local = new Transform();
        getLocalTransform(local);

        local.mul(local, transform);
        setLocalTransform(local);
        // NOTE: setLocalTransform will send appropriate change event
    }

    public void getLocalAABB(AABB aabb)
    {
        aabb.set(m_AABB);
    }

    private static void getAABBRecursively(AABB aabb, Node node)
    {
        AABB tmp = new AABB();
        Transform t = new Transform();

        node.getLocalAABB(tmp);
        node.getWorldTransform(t);

        tmp.transform(t);
        aabb.union(tmp);

        for (Node n : node.getChildren()) {
            getAABBRecursively(aabb, n);
        }
    }

    public void getWorldAABB(AABB aabb) {
        if (m_WorldAABBDirty) {
            m_WorldAABB.setIdentity();
            getAABBRecursively(m_WorldAABB, this);
            m_WorldAABBDirty = false;
        }
        aabb.set(m_WorldAABB);
    }

    public abstract void draw(DrawContext context);

    public String getToolTip() {
        for (int i = 0; i < ERROR_FLAG_COUNT; ++i) {
            if ((errorFlags & (1 << i)) != 0 && errorMessages[i] != null) {
                return errorMessages[i];
            }
        }
        if ((errorFlags & ERROR_FLAG_DUPLICATE_ID) != 0) {
            return "The id is already used in this collection.";
        } else if ((errorFlags & ERROR_FLAG_RESOURCE_ERROR) != 0) {
                return "A resource could not be loaded.";
        } else if ((errorFlags & ERROR_FLAG_CHILD_ERROR) != 0) {
            return "An item below this item contains an error.";
        } else {
            return null;
        }
    }
}
