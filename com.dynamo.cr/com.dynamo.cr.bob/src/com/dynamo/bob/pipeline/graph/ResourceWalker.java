// Copyright 2020-2023 The Defold Foundation
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
import java.util.HashSet;
import java.util.Set;
import java.util.Stack;

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
        public void visit(IResource resource) throws CompileExceptionError;
        public void leave(IResource resource) throws CompileExceptionError;
        public boolean shouldVisit(IResource resource);
    }


    private static void visitMessage(Project project, Message node, IResourceVisitor visitor) throws CompileExceptionError {
        List<FieldDescriptor> fields = node.getDescriptorForType().getFields();
        for (FieldDescriptor fieldDescriptor : fields) {
            FieldOptions options = fieldDescriptor.getOptions();
            FieldDescriptor resourceDesc = DdfExtensions.resource.getDescriptor();
            boolean isResource = (Boolean) options.getField(resourceDesc);
            Object value = node.getField(fieldDescriptor);
            if (value instanceof Message) {
                visitMessage(project, (Message) value, visitor);
            } else if (value instanceof List) {
                @SuppressWarnings("unchecked")
                List<Object> list = (List<Object>) value;
                for (Object v : list) {
                    if (v instanceof Message) {
                        visitMessage(project, (Message) v, visitor);
                    } else if (isResource && v instanceof String) {
                        visitResource(project, project.getResource((String) v), visitor);
                    }
                }
            } else if (isResource && value instanceof String) {
                visitResource(project, project.getResource((String) value), visitor);
            }
        }
    }

    private static void visitResource(Project project, IResource resource, IResourceVisitor visitor) throws CompileExceptionError {
        if (resource.getPath().equals("") || !visitor.shouldVisit(resource)) {
            return;
        }

        visitor.visit(resource);

        int i = resource.getPath().lastIndexOf(".");
        if (i == -1) {
            visitor.leave(resource);
            return;
        }

        String ext = resource.getPath().substring(i);
        if (!ProtoBuilder.supportsType(ext)) {
            visitor.leave(resource);
            return;
        }

        GeneratedMessageV3.Builder<?> builder = ProtoBuilder.newBuilder(ext);
        try {
            final byte[] content = resource.output().getContent();
            if(content == null) {
                throw new CompileExceptionError(resource, 0, "Unable to find resource " + resource.getPath());
            }
            builder.mergeFrom(content);
            Object message = builder.build();
            visitMessage(project, (Message) message, visitor);
        } catch(CompileExceptionError e) {
            throw e;
        } catch(Exception e) {
            throw new RuntimeException(e);
        }
        visitor.leave(resource);
    }

    public static void walk(Project project, IResource rootResource, IResourceVisitor visitor) throws CompileExceptionError {
        visitResource(project, rootResource, visitor);
    }

}