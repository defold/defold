package com.dynamo.cr.ddfeditor.refactoring;

import java.util.List;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.editor.core.IResourceRefactorParticipant;
import com.dynamo.cr.editor.core.ResourceRefactorContext;
import com.dynamo.cr.protobind.IPath;
import com.dynamo.cr.protobind.MessageNode;
import com.dynamo.cr.protobind.RepeatedNode;
import com.dynamo.proto.DdfExtensions;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;
import com.google.protobuf.DescriptorProtos.FieldOptions;
import com.google.protobuf.Descriptors.FieldDescriptor;
import com.google.protobuf.Message.Builder;
import com.google.protobuf.TextFormat.ParseException;

public abstract class GenericRefactorParticipant implements IResourceRefactorParticipant {

    public abstract Builder getBuilder();

    public static boolean doUpdateReferences(IContainer contentRoot, IFile reference, String newPath, MessageNode root, MessageNode node) {
        List<FieldDescriptor> fields = node.getDescriptor().getFields();

        boolean changed = false;
        for (FieldDescriptor fieldDescriptor : fields) {
            FieldOptions options = fieldDescriptor.getOptions();
            FieldDescriptor resourceDesc = DdfExtensions.resource.getDescriptor();
            boolean isResource = (Boolean) options.getField(resourceDesc);
            IPath path = node.getPathTo(fieldDescriptor.getName());
            Object value = root.getField(path);
            if (isResource) {
                if (value instanceof RepeatedNode) {
                    RepeatedNode repeated = (RepeatedNode) value;
                    List<Object> values = repeated.getValueList();
                    int index = 0;
                    for (Object listValue : values) {
                        if (listValue instanceof MessageNode) {
                            MessageNode listNode = (MessageNode) listValue;
                            if (doUpdateReferences(contentRoot, reference, newPath, root, listNode))
                                changed = true;
                        } else {
                            IFile file = contentRoot.getFile(new Path((String) listValue));
                            if (reference.equals(file)) {
                                root.setField(repeated.getPathTo(index), newPath);
                                changed = true;
                            }
                        }
                        ++index;
                    }
                }
                else {
                    String tmp = (String) root.getField(path);
                    if (tmp.length() > 0) {
                        IFile file = contentRoot.getFile(new Path(tmp));
                        if (reference.equals(file)) {
                            root.setField(path, newPath);
                            changed = true;
                        }
                    }
                }

            }
            else {
                if (value instanceof MessageNode) {
                    MessageNode subNode = (MessageNode) value;
                    if (doUpdateReferences(contentRoot, reference, newPath, root, subNode))
                        changed = true;
                }
            }
        }

        return changed;
    }

    @Override
    public String updateReferences(ResourceRefactorContext context,
                                   String resource,
                                   IFile reference,
                                   String newPath) throws CoreException {
        Builder builder = getBuilder();

        IContainer contentRoot = context.getContentRoot();
        try {
            TextFormat.merge(resource, builder);
            Message message = builder.build();
            MessageNode node = new MessageNode(message);
            boolean changed = doUpdateReferences(contentRoot, reference, newPath, node, node);
            if (changed) {
                return node.build().toString();
            }

        } catch (ParseException e) {
            throw new CoreException(new Status(IStatus.ERROR, "com.dynamo.cr.ddfeditor", e.getMessage()));
        }
        return null;
    }

    @Override
    public String deleteReferences(ResourceRefactorContext context,
                                   String resource,
                                   IFile reference) throws CoreException {
        Builder builder = getBuilder();

        IContainer contentRoot = context.getContentRoot();
        try {
            TextFormat.merge(resource, builder);
            Message message = builder.build();
            MessageNode node = new MessageNode(message);
            boolean changed = doUpdateReferences(contentRoot, reference, "", node, node);
            if (changed) {
                return node.build().toString();
            }

        } catch (ParseException e) {
            throw new CoreException(new Status(IStatus.ERROR, "com.dynamo.cr.ddfeditor", e.getMessage()));
        }
        return null;
    }
}
