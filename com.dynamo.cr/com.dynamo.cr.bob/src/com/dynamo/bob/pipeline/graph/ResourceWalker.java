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

package com.dynamo.bob.pipeline.graph;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.function.Function;

import com.dynamo.bob.Project;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.fs.IResource;
import com.dynamo.proto.DdfExtensions;
import com.dynamo.bob.ProtoBuilder;

import com.google.protobuf.GeneratedMessage;
import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.Descriptors.FieldDescriptor;
import com.google.protobuf.Message;


public class ResourceWalker {

    private static final ClassValue<Map<Descriptor, List<FieldDescriptor>>> REFERENCE_FIELDS = new ClassValue<>() {
        @Override
        protected Map<Descriptor, List<FieldDescriptor>> computeValue(Class<?> messageClass) {
            return new ConcurrentHashMap<>();
        }
    };

    public interface IResourceVisitor {
        public void visit(IResource resource, IResource parentResource) throws CompileExceptionError;
        public void visitMessage(Message message, IResource resource, IResource parentResource) throws CompileExceptionError;
        public void leave(IResource resource, IResource parentResource) throws CompileExceptionError;
        public boolean shouldVisit(IResource resource, IResource parentResource);

        default void visitCompiledContent(IResource output, byte[] content) {
        }
    }

    @FunctionalInterface
    private interface ReferenceVisitor {
        void visit(String path) throws CompileExceptionError;
    }

    private static List<FieldDescriptor> findReferenceFields(Descriptor descriptor) {
        List<FieldDescriptor> fields = new ArrayList<>();
        FieldDescriptor resource = DdfExtensions.resource.getDescriptor();
        for (FieldDescriptor field : descriptor.getFields()) {
            if (isResourceField(field, resource)
                    || (field.getJavaType() == FieldDescriptor.JavaType.MESSAGE
                        && hasReferenceFields(field.getMessageType(), resource, new HashSet<>()))) {
                fields.add(field);
            }
        }
        return List.copyOf(fields);
    }

    private static boolean isResourceField(FieldDescriptor field, FieldDescriptor resource) {
        return field.getJavaType() == FieldDescriptor.JavaType.STRING
                && Boolean.TRUE.equals(field.getOptions().getField(resource));
    }

    private static boolean hasReferenceFields(Descriptor descriptor, FieldDescriptor resource, Set<Descriptor> visited) {
        if (!visited.add(descriptor)) {
            return false;
        }
        for (FieldDescriptor field : descriptor.getFields()) {
            if (isResourceField(field, resource)
                    || (field.getJavaType() == FieldDescriptor.JavaType.MESSAGE
                        && hasReferenceFields(field.getMessageType(), resource, visited))) {
                return true;
            }
        }
        return false;
    }

    private static void visitReferences(Message node, ReferenceVisitor visitor) throws CompileExceptionError {
        List<FieldDescriptor> fields = REFERENCE_FIELDS.get(node.getClass())
                .computeIfAbsent(node.getDescriptorForType(), ResourceWalker::findReferenceFields);
        for (FieldDescriptor fieldDescriptor : fields) {
            Object value = node.getField(fieldDescriptor);
            if (value instanceof Message message) {
                visitReferences(message, visitor);
            } else if (value instanceof List<?> list) {
                for (Object v : list) {
                    if (v instanceof Message message) {
                        visitReferences(message, visitor);
                    } else if (v instanceof String path && !path.isEmpty()) {
                        visitor.visit(path);
                    }
                }
            } else if (value instanceof String path && !path.isEmpty()) {
                visitor.visit(path);
            }
        }
    }

    private static void visitResource(IResource parentResource, IResource resource, IResourceVisitor visitor,
            Function<String, IResource> resolve) throws CompileExceptionError {
        if (resource.getPath().isEmpty() || !visitor.shouldVisit(resource, parentResource)) {
            return;
        }

        visitor.visit(resource, parentResource);

        String path = resource.getPath();
        int dot = path.lastIndexOf('.');
        String ext = dot < 0 ? null : path.substring(dot);
        if (ext == null || !ProtoBuilder.supportsType(ext)) {
            visitor.leave(resource, parentResource);
            return;
        }

        try {
            IResource output = resource.output();
            GeneratedMessage.Builder builder = ProtoBuilder.newBuilder(ext);
            byte[] content = output.getContent();
            if (content == null) {
                throw new CompileExceptionError(resource, 0, "Unable to find resource " + resource.getPath());
            }
            visitor.visitCompiledContent(output, content);
            builder.mergeFrom(content);
            Message message = builder.build();
            visitor.visitMessage(message, resource, parentResource);
            visitReferences(message, child -> visitResource(resource, resolve.apply(child), visitor, resolve));
        } catch(CompileExceptionError e) {
            throw e;
        } catch(Exception e) {
            throw new RuntimeException("Resource '" + resource.getPath() + "': " + e.getMessage(), e);
        }
        visitor.leave(resource, parentResource);
    }

    public static void walk(Project project, IResource rootResource, IResourceVisitor visitor) throws CompileExceptionError {
        Map<String, IResource> resolved = new HashMap<>();
        Function<String, IResource> getResource = project::getResource;
        visitResource(null, rootResource, visitor, path -> resolved.computeIfAbsent(path, getResource));
    }

}
