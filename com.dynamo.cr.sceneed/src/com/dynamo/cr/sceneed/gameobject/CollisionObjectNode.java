package com.dynamo.cr.sceneed.gameobject;

import com.dynamo.cr.properties.Property;
import com.dynamo.physics.proto.Physics.CollisionObjectType;

public class CollisionObjectNode extends ComponentTypeNode {

    @Property(isResource=true)
    private String collisionShape = "";
    @Property private CollisionObjectType type;
    @Property private float mass;
    @Property private float friction;
    @Property private float restitution;
    @Property private String group;
    @Property private String mask;

    public String getCollisionShape() {
        return this.collisionShape;
    }

    public void setCollisionShape(String collisionShape) {
        this.collisionShape = collisionShape;
    }

    public CollisionObjectType getType() {
        return this.type;
    }

    public void setType(CollisionObjectType type) {
        this.type = type;
    }

    public float getMass() {
        return this.mass;
    }

    public void setMass(float mass) {
        this.mass = mass;
    }

    public float getFriction() {
        return this.friction;
    }

    public void setFriction(float friction) {
        this.friction = friction;
    }

    public float getRestitution() {
        return this.restitution;
    }

    public void setRestitution(float restitution) {
        this.restitution = restitution;
    }

    public String getGroup() {
        return group;
    }

    public void setGroup(String group) {
        this.group = group;
    }

    public String getMask() {
        return mask;
    }

    public void setMask(String mask) {
        this.mask = mask;
    }

    @Override
    public String getTypeName() {
        return "Collision Object";
    }

}
