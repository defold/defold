package com.dynamo.cr.protobind;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

import com.dynamo.cr.protobind.internal.Path;
import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.Descriptors.FieldDescriptor;
import com.google.protobuf.Message;
import com.google.protobuf.Message.Builder;

/**
 * Google protocol buffer message wrapper for mutable support.
 * @author chmu
 *
 */
public class MessageNode extends Node {

    Message originalMessage;
    FieldDescriptor[] fieldDescriptors;
    Map<FieldDescriptor, Object> valuesMap = new HashMap<FieldDescriptor, Object>();
    Descriptor descriptor;
    Path thisPath;

    /**
     * Creates a new MessageNode from a google protocol buffer Message
     * @param value Messge to wrap
     */
    public MessageNode(Message value) {
        this(value, new Path());
    }

    @SuppressWarnings("unchecked")
    MessageNode(Message message, Path fromPath) {
        this.thisPath = fromPath;
        this.originalMessage = message;
        this.descriptor = message.getDescriptorForType();
        List<FieldDescriptor> fieldsList = message.getDescriptorForType().getFields();
        for (FieldDescriptor fd : fieldsList) {
            Object value = message.getField(fd);

            if (value instanceof Message) {
                value = new MessageNode((Message) value, new Path(fromPath, new FieldElement(fd)));
            }
            else if (value instanceof List) {
                value = new RepeatedNode(fd, (List<Object>) value, new Path(fromPath, new FieldElement(fd)));
            }

            valuesMap.put(fd, value);
        }
        fieldDescriptors = fieldsList.toArray(new FieldDescriptor[fieldsList.size()]);
    }

    /**
     * Get absolute path to a field. The path is always absolute and relative to the top hierarchy node. The path is only valid for the top node
     * with {@link #getField(IPath)} and {@link #setField(IPath, Object)}
     * @param fieldName Field to get path to
     * @return absolute to path to the field
     */
    public IPath getPathTo(String fieldName) {
        for (FieldDescriptor fd : fieldDescriptors) {
            if (fd.getName().equals(fieldName)) {
                return new Path(thisPath, new FieldElement(fd));
            }
        }
        return null;
    }

    public IPath[] getAllPaths() {
        IPath[] ret = new IPath[fieldDescriptors.length];
        int index = 0;
        for (FieldDescriptor fd : fieldDescriptors) {
            ret[index++] = new Path(thisPath, new FieldElement(fd));
        }
        return ret;
    }

    /**
     * Get value from {@link IPath}. For non-leaf nodes, ie strings, float, etc, a wrapper node is returned.
     * {@link MessageNode} for messages and {@link RepeatedNode} for repeated nodes.
     * @param path path to get value for
     * @return the value
     */
    public Object getField(IPath path) {
        return doGetField(this, path, 0);
    }

    /**
     * Get field value in this node.
     * @param fieldName Field name to get value for
     * @throws RuntimeException if the field doesn't exists.
     * @return field value
     */
    public Object getField(String fieldName) {
        for (FieldDescriptor fd : fieldDescriptors) {
            if (fd.getName().equals(fieldName)) {
                return this.valuesMap.get(fd);
            }
        }
        throw new RuntimeException(String.format("Field %s does not exist in %s", fieldName, this.descriptor.getName()));
    }

    /**
     * Set value from {@link IPath}. The value to be set should be of the underlying google protocol buffers type,
     * eg Message for messages. Wrapped values, eg {@link MessageNode}, are not supported.
     * @param path path to set value for
     * @param value value to set
     */
    public void setField(IPath path, Object value) {
        doSetField(this, path, 0, value);
    }

    public void addRepeated(IPath path, Object value) {
        doAddRepeated(this, path, 0, value);
    }

    public void removeRepeated(IPath path) {
        doRemoveRepeated(this, path, 0);
        updateRepeatedIndices(this);
    }

    /**
     * Build a google protocol buffer message from current state
     * @return a google protocol buffer message
     */
    public Message build() {
        Builder builder = (Builder) originalMessage.newBuilderForType();

        for (FieldDescriptor fd : fieldDescriptors) {
            Object value = valuesMap.get(fd);
            if (value instanceof MessageNode) {
                MessageNode messageNode = (MessageNode) value;
                value = messageNode.build();
            }
            else if (value instanceof RepeatedNode) {
                RepeatedNode repeatedNode = (RepeatedNode) value;
                value = repeatedNode.build();
            }
            builder.setField(fd, value);
        }

        return builder.build();
    }

    static void updateRepeatedIndices(Node node) {
        if (node instanceof MessageNode) {
            MessageNode messageNode = (MessageNode) node;
            Set<FieldDescriptor> keys = messageNode.valuesMap.keySet();
            for (FieldDescriptor fieldDescriptor : keys) {
                Object value = messageNode.valuesMap.get(fieldDescriptor);
                if (value instanceof RepeatedNode) {
                    RepeatedNode repeatedNode = (RepeatedNode) value;
                    List<Object> list = ((RepeatedNode) value).getValueList();
                    for (Object subValue : list) {
                        if (subValue instanceof Node) {
                            updateRepeatedIndices((Node) subValue);
                        }
                    }
                    list = ((RepeatedNode) value).getValueList();

                    Path repeatedPath = new Path(messageNode.thisPath, new FieldElement(fieldDescriptor));
                    RepeatedNode repeated = new RepeatedNode(fieldDescriptor, repeatedNode.getValueList(), repeatedPath);
                    messageNode.valuesMap.put(fieldDescriptor, repeated);
                }
                else if (value instanceof MessageNode) {
                    updateRepeatedIndices((Node) value);
                }
            }
        }
    }

