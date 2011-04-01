package com.dynamo.cr.scene.graph;

import java.util.ArrayList;
import java.util.List;

import javax.vecmath.Matrix4d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import com.dynamo.cr.scene.math.AABB;
import com.dynamo.cr.scene.math.MathUtil;
import com.dynamo.cr.scene.math.Transform;

public abstract class Node
{
    public static final int FLAG_EDITABLE = (1 << 0);
    public static final int FLAG_CAN_HAVE_CHILDREN = (1 << 1);
    public static final int FLAG_TRANSFORMABLE = (1 << 2);

    // In order of severity, most severe first
    public static final int ERROR_FLAG_DUPLICATE_ID = (1 << 0);
    public static final int ERROR_FLAG_RECURSION = (1 << 1);
    public static final int ERROR_FLAG_RESOURCE_ERROR = (1 << 2);
    public static final int ERROR_FLAG_CHILD_ERROR = (1 << 3);
    public static final int ERROR_FLAG_COUNT = 4;

    protected Vector4d m_Translation = new Vector4d();
    protected Quat4d m_Rotation = new Quat4d();
    protected Vector4d m_Euler = new Vector4d();
    protected Node m_Parent;
    protected Scene m_Scene;
    protected IProperty[] m_Properties;
    private Vector4Property m_TranslationProperty;
    private Vector4Property m_RotationProperty;
    private Vector4Property m_EulerProperty;
    private int m_Flags = 0;
    protected AABB m_AABB = new AABB();
    private int errorFlags;
    private String[] errorMessages;
    private String identifier;

    // Psuedo states
    private Vector4d m_WorldTranslation = new Vector4d();
    private Vector4Property m_WorldTranslationProperty;

    public Node(String identifier, Scene scene, int flags)
    {
        this.identifier = identifier;
        m_Translation.set(0, 0, 0, 0);
        m_Rotation.set(0, 0, 0, 1);
        m_Parent = null;
        m_Scene = scene;
        m_Flags = flags;
        errorFlags = 0;
        errorMessages = new String[ERROR_FLAG_COUNT];

        List<IProperty> properties = new ArrayList<IProperty>();
        addProperties(properties);
        m_Properties = new IProperty[properties.size()];
        properties.toArray(m_Properties);
    }

