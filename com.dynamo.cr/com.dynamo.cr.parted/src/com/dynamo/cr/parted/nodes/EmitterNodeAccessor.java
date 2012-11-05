package com.dynamo.cr.parted.nodes;

import com.dynamo.cr.properties.IPropertyAccessor;
import com.dynamo.cr.properties.types.ValueSpread;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.particle.proto.Particle.EmitterType;

public class EmitterNodeAccessor implements IPropertyAccessor<EmitterNode, ISceneModel>{

    private EmitterNode node;

    public EmitterNodeAccessor(EmitterNode node) {
        this.node = node;
    }

    @Override
    public void setValue(EmitterNode obj, String property, Object value,
            ISceneModel world) {
        if (value instanceof Double[]) {
            Double[] a = (Double[]) value;
            ValueSpread tmp = node.getProperty(property);
            if (a[0] != null)
                tmp.setValue(a[0]);
            if (a[1] != null)
                tmp.setSpread(a[1]);
            node.setProperty(property, tmp);
        } else {
            node.setProperty(property, (ValueSpread) value);
        }
    }

    @Override
    public void resetValue(EmitterNode obj, String property, ISceneModel world) {
    }

    @Override
    public Object getValue(EmitterNode obj, String property, ISceneModel world) {
        ValueSpread val = node.getProperty(property);
        return val;
    }

    @Override
    public boolean isEditable(EmitterNode obj, String property,
            ISceneModel world) {
        EmitterType t = obj.getEmitterType();

        if (property.equals("EMITTER_KEY_SIZE_X")) {
            return true;
        } else if (property.startsWith("EMITTER_KEY_SIZE_Y") ) {
            return t == EmitterType.EMITTER_TYPE_CONE
                || t == EmitterType.EMITTER_TYPE_2DCONE
                || t == EmitterType.EMITTER_TYPE_BOX;
        } else if (property.startsWith("EMITTER_KEY_SIZE_Z")) {
            return t == EmitterType.EMITTER_TYPE_BOX;
        }

        return true;
    }

    @Override
    public boolean isVisible(EmitterNode obj, String property, ISceneModel world) {
        return true;
    }

    @Override
    public boolean isOverridden(EmitterNode obj, String property,
            ISceneModel world) {
        return false;
    }

    @Override
    public Object[] getPropertyOptions(EmitterNode obj, String property,
            ISceneModel world) {
        return null;
    }

}
