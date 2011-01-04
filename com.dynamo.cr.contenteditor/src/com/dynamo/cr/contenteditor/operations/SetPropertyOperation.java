package com.dynamo.cr.contenteditor.operations;

import javax.xml.bind.PropertyException;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.contenteditor.scene.IProperty;
import com.dynamo.cr.contenteditor.scene.Node;

public class SetPropertyOperation extends AbstractOperation
{
    private final Node m_Node;
    private final IProperty m_Property;
    private final IProperty m_UndoProperty;
    private final Object m_OrginalValue;
    private final Object m_NewValue;

    public SetPropertyOperation(String label, Node node, IProperty property, IProperty undo_property, Object original_value, Object new_value)
    {
        super(label);
        m_Node = node;
        m_Property = property;
        m_UndoProperty = undo_property;
        m_OrginalValue = original_value;
        m_NewValue = new_value;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info) throws ExecutionException
    {
        try
        {
            m_Property.setValue(m_Node, m_NewValue);
        } catch (PropertyException e)
        {
            e.printStackTrace();
            return Status.CANCEL_STATUS;
        }
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info) throws ExecutionException
    {
        try
        {
            m_Property.setValue(m_Node, m_NewValue);
        } catch (PropertyException e)
        {
            e.printStackTrace();
            return Status.CANCEL_STATUS;
        }
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info) throws ExecutionException
    {
        try
        {
            m_UndoProperty.setValue(m_Node, m_OrginalValue);
        } catch (PropertyException e)
        {
            e.printStackTrace();
            return Status.CANCEL_STATUS;
        }
        return Status.OK_STATUS;
    }
}
