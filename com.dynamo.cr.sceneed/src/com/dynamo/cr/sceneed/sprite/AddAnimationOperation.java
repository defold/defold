package com.dynamo.cr.sceneed.sprite;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.StructuredSelection;

public class AddAnimationOperation extends AbstractOperation {

    final private SpriteNode sprite;
    final private AnimationNode animation;
    final private IStructuredSelection oldSelection;
    final private IStructuredSelection newSelection;

    public AddAnimationOperation(SpriteNode sprite) {
        super("Add Animation");
        this.sprite = sprite;
        this.animation = new AnimationNode();
        this.animation.setId("animation");
        this.oldSelection = this.sprite.getModel().getSelection();
        this.newSelection = new StructuredSelection(this.animation);
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.sprite.addAnimation(this.animation);
        this.sprite.getModel().setSelection(this.newSelection);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.sprite.addAnimation(this.animation);
        this.sprite.getModel().setSelection(this.newSelection);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.sprite.removeAnimation(this.animation);
        this.sprite.getModel().setSelection(this.oldSelection);
        return Status.OK_STATUS;
    }

}
