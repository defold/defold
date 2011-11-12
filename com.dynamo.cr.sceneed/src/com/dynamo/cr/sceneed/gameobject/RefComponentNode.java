package com.dynamo.cr.sceneed.gameobject;

import com.dynamo.cr.properties.Property;

public class RefComponentNode extends ComponentNode {

    @Property(isResource=true)
    private String reference;

    public RefComponentNode(ComponentTypeNode type) {
        super(type);
    }

    public String getReference() {
        return this.reference;
    }

    public void setReference(String reference) {
        if (this.reference != null ? !this.reference.equals(reference) : reference != null) {
            this.reference = reference;
            notifyChange();
        }
    }

    @Override
    public String toString() {
        return String.format("%s (%s)", getId(), this.reference);
    }

}