    /**
     * Internal method for adding properties in sub-classes.
     * Inherited classes must call addProperties in super-class first in method
     * @param properties Property list to add to
     */
    protected void addProperties(List<IProperty> properties)
    {
        m_TranslationProperty = new Vector4Property(null, "Translation", "m_Translation", true, true);
        m_RotationProperty = new Vector4Property(null, "Rotation", "m_Rotation", false, false);
        m_EulerProperty = new Vector4Property(null, "Euler", "m_Euler", true, false);
        properties.add(m_TranslationProperty);
        properties.add(m_RotationProperty);
        properties.add(m_EulerProperty);

        m_WorldTranslationProperty = new Vector4Property(null, "WorldTranslation", "m_WorldTranslation", true, true);
        m_WorldTranslationProperty.setUpdater(new IPropertyUpdater() {

            @Override
            public void update(IProperty property) {
                Transform transform = new Transform();
                getWorldTransform(transform);
                transform.getTranslation(m_WorldTranslation);
            }
        }, true);
        properties.add(m_WorldTranslationProperty);
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

    public IProperty[] getProperties()
    {
        return m_Properties;
    }


    public void propertyChanged(IProperty property)
    {
        if (PropertyUtil.isPropertyOf(property, m_RotationProperty))
        {
            //m_Rotation.x =0.5;
            m_Rotation.normalize();
            MathUtil.quatToEuler(m_Rotation, m_Euler);
            m_Scene.propertyChanged(this, m_RotationProperty); // Could be changed to due normalize() above
            m_Scene.propertyChanged(this, m_EulerProperty);
        }
        else if (PropertyUtil.isPropertyOf(property, m_EulerProperty))
        {
            MathUtil.eulerToQuat(m_Euler, m_Rotation);
            m_Scene.propertyChanged(this, m_RotationProperty);
        }
        else if (PropertyUtil.isPropertyOf(property, m_WorldTranslationProperty))
        {
            Transform t = new Transform();
            getWorldTransform(t);
            t.setTranslation(m_WorldTranslation);
            NodeUtil.setWorldTransform(this, t);
        }
        else if (PropertyUtil.isPropertyOf(property, m_TranslationProperty))
        {
            updateWorldTranslationProperty();
        }
    }

    private void updateWorldTranslationProperty()
    {
        Transform t = new Transform();
        getWorldTransform(t);
        t.getTranslation(m_WorldTranslation);
        if (m_Scene != null)
        {
            m_Scene.propertyChanged(this, m_WorldTranslationProperty);
        }
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

        if (m_Parent != null) {
            m_Parent.removeNode(this);
        }
        m_Parent = parent;
        if (m_Parent != null) {
            m_Parent.addNode(this);
        }
        m_Scene.nodeReparented(this, parent);
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
        translation.set(m_Translation);
    }

    public void setLocalTranslation(Vector4d translation)
    {
        m_Translation.set(translation);
        m_Translation.w = 0;
        if (m_Scene != null) {
            m_Scene.nodeTransformChanged(this);
            m_Scene.propertyChanged(this, m_TranslationProperty);
        }

        updateWorldTranslationProperty();
    }

    public void setLocalRotation(Quat4d rotation)
    {
        m_Rotation.set(rotation);
        m_Rotation.normalize();
        update();
        if (m_Scene != null) {
            m_Scene.nodeTransformChanged(this);
            m_Scene.propertyChanged(this, m_RotationProperty);
            m_Scene.propertyChanged(this, m_EulerProperty);
        }

        updateWorldTranslationProperty();
    }

    public void getLocalTransform(Matrix4d transform)
    {
        transform.setIdentity();
        transform.setColumn(3, m_Translation);
        transform.m33 = 1;
        transform.setRotation(m_Rotation);
    }

    public void getLocalTransform(Transform transform)
    {
        transform.setTranslation(m_Translation);
        transform.setRotation(m_Rotation);
    }

    private void update()
    {
        MathUtil.quatToEuler(m_Rotation, m_Euler);

        Transform t = new Transform();
        getWorldTransform(t);
        t.getTranslation(m_WorldTranslation);
    }

    public void setLocalTransform(Matrix4d transform)
    {
        Vector4d last_posision = new Vector4d(m_Translation);
        Vector4d last_rotation = new Vector4d(m_Rotation);

        transform.getColumn(3, m_Translation);
        m_Translation.w = 0;
        m_Rotation.set(transform);
        //System.out.println(last_rotation + ", " + m_Rotation);
        m_Rotation.normalize();

        //System.out.println(transform);

        update();

        if (m_Scene != null)
        {
            m_Scene.nodeTransformChanged(this);

            last_posision.sub(m_Translation);
            if (last_posision.lengthSquared() > 0.0001)
            {
                m_Scene.propertyChanged(this, m_TranslationProperty);
            }

            last_rotation.sub(m_Rotation);
            if (last_rotation.lengthSquared() > 0.0001)
            {
                m_Scene.propertyChanged(this, m_RotationProperty);
                m_Scene.propertyChanged(this, m_EulerProperty);
            }
        }
        else
        {
            System.err.println("ERROR: No scene for node: " + this);
        }

        updateWorldTranslationProperty();
    }

    public void setLocalTransform(Transform transform)
    {
        transform.getTranslation(m_Translation);
        transform.getRotation(m_Rotation);
        m_Rotation.normalize();

        update();
        if (m_Scene != null) {
            m_Scene.nodeTransformChanged(this);
            m_Scene.propertyChanged(this, m_TranslationProperty);
            m_Scene.propertyChanged(this, m_RotationProperty);
            m_Scene.propertyChanged(this, m_EulerProperty);
        }

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