    static Object doGetField(Object object, IPath path, int pathIndex) {
        PathElement pathElement = path.element(pathIndex);
        Object value;

        if (object instanceof RepeatedNode) {
            RepeatedNode repeatedNode = (RepeatedNode) object;
            IndexElement indexElement = (IndexElement) pathElement;
            List<?> list = repeatedNode.getValueList();
            value = list.get(indexElement.index);
        }
        else {
            FieldElement fieldElement = (FieldElement) pathElement;
            MessageNode messageContainer = (MessageNode) object;
            value = messageContainer.valuesMap.get(fieldElement.fieldDescriptor);
            if (value == null) {
                throw new RuntimeException(String.format("Invalid path '%s'. No such element '%s' in '%s'",
                        path,
                        pathElement,
                        messageContainer.originalMessage.getDescriptorForType().getName()));
            }
        }

        if (path.elementCount() - 1 == pathIndex)
            return value;
        else
            return doGetField(value, path, pathIndex + 1);
    }

    @SuppressWarnings("unchecked")
    static void doSetField(Object object, IPath path, int pathIndex, Object value) {
        PathElement pathElement = path.element(pathIndex);

        if (object instanceof RepeatedNode) {
            RepeatedNode repeatedNode = (RepeatedNode) object;
            IndexElement indexElement = (IndexElement) pathElement;
            List<Object> list = repeatedNode.getValueList();

            if (path.elementCount() - 1 == pathIndex) {
                repeatedNode.doSetElement(indexElement.index, value);
            }
            else {
                doSetField(list.get(indexElement.index), path, pathIndex + 1, value);
            }
        }
        else {
            FieldElement fieldElement = (FieldElement) pathElement;
            MessageNode messageContainer = (MessageNode) object;

            if (path.elementCount() - 1 == pathIndex) {

                if (fieldElement.fieldDescriptor.isRepeated()) {
                    List<Object> list = (List<Object>) value;
                    Path repeatedPath = new Path(messageContainer.thisPath, new FieldElement(fieldElement.fieldDescriptor));
                    RepeatedNode repeated = new RepeatedNode(fieldElement.fieldDescriptor, list, repeatedPath);
                    messageContainer.valuesMap.put(fieldElement.fieldDescriptor, repeated);
                }
                else {
                    messageContainer.valuesMap.put(fieldElement.fieldDescriptor, value);
                }
            }
            else {
                Object fieldValue = messageContainer.valuesMap.get(fieldElement.fieldDescriptor);
                if (fieldValue == null) {
                    throw new RuntimeException(String.format("Invalid path '%s'. No such element '%s' in '%s'",
                                                              path,
                                                              pathElement,
                                                              messageContainer.originalMessage.getDescriptorForType().getName()));
                }
                doSetField(fieldValue, path, pathIndex + 1, value);
            }
        }
    }

    static void doAddRepeated(Object object, IPath path, int pathIndex, Object value) {
        PathElement pathElement = path.element(pathIndex);

        if (object instanceof RepeatedNode) {
            RepeatedNode repeatedNode = (RepeatedNode) object;
            IndexElement indexElement = (IndexElement) pathElement;
            List<Object> list = repeatedNode.getValueList();
            doAddRepeated(list.get(indexElement.index), path, pathIndex + 1, value);
        }
        else {
            FieldElement fieldElement = (FieldElement) pathElement;
            MessageNode messageContainer = (MessageNode) object;

            Object fieldValue = messageContainer.valuesMap.get(fieldElement.fieldDescriptor);
            if (fieldValue == null) {
                throw new RuntimeException(String.format("Invalid path '%s'. No such element '%s' in '%s'",
                                                          path,
                                                          pathElement,
                                                          messageContainer.originalMessage.getDescriptorForType().getName()));
            }

            if (path.elementCount() - 1 == pathIndex) {
                if (fieldValue instanceof RepeatedNode) {
                    RepeatedNode repeatedNode = (RepeatedNode) fieldValue;
                    repeatedNode.doAddElement(value);
                }
                else {
                    System.out.println(object);
                    throw new RuntimeException(String.format("Invalid path '%s'. Last element is not a repeated node.", path));
                }
            }
            else {
                doAddRepeated(fieldValue, path, pathIndex + 1, value);
            }
        }
    }

    static void doRemoveRepeated(Object object, IPath path, int pathIndex) {
        PathElement pathElement = path.element(pathIndex);

        if (object instanceof RepeatedNode) {
            RepeatedNode repeatedNode = (RepeatedNode) object;
            IndexElement indexElement = (IndexElement) pathElement;
            List<Object> list = repeatedNode.getValueList();
            if (path.elementCount() - 1 == pathIndex) {
                repeatedNode.doRemoveElement(indexElement.index);
            }
            else {
                doRemoveRepeated(list.get(indexElement.index), path, pathIndex + 1);
            }
        }
        else {
            FieldElement fieldElement = (FieldElement) pathElement;
            MessageNode messageContainer = (MessageNode) object;

            Object fieldValue = messageContainer.valuesMap.get(fieldElement.fieldDescriptor);
            if (fieldValue == null) {
                throw new RuntimeException(String.format("Invalid path '%s'. No such element '%s' in '%s'",
                                                          path,
                                                          pathElement,
                                                          messageContainer.originalMessage.getDescriptorForType().getName()));
            }
            doRemoveRepeated(fieldValue, path, pathIndex + 1);
        }
    }

    @Override
    public String toString() {
        return this.descriptor.getName();
    }

    public Descriptor getDescriptor() {
        return this.descriptor;
    }

}
