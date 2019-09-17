package com.dynamo.bob.util;

import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;
import java.util.List;

import com.dynamo.bob.Project;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.BuilderUtil;
import com.dynamo.bob.pipeline.ProtoBuilders;
import com.dynamo.gameobject.proto.GameObject.PropertyDesc;
import com.dynamo.gameobject.proto.GameObject.PropertyType;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarationEntry;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarations;

public class PropertiesUtil {

    public static boolean transformPropertyDesc(Project project, PropertyDesc desc, PropertyDeclarations.Builder builder, Collection<String> propertyResources) {
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
                String value = desc.getValue();
                if (isResourceProperty(project, desc.getType(), desc.getValue())) {
                    value = transformResourcePropertyValue(value);
                    propertyResources.add(value);
                }
                entryBuilder.setIndex(builder.getHashValuesCount());
                builder.addHashValues(MurmurHash.hash64(value));
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
            default:
                return false;
            }
        } catch (NumberFormatException e) {
            return false;
        }
        return true;
    }

    /* NOTE!
     * This is a very weak method of determining whether a hash-property should be considered a resource.
     * The test checks for existing of the value (=path) as a file on disk, if so it is included as a resource file.
     * This might not be what the user expects, and if so the file is erroneously included in the build and loaded into memory.
     * Note that the user must use the full path as a property value (e.g. "/x.png") for this check to falsely evaluate truthy.
     * The reason for this hack is that it would require a lot of code changes to do this properly. E.g. when building a collection
     * we would need to load each sub-collection, prototype, script file etc. to determine if the property is referencing a resource
     * or not.
     */
    public static boolean isResourceProperty(Project project, PropertyType type, String value) {
        if (type == PropertyType.PROPERTY_TYPE_HASH) {
            IResource resource = project.getResource(value);
            return resource.exists();
        }
        return false;
    }

    public static String transformResourcePropertyValue(String value) {
        // This is not optimal, but arguably ok for this case.
        value = BuilderUtil.replaceExt(value, ".material", ".materialc");
        value = BuilderUtil.replaceExt(value, ".font", ".fontc");
        value = ProtoBuilders.replaceTextureName(value);
        value = ProtoBuilders.replaceTextureSetName(value);
        return value;
    }

    public static Collection<String> getPropertyDescResources(Project project, List<PropertyDesc> propertyDescList) {
        Collection<String> resources = new HashSet<String>();
        for (PropertyDesc desc : propertyDescList) {
            if (isResourceProperty(project, desc.getType(), desc.getValue())) {
                resources.add(desc.getValue());
            }
        }
        return resources;
    }

}
