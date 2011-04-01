package com.dynamo.cr.scene.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.scene.graph.Node;
import com.dynamo.cr.scene.math.Transform;

public class TransformNodeOperation extends AbstractOperation
{
    private com.dynamo.cr.scene.math.Transform[] originalLocalTransforms = null;
    private com.dynamo.cr.scene.math.Transform[] newLocalTransforms = null;
    private Node[] nodes = null;

    public TransformNodeOperation(String label, Node[] nodes, Transform[] originalLocalTransforms, Transform[] newLocalTransforms) {
        super(label);

        if (nodes != null && nodes.length > 0 && originalLocalTransforms != null && originalLocalTransforms.length == nodes.length && newLocalTransforms != null && nodes.length == newLocalTransforms.length) {
            this.nodes = new Node[nodes.length];
            this.originalLocalTransforms = new Transform[nodes.length];
            this.newLocalTransforms = new Transform[nodes.length];

            for (int i = 0; i < nodes.length; i++) {
                if (nodes[i] != null) {
                    this.nodes[i] = nodes[i];
                    this.originalLocalTransforms[i] = originalLocalTransforms[i];
                    this.newLocalTransforms[i] = newLocalTransforms[i];
                }
            }
        }
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info) throws ExecutionException
    {
        boolean success = false;
        for (int i = 0; i < this.nodes.length; i++) {
            if (this.nodes[i] != null) {
                this.nodes[i].setLocalTransform(this.newLocalTransforms[i]);
                success = true;
            }
        }
        if (!success) {
            throw new ExecutionException("No items could be transformed.");
        }
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info) throws ExecutionException
    {
        for (int i = 0; i < this.nodes.length; i++) {
            this.nodes[i].setLocalTransform(this.newLocalTransforms[i]);
        }
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info) throws ExecutionException
    {
        for (int i = 0; i < this.nodes.length; i++) {
            this.nodes[i].setLocalTransform(this.originalLocalTransforms[i]);
        }
        return Status.OK_STATUS;
    }
}
