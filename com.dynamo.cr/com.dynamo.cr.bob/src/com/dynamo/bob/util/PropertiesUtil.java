// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.dynamo.bob.util;

import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.Project;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.fs.ResourceUtil;
import com.dynamo.bob.pipeline.ProtoBuilders;
import com.dynamo.gameobject.proto.GameObject.PropertyDesc;
import com.dynamo.gameobject.proto.GameObject.PropertyType;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarationEntry;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarations;

public class PropertiesUtil {

    public static boolean transformPropertyDesc(Project project, PropertyDesc desc, PropertyDeclarations.Builder builder, Collection<String> propertyResources) throws CompileExceptionError {
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
                    value = transformResourcePropertyValue(null, value);
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
    public static IResource getResourceProperty(Project project, PropertyType type, String value) {
        if (type == PropertyType.PROPERTY_TYPE_HASH) {
            IResource resource = project.getResource(value);
            if (resource.exists())
                return resource;
        }
        return null;
    }

    public static String transformResourcePropertyValue(IResource resource, String value) throws CompileExceptionError {
        String ext = "." + FilenameUtils.getExtension(value);
        // This is not optimal, but arguably ok for this case.
        value = ResourceUtil.replaceExt(value, ".material", ".materialc");
        value = ResourceUtil.replaceExt(value, ".font", ".fontc");
        value = ResourceUtil.replaceExt(value, ".buffer", ".bufferc");
        value = ProtoBuilders.replaceTextureName(value);
        try {
            if (TextureUtil.isAtlasFileType(ext))
            {
                value = ProtoBuilders.replaceTextureSetName(value);
            }
        } catch (Exception e) {
            throw new CompileExceptionError(resource, -1, e.getMessage(), e);
        }
        value = ResourceUtil.minifyPath(value);
        return value;
    }

    public static Map<String, String> getPropertyDescResources(Project project, List<PropertyDesc> propertyDescList) {
        Map<String, String> resources = new HashMap<>();
        for (PropertyDesc desc : propertyDescList) {
            if (isResourceProperty(project, desc.getType(), desc.getValue())) {
                resources.put(desc.getId(), desc.getValue());
            }
        }
        return resources;
    }

}
