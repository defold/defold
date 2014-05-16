package com.dynamo.cr.sceneed.core.operations;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.sceneed.core.Node;

public class TransformNodeOperation<T> extends AbstractOperation
{
    public interface ITransformer<T> {
        T get(Node n);
        void set(Node n, T value);
    }

    private List<T> originalTransforms;
    private List<T> newTransforms;
    private List<Node> nodes;
    private ITransformer<T> transformer;

    public TransformNodeOperation(String label, ITransformer<T> transformer, List<Node> nodes,
            List<T> originaTransforms, List<T> newTransforms) {
        super(label);
        this.nodes = new ArrayList<Node>(nodes);
        this.originalTransforms = new ArrayList<T>(originaTransforms);
        this.newTransforms = new ArrayList<T>(newTransforms);
        this.transformer = transformer;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info) throws ExecutionException
    {
        int i = 0;
        for (Node n : nodes) {
            this.transformer.set(n, newTransforms.get(i++));
        }
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info) throws ExecutionException
    {
        int i = 0;
        for (Node n : nodes) {
            this.transformer.set(n, newTransforms.get(i++));
        }
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info) throws ExecutionException
    {
        int i = 0;
        for (Node n : nodes) {
            this.transformer.set(n, originalTransforms.get(i++));
        }
        return Status.OK_STATUS;
    }
}
