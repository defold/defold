package com.dynamo.cr.goprot.sprite;

import java.util.List;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.StructuredSelection;

import com.dynamo.cr.goprot.core.Node;

public class RemoveAnimationOperation extends AbstractOperation {

    final private SpriteNode sprite;
    final private AnimationNode animation;
    final private IStructuredSelection oldSelection;
    final private IStructuredSelection newSelection;

    public RemoveAnimationOperation(AnimationNode animation) {
        super("Remove Component");
        this.sprite = (SpriteNode)animation.getParent();
        this.animation = animation;
        this.oldSelection = animation.getModel().getSelection();
        List<Node> children = this.sprite.getChildren();
        int index = children.indexOf(animation);
        if (index + 1 < children.size()) {
            ++index;
        } else {
            --index;
        }
        Node selected = null;
        if (index >= 0) {
            selected = children.get(index);
        } else {
            selected = this.sprite;
        }
        this.newSelection = new StructuredSelection(selected);
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.sprite.removeAnimation(this.animation);
        this.sprite.getModel().setSelection(this.newSelection);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.sprite.removeAnimation(this.animation);
        this.sprite.getModel().setSelection(this.newSelection);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.sprite.addAnimation(this.animation);
        this.sprite.getModel().setSelection(this.oldSelection);
        return Status.OK_STATUS;
    }

}
