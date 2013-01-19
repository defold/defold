package com.dynamo.cr.go.core;

import java.util.StringTokenizer;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;

import com.dynamo.cr.properties.IPropertyDesc;
import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.properties.descriptors.BooleanPropertyDesc;
import com.dynamo.cr.properties.descriptors.DoublePropertyDesc;
import com.dynamo.cr.properties.descriptors.Quat4PropertyDesc;
import com.dynamo.cr.properties.descriptors.TextPropertyDesc;
import com.dynamo.cr.properties.descriptors.Vector3PropertyDesc;
import com.dynamo.cr.properties.descriptors.Vector4PropertyDesc;
import com.dynamo.gameobject.proto.GameObject;
import com.dynamo.gameobject.proto.GameObject.PropertyType;

public class GoPropertyUtil {

    public static String propertyToString(Object value) {
        if (value instanceof Double) {
            return ((Double)value).toString();
        } else if (value instanceof String) {
            return (String)value;
        } else if (value instanceof Vector3d) {
            Vector3d v = (Vector3d)value;
            return "" + v.getX() + ", " + v.getY() + ", " + v.getZ();
        } else if (value instanceof Vector4d) {
            Vector4d v = (Vector4d)value;
            return "" + v.getX() + ", " + v.getY() + ", " + v.getZ() + ", " + v.getW();
        } else if (value instanceof Quat4d) {
            Quat4d v = (Quat4d)value;
            return "" + v.getX() + ", " + v.getY() + ", " + v.getZ() + ", " + v.getW();
        } else if (value instanceof Boolean) {
            return ((Boolean)value).toString();
        }
        return null;
    }

    public static Object stringToProperty(GameObject.PropertyType type, String value) {
        StringTokenizer tokenizer = new StringTokenizer(value, ",");
        switch (type) {
        case PROPERTY_TYPE_NUMBER:
            return Double.parseDouble(value);
        case PROPERTY_TYPE_HASH:
        case PROPERTY_TYPE_URL:
            return value;
        case PROPERTY_TYPE_VECTOR3:
            return new Vector3d(Double.parseDouble(tokenizer.nextToken().trim()),
                    Double.parseDouble(tokenizer.nextToken().trim()),
                    Double.parseDouble(tokenizer.nextToken().trim()));
        case PROPERTY_TYPE_VECTOR4:
            return new Vector4d(Double.parseDouble(tokenizer.nextToken().trim()),
                    Double.parseDouble(tokenizer.nextToken().trim()),
                    Double.parseDouble(tokenizer.nextToken().trim()),
                    Double.parseDouble(tokenizer.nextToken().trim()));
        case PROPERTY_TYPE_QUAT:
            return new Quat4d(Double.parseDouble(tokenizer.nextToken().trim()),
                    Double.parseDouble(tokenizer.nextToken().trim()),
                    Double.parseDouble(tokenizer.nextToken().trim()),
                    Double.parseDouble(tokenizer.nextToken().trim()));
        case PROPERTY_TYPE_BOOLEAN:
            return Boolean.parseBoolean(value);
        }
        return null;
    }

    public static <T, U extends IPropertyObjectWorld> IPropertyDesc<T, U> typeToDesc(PropertyType type, String id, String name, String category, EditorType editorType) {
        IPropertyDesc<T, U> desc = null;
        switch (type) {
        case PROPERTY_TYPE_NUMBER:
            desc = new DoublePropertyDesc<T, U>(id, name, category, editorType);
            break;
        case PROPERTY_TYPE_HASH:
        case PROPERTY_TYPE_URL:
            desc = new TextPropertyDesc<T, U>(id, name, category, editorType);
            break;
        case PROPERTY_TYPE_VECTOR3:
            desc = new Vector3PropertyDesc<T, U>(id, name, category);
            break;
        case PROPERTY_TYPE_VECTOR4:
            desc = new Vector4PropertyDesc<T, U>(id, name, category);
            break;
        case PROPERTY_TYPE_QUAT:
            desc = new Quat4PropertyDesc<T, U>(id, name, category);
            break;
        case PROPERTY_TYPE_BOOLEAN:
            desc = new BooleanPropertyDesc<T, U>(id, name, category);
            break;
        }
        return desc;
    }
}
