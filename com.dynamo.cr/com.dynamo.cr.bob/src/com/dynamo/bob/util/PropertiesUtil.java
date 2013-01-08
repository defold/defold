package com.dynamo.bob.util;

import java.util.Arrays;
import java.util.List;

import com.dynamo.bob.IResource;
import com.dynamo.gameobject.proto.GameObject.PropertyDesc;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarationEntry;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarations;

public class PropertiesUtil {

    public static boolean transformPropertyDesc(IResource resource, PropertyDesc desc, PropertyDeclarations.Builder builder) {
        PropertyDeclarationEntry.Builder entryBuilder = PropertyDeclarationEntry.newBuilder();
        entryBuilder.setKey(desc.getId());
        entryBuilder.setId(MurmurHash.hash64(desc.getId()));
        List<String> items = Arrays.asList(desc.getValue().split("\\s*,\\s*"));
        try {
            switch (desc.getType()) {
            case PROPERTY_TYPE_NUMBER:
                entryBuilder.setIndex(builder.getFloatValuesCount());
                builder.addFloatValues(Float.parseFloat(desc.getValue()));
                builder.addNumberEntries(entryBuilder);
                break;
            case PROPERTY_TYPE_HASH:
                entryBuilder.setIndex(builder.getHashValuesCount());
                builder.addHashValues(MurmurHash.hash64(desc.getValue()));
                builder.addHashEntries(entryBuilder);
                break;
            case PROPERTY_TYPE_URL:
                entryBuilder.setIndex(builder.getStringValuesCount());
                builder.addStringValues(desc.getValue());
                builder.addUrlEntries(entryBuilder);
                break;
            case PROPERTY_TYPE_VECTOR3:
                entryBuilder.setIndex(builder.getFloatValuesCount());
                if (items.size() != 3) {
                    return false;
                }
                builder.addFloatValues(Float.parseFloat(items.get(0)));
                builder.addFloatValues(Float.parseFloat(items.get(1)));
                builder.addFloatValues(Float.parseFloat(items.get(2)));
                builder.addVector3Entries(entryBuilder);
                break;
            case PROPERTY_TYPE_VECTOR4:
                entryBuilder.setIndex(builder.getFloatValuesCount());
                if (items.size() != 4) {
                    return false;
                }
                builder.addFloatValues(Float.parseFloat(items.get(0)));
                builder.addFloatValues(Float.parseFloat(items.get(1)));
                builder.addFloatValues(Float.parseFloat(items.get(2)));
                builder.addFloatValues(Float.parseFloat(items.get(3)));
                builder.addVector4Entries(entryBuilder);
                break;
            case PROPERTY_TYPE_QUAT:
                entryBuilder.setIndex(builder.getFloatValuesCount());
                if (items.size() != 4) {
                    return false;
                }
                builder.addFloatValues(Float.parseFloat(items.get(0)));
                builder.addFloatValues(Float.parseFloat(items.get(1)));
                builder.addFloatValues(Float.parseFloat(items.get(2)));
                builder.addFloatValues(Float.parseFloat(items.get(3)));
                builder.addQuatEntries(entryBuilder);
                break;
            default:
                return false;
            }
        } catch (NumberFormatException e) {
            return false;
        }
        return true;
    }
}
