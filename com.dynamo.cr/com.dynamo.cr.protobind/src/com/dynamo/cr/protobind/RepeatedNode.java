package com.dynamo.cr.protobind;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import com.dynamo.cr.protobind.internal.Path;
import com.google.protobuf.Descriptors.FieldDescriptor;
import com.google.protobuf.Message;

public class RepeatedNode extends Node {
    FieldDescriptor fieldDescriptor;
    Path thisPath;
    List<Object> list;

    RepeatedNode(FieldDescriptor fieldDescriptor, List<Object> list, Path thisPath) {
        this.fieldDescriptor = fieldDescriptor;
        this.thisPath = thisPath;
        this.list = new ArrayList<Object>(list.size());
        int index = 0;
        for (Object value : list) {
            this.list.add(null);
            doSetElement(index, value);
            ++index;
        }
    }

    /**
     * Get absolute path to a field. The path is always absolute and relative to the top hierarchy node. The path is only valid for the top node
     * with {@link #getField(IPath)} and {@link #setField(IPath, Object)}
     * @param index element index
     * @return absolute to path to the field
     */
    public IPath getPathTo(int index) {
        IPath path = new Path(thisPath, new IndexElement(index, this.fieldDescriptor));
        return path;
    }

    @Override
    public IPath[] getAllPaths() {
        IPath[] ret = new IPath[this.list.size()];
        for (int i = 0; i < this.list.size(); ++i) {
            ret[i] = new Path(thisPath, new IndexElement(i, this.fieldDescriptor));
        }
        return ret;
    }

    /**
     * Build a google protocol buffer data from current state
     * @return a list of values
     */
    public Object build() {
        List<Object> ret = new ArrayList<Object>(this.list.size());

        for (Object value : this.list) {
            if (value instanceof Node) {
                Node node = (Node) value;
                ret.add(node.build());
            }
            else {
                ret.add(value);
            }
        }
        return ret;
    }

    /**
     * Get the list of repeated values. The returned list is immutable.
     * @param <T>
     * @return list of values
     */
    @SuppressWarnings("unchecked")
    public <T> List<T> getValueList() {
        // NOTE: We make a copy of the list. Required when the list is used for undo-operations, etc
        return (List<T>) Collections.unmodifiableList(new ArrayList<Object>(this.list));
    }

    void doSetElement(int index, Object value) {
        if (value instanceof Message) {
            Message message = (Message) value;
            this.list.set(index, new MessageNode(message, new Path(this.thisPath, new IndexElement(index, this.fieldDescriptor))));
        }
        else if (value instanceof MessageNode) {
            MessageNode messageNode = (MessageNode) value;
            messageNode.thisPath = new Path(this.thisPath, new IndexElement(index, this.fieldDescriptor));
            this.list.set(index, messageNode);
        }
        else {
            this.list.set(index, value);
        }
    }

    public void doAddElement(Object value) {
        if (value instanceof Message) {
            Message message = (Message) value;
            this.list.add(new MessageNode(message, new Path(this.thisPath, new IndexElement(this.list.size(), this.fieldDescriptor))));
        }
        else if (value instanceof MessageNode) {
            MessageNode messageNode = (MessageNode) value;
            messageNode.thisPath = new Path(this.thisPath, new IndexElement(this.list.size(), this.fieldDescriptor));
            this.list.add(messageNode);
        }
        else {
            this.list.add(value);
        }
    }

    public void doRemoveElement(int index) {
        this.list.remove(index);
    }

    @Override
    public String toString() {
        return this.list.toString();
    }

    /**
     * Get field descriptor for message where the repeated field is defined.
     * @return
     */
    public FieldDescriptor getFieldDescriptor() {
        return this.fieldDescriptor;
    }
}
