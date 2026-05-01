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

import java.util.List;

import com.dynamo.bob.Project;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.fs.IResource;
import com.dynamo.proto.DdfExtensions;
import com.dynamo.bob.ProtoBuilder;

import com.google.protobuf.GeneratedMessageV3;
import com.google.protobuf.DescriptorProtos.FieldOptions;
import com.google.protobuf.Descriptors.FieldDescriptor;
import com.google.protobuf.Message;


public class ResourceWalker {

    public interface IResourceVisitor {
        public void visit(IResource resource, IResource parentResource) throws CompileExceptionError;
        public void visitMessage(Message message, IResource resource, IResource parentResource) throws CompileExceptionError;
        public void leave(IResource resource, IResource parentResource) throws CompileExceptionError;
        public boolean shouldVisit(IResource resource, IResource parentResource);
    }

    private static void visitMessage(Project project, IResource currentResource, Message node, IResourceVisitor visitor) throws CompileExceptionError {
        List<FieldDescriptor> fields = node.getDescriptorForType().getFields();
        for (FieldDescriptor fieldDescriptor : fields) {
            FieldOptions options = fieldDescriptor.getOptions();
            FieldDescriptor resourceDesc = DdfExtensions.resource.getDescriptor();
            boolean isResource = (Boolean) options.getField(resourceDesc);
            Object value = node.getField(fieldDescriptor);
            if (value instanceof Message) {
                visitMessage(project, currentResource, (Message) value, visitor);
            } else if (value instanceof List) {
                @SuppressWarnings("unchecked")
                List<Object> list = (List<Object>) value;
                for (Object v : list) {
                    if (v instanceof Message) {
                        visitMessage(project, currentResource, (Message) v, visitor);
                    } else if (isResource && v instanceof String) {
                        visitResource(project, currentResource, project.getResource((String) v), visitor);
                    }
                }
            } else if (isResource && value instanceof String) {
                visitResource(project, currentResource, project.getResource((String) value), visitor);
            }
        }
    }

    private static void visitResource(Project project, IResource parentResource, IResource resource, IResourceVisitor visitor) throws CompileExceptionError {
        if (resource.getPath().equals("") || !visitor.shouldVisit(resource, parentResource)) {
            return;
        }

        visitor.visit(resource, parentResource);

        int i = resource.getPath().lastIndexOf(".");
        if (i == -1) {
            visitor.leave(resource, parentResource);
            return;
        }

        String ext = resource.getPath().substring(i);
        if (!ProtoBuilder.supportsType(ext)) {
            visitor.leave(resource, parentResource);
            return;
        }

        GeneratedMessageV3.Builder builder = ProtoBuilder.newBuilder(ext);
        try {
            final byte[] content = resource.output().getContent();
            if(content == null) {
                throw new CompileExceptionError(resource, 0, "Unable to find resource " + resource.getPath());
            }
            builder.mergeFrom(content);
            Message message = builder.build();
            visitor.visitMessage(message, resource, parentResource);
            visitMessage(project, resource, message, visitor);
        } catch(CompileExceptionError e) {
            throw e;
        } catch(Exception e) {
            throw new RuntimeException("Resource '" + resource.getPath() + "': " + e.getMessage(), e);
        }
        visitor.leave(resource, parentResource);
    }

    public static void walk(Project project, IResource rootResource, IResourceVisitor visitor) throws CompileExceptionError {
        visitResource(project, null, rootResource, visitor);
    }

}