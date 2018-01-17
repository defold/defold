package com.dynamo.bob.util;

import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;
import java.util.List;

import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.BuilderUtil;
import com.dynamo.bob.pipeline.LuaScanner;
import com.dynamo.bob.pipeline.ProtoBuilders;
import com.dynamo.gameobject.proto.GameObject.PropertyDesc;
import com.dynamo.gameobject.proto.GameObject.PropertyType;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarationEntry;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarations;

public class PropertiesUtil {

    public static boolean transformPropertyDesc(IResource resource, PropertyDesc desc, PropertyDeclarations.Builder builder, Collection<String> propertytResources) {
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
            case PROPERTY_TYPE_BOOLEAN:
                entryBuilder.setIndex(builder.getFloatValuesCount());
                builder.addFloatValues(Boolean.parseBoolean(desc.getValue()) ? 1.0f : 0.0f);
                builder.addBoolEntries(entryBuilder);
                break;
            case PROPERTY_TYPE_RESOURCE:
                {
                    entryBuilder.setIndex(builder.getStringValuesCount());
                    String s = transformResourcePropertyValue(desc);
                    if(!s.isEmpty()) {
                        propertytResources.add(s);
                    }
                    builder.addStringValues(s);
                    builder.addResourceEntries(entryBuilder);
                }
                break;

            default:
                return false;
            }
        } catch (NumberFormatException e) {
            return false;
        }
        return true;
    }

    public static String transformResourcePropertyValue(PropertyType type, long subType, String value) {

        if((type == PropertyType.PROPERTY_TYPE_RESOURCE) && (!value.isEmpty())) {
            if (subType == LuaScanner.Property.subTypeMaterial) {
                if(!value.endsWith(".material")) {
                    throw new IllegalArgumentException(String.format("%s is not a material file", value));
                }
                value = BuilderUtil.replaceExt(value, ".material", ".materialc");
            }
            else if (subType == LuaScanner.Property.subTypeTextureSet) {
                String org_value = value;
                value = ProtoBuilders.transformTextureSetFilename(org_value);
                if(value.equals(org_value)) {
                     throw new IllegalArgumentException(String.format("%s is not a valid textureset file", org_value));
                }
            }
            else if (subType == LuaScanner.Property.subTypeTexture) {
                String org_value = value;
                value = ProtoBuilders.resolveTextureFilename(org_value);
                if(value.equals(org_value)) {
                     throw new IllegalArgumentException(String.format("%s is not a valid texture file", org_value));
                }
            }
        }
        return value;
    }

    public static String transformResourcePropertyValue(PropertyDesc desc) {
        return transformResourcePropertyValue(desc.getType(), desc.getSubType(), desc.getValue());
    }

    public static Collection<String> getPropertyDescResources(List<PropertyDesc> propertyDescList) {
        Collection<String> resources = new HashSet<String>();
        for (PropertyDesc desc : propertyDescList) {
            if(desc.getType() == PropertyType.PROPERTY_TYPE_RESOURCE) {
                String resource = desc.getValue();
                if(resource.isEmpty()) {
                    continue;
                }
                resources.add(resource);
            }
        }
        return resources;
    }

}
