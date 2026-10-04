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

import java.io.InputStream;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.security.DigestInputStream;
import java.security.MessageDigest;
import java.util.Collection;
import java.util.HexFormat;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.gamesys.proto.Gui.Property.PropertyType;
import com.dynamo.gamesys.proto.Gui.NodeDesc;

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

        public boolean isRequiredResource() {
            return (editTypeFlags & IGuiCustomType.EDIT_TYPE_RESOURCE_REQUIRED) == IGuiCustomType.EDIT_TYPE_RESOURCE_REQUIRED;
        }
    }

    public static class Type implements IGuiCustomType {
        private final String name;
        private final int nameHash;
        private final LinkedHashMap<String, Property> properties = new LinkedHashMap<String, Property>();
        private final Map<Long, Property> propertiesByHash = new LinkedHashMap<Long, Property>();
        private Method migratePropertiesMethod;
        private Method validateNodesMethod;
        private String signature;

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

        public String getSignature() {
            return signature;
        }

        public boolean hasValidator() {
            return validateNodesMethod != null;
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

        public void validateNodes(Project project, IResource resource, List<NodeDesc> nodes, Map<String, IResource> resources) throws CompileExceptionError {
            if (validateNodesMethod == null) {
                return;
            }
            try {
                validateNodesMethod.invoke(null, project, resource, nodes, resources);
            } catch (InvocationTargetException e) {
                Throwable cause = e.getCause();
                if (cause instanceof CompileExceptionError compileError) {
                    throw compileError;
                }
                throw new CompileExceptionError(resource, 0, "Unable to validate GUI custom nodes of type '" + name + "': " + cause.getMessage(), cause);
            } catch (IllegalAccessException e) {
                throw new CompileExceptionError(resource, 0, "Unable to validate GUI custom nodes of type '" + name + "'", e);
            }
        }
    }

    private final Map<Integer, Type> typesByHash = new LinkedHashMap<Integer, Type>();
    private final Map<String, Type> typesByName = new LinkedHashMap<String, Type>();

    public void register(Class<?> klass) {
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
        type.validateNodesMethod = findStaticMethod(klass, "validateNodes", Project.class, IResource.class, List.class, Map.class);
        type.signature = calculateSignature(klass, type);

        typesByHash.put(type.getNameHash(), type);
        typesByName.put(type.getName(), type);
    }

    public Type getByHash(int nameHash) {
        return typesByHash.get(nameHash);
    }

    public Type getByName(String name) {
        return typesByName.get(name);
    }

    private static String calculateSignature(Class<?> klass, Type type) {
        try {
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            URL location = klass.getProtectionDomain().getCodeSource() == null
                    ? null : klass.getProtectionDomain().getCodeSource().getLocation();
            // Hash the whole plugin JAR, including helpers used by its validator and registration.
            // Exploded development classes have no JAR; use the registration class in that case.
            URL implementation = location != null && location.getPath().endsWith(".jar")
                    ? location : klass.getResource("/" + klass.getName().replace('.', '/') + ".class");
            if (implementation == null) {
                throw new IllegalStateException("Unable to locate class bytes");
            }
            var connection = implementation.openConnection();
            connection.setUseCaches(false);
            try (InputStream stream = new DigestInputStream(connection.getInputStream(), digest)) {
                stream.transferTo(java.io.OutputStream.nullOutputStream());
            }
            digest.update(klass.getName().getBytes(StandardCharsets.UTF_8));
            for (Property property : type.getProperties()) {
                String definition = "\0" + property.name + "\0" + property.propertyType + "\0"
                        + property.editTypeFlags + "\0" + property.defaultValue;
                digest.update(definition.getBytes(StandardCharsets.UTF_8));
            }
            return "gui-custom-type:" + type.name + ":" + HexFormat.of().formatHex(digest.digest());
        } catch (Exception e) {
            throw new RuntimeException("Unable to calculate GUI custom node signature for class " + klass.getName(), e);
        }
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

    private static Method findStaticMethod(Class<?> klass, String name, Class<?>... parameterTypes) {
        try {
            Method method = klass.getMethod(name, parameterTypes);
            if (!Modifier.isStatic(method.getModifiers())) {
                throw new RuntimeException("Method " + klass.getName() + "." + name + " must be static");
            }
            return method;
        } catch (NoSuchMethodException e) {
            return null;
        }
    }
}
