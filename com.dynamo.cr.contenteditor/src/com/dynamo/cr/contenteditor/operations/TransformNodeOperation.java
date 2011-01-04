package com.dynamo.cr.contenteditor.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.contenteditor.math.Transform;
import com.dynamo.cr.contenteditor.scene.Node;

public class TransformNodeOperation extends AbstractOperation
{
    private final Transform[] m_OriginalLocalTransforms;
    private final Transform[] m_NewLocalTransforms;
    private final Node[] m_Nodes;

    public TransformNodeOperation(String label, Node[] nodes, Transform[] original_local_transforms, Transform[] new_local_transforms)
    {
        super(label);

        m_Nodes = new Node[nodes.length];
        m_OriginalLocalTransforms = new Transform[nodes.length];
        m_NewLocalTransforms = new Transform[nodes.length];

        for (int i = 0; i < nodes.length; i++) {
            m_Nodes[i] = nodes[i];
            m_OriginalLocalTransforms[i] = original_local_transforms[i];
            m_NewLocalTransforms[i] = new_local_transforms[i];
        }
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info) throws ExecutionException
    {
        for (int i = 0; i < m_Nodes.length; i++) {
            m_Nodes[i].setLocalTransform(m_NewLocalTransforms[i]);
        }
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info) throws ExecutionException
    {
        for (int i = 0; i < m_Nodes.length; i++) {
            m_Nodes[i].setLocalTransform(m_NewLocalTransforms[i]);
        }
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info) throws ExecutionException
    {
        for (int i = 0; i < m_Nodes.length; i++) {
            m_Nodes[i].setLocalTransform(m_OriginalLocalTransforms[i]);
        }
        return Status.OK_STATUS;
    }
}
