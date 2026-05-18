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

package com.dynamo.bob.pipeline;

import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.Collection;
import java.util.LinkedHashMap;
import java.util.Map;

import com.dynamo.bob.util.MurmurHash;
import com.dynamo.gamesys.proto.Gui.Property.PropertyType;

public class GuiCustomTypeRegistry {
    public static class Property {
        private final String name;
        private final long nameHash;
        private final Object defaultValue;
        private final PropertyType propertyType;
        private final int editTypeFlags;

        private Property(String name, Object defaultValue, PropertyType propertyType, int editTypeFlags) {
            this.name = name;
            this.nameHash = MurmurHash.hash64(name);
            this.defaultValue = defaultValue;
            this.propertyType = propertyType;
            this.editTypeFlags = editTypeFlags;
        }

        public String getName() {
            return name;
        }

        public long getNameHash() {
            return nameHash;
        }

        public Object getDefaultValue() {
            return defaultValue;
        }

        public PropertyType getPropertyType() {
            return propertyType;
        }

        public boolean isResource() {
            return (editTypeFlags & IGuiCustomType.EDIT_TYPE_RESOURCE) != 0;
        }
    }

    public static class Type implements IGuiCustomType {
        private final String name;
        private final int nameHash;
        private final LinkedHashMap<String, Property> properties = new LinkedHashMap<String, Property>();
        private final Map<Long, Property> propertiesByHash = new LinkedHashMap<Long, Property>();
        private Method migratePropertiesMethod;

        private Type(String name) {
            this.name = name;
            this.nameHash = MurmurHash.hash32(name);
        }

        @Override
        public void addProperty(String name, Object defaultValue, PropertyType propertyType, int editTypeFlags) {
            Property property = new Property(name, defaultValue, propertyType, editTypeFlags);
            properties.put(name, property);
            propertiesByHash.put(property.getNameHash(), property);
        }

        public String getName() {
            return name;
        }

        public int getNameHash() {
            return nameHash;
        }

        public Collection<Property> getProperties() {
            return properties.values();
        }

        public Property getProperty(String name) {
            return properties.get(name);
        }

        public Property getProperty(long nameHash) {
            return propertiesByHash.get(nameHash);
        }

        public void migrateProperties(Map<String, Object> properties) {
            if (migratePropertiesMethod == null) {
                return;
            }
            try {
                migratePropertiesMethod.invoke(null, properties);
            } catch (Exception e) {
                throw new RuntimeException("Unable to migrate gui custom node properties for type '" + name + "'", e);
            }
        }
    }

    private static final Map<Integer, Type> typesByHash = new LinkedHashMap<Integer, Type>();
    private static final Map<String, Type> typesByName = new LinkedHashMap<String, Type>();

    public static void register(Class<?> klass) {
        GuiCustomNode annotation = klass.getAnnotation(GuiCustomNode.class);
        if (annotation == null) {
            return;
        }
        if (!IGuiCustomNode.class.isAssignableFrom(klass)) {
            throw new RuntimeException("Class " + klass.getName() + " is annotated with @GuiCustomNode but does not implement IGuiCustomNode");
        }

        Type type = new Type(annotation.type());
        invokeRegisterProperties(klass, type);
        type.migratePropertiesMethod = findStaticMethod(klass, "migrateProperties", Map.class);

        typesByHash.put(type.getNameHash(), type);
        typesByName.put(type.getName(), type);
    }

    public static Type getByHash(int nameHash) {
        return typesByHash.get(nameHash);
    }

    public static Type getByName(String name) {
        return typesByName.get(name);
    }

    private static void invokeRegisterProperties(Class<?> klass, Type type) {
        Method method = findStaticMethod(klass, "registerProperties", IGuiCustomType.class);
        if (method == null) {
            return;
        }
        try {
            method.invoke(null, type);
        } catch (Exception e) {
            throw new RuntimeException("Unable to register gui custom node properties for class " + klass.getName(), e);
        }
    }

    private static Method findStaticMethod(Class<?> klass, String name, Class<?> parameterType) {
        try {
            Method method = klass.getMethod(name, parameterType);
            if (!Modifier.isStatic(method.getModifiers())) {
                throw new RuntimeException("Method " + klass.getName() + "." + name + " must be static");
            }
            return method;
        } catch (NoSuchMethodException e) {
            return null;
        }
    }
}
