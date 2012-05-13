package com.dynamo.cr.sceneed.core.operations;

import java.util.ArrayList;
import java.util.List;

import javax.vecmath.Matrix4d;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.sceneed.core.Node;

public class TransformNodeOperation extends AbstractOperation
{
    private List<Matrix4d> originalTransforms;
    private List<Matrix4d> newTransforms;
    private List<Node> nodes;

    public TransformNodeOperation(String label, List<Node> nodes, List<Matrix4d> originaTransforms, List<Matrix4d> newTransforms) {
        super(label);
        this.nodes = new ArrayList<Node>(nodes);
        this.originalTransforms = new ArrayList<Matrix4d>(originaTransforms);
        this.newTransforms = new ArrayList<Matrix4d>(newTransforms);
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info) throws ExecutionException
    {
        int i = 0;
        for (Node n : nodes) {
            n.setLocalTransform(newTransforms.get(i++));
        }
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info) throws ExecutionException
    {
        int i = 0;
        for (Node n : nodes) {
            n.setLocalTransform(newTransforms.get(i++));
        }
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info) throws ExecutionException
    {
        int i = 0;
        for (Node n : nodes) {
            n.setLocalTransform(originalTransforms.get(i++));
        }
        return Status.OK_STATUS;
    }
}
