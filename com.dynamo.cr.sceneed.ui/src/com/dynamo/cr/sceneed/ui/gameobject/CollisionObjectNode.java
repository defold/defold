package com.dynamo.cr.sceneed.ui.gameobject;

import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.ui.Resource;
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

    public CollisionObjectNode() {
        super();
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
        return this.mass;
    }

    public void setMass(float mass) {
        if (this.mass != mass) {
            this.mass = mass;
            notifyChange();
        }
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

}
