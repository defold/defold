package com.dynamo.cr.ddfeditor.scene;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IStatus;

import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Resource;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.physics.proto.Physics.CollisionObjectType;

public class CollisionObjectNode extends ComponentTypeNode {

    @Property(isResource=true)
    @Resource
    private String collisionShape = "";
    @Property private CollisionObjectType type = CollisionObjectType.COLLISION_OBJECT_TYPE_DYNAMIC;
    @Property private float mass;
    @Property private float friction;
    @Property private float restitution;
    @Property private String group = "";
    @Property private String mask = "";
    private Node collisionShapeNode;

    public CollisionObjectNode() {
        super();
    }

    @Override
    public void handleReload(IFile file) {
        IFile componentFile = getModel().getFile(this.collisionShape);
        if (componentFile.exists() && componentFile.equals(file)) {
            reloadCollisionShape();
        }
    }

    private void reloadCollisionShape() {
        ISceneModel model = getModel();
        if (model != null) {
            try {
                Node node = model.loadNode(this.collisionShape);
                if (this.collisionShapeNode != null) {
                    this.collisionShapeNode = node;
                }
                notifyChange();
            } catch (Throwable e) {
                // no reason to handle exception since having a null collision shape is invalid state, will be caught in resource validation
            }
        }
    }

    public String getCollisionShape() {
        return this.collisionShape;
    }

    public void setCollisionShape(String collisionShape) {
        if (!this.collisionShape.equals(collisionShape)) {
            this.collisionShape = collisionShape;
            notifyChange();
        }
    }

    public CollisionObjectType getType() {
        return this.type;
    }

    public void setType(CollisionObjectType type) {
        if (this.type != type) {
            this.type = type;
            notifyChange();
        }
    }

    public float getMass() {
        if (isMassEditable())
            return this.mass;
        else
            return 0.0f;
    }

    public void setMass(float mass) {
        if (this.mass != mass) {
            this.mass = mass;
            notifyChange();
        }
    }

    public boolean isMassEditable() {
        return getType() == CollisionObjectType.COLLISION_OBJECT_TYPE_DYNAMIC;
    }

    public float getFriction() {
        return this.friction;
    }

    public void setFriction(float friction) {
        if (this.friction != friction) {
            this.friction = friction;
            notifyChange();
        }
    }

    public float getRestitution() {
        return this.restitution;
    }

    public void setRestitution(float restitution) {
        if (this.restitution != restitution) {
            this.restitution = restitution;
            notifyChange();
        }
    }

    public String getGroup() {
        return group;
    }

    public void setGroup(String group) {
        if (!this.group.equals(group)) {
            this.group = group;
            notifyChange();
        }
    }

    public String getMask() {
        return mask;
    }

    public void setMask(String mask) {
        if (!this.mask.equals(mask)) {
            this.mask = mask;
            notifyChange();
        }
    }

    @Override
    public String getTypeId() {
        return "collisionobject";
    }

    @Override
    public String getTypeName() {
        return "Collision Object";
    }

    public void setCollisionShapeNode(Node collisionShapeNode) {
        this.collisionShapeNode = collisionShapeNode;
    }

    public Node getCollisionShapeNode() {
        return collisionShapeNode;
    }

    @Override
    protected IStatus doValidate() {
        return validateProperties(new String[] {
                "collisionShape",
                "type",
                "mass",
                "friction",
                "restitution",
                "group",
                "mask"
        });
    }
}
