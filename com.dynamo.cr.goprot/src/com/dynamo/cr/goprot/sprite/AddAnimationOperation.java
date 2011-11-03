package com.dynamo.cr.goprot.sprite;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

public class AddAnimationOperation extends AbstractOperation {

    final private SpriteNode sprite;
    final private AnimationNode animation;

    public AddAnimationOperation(SpriteNode sprite, AnimationNode animation) {
        super("Add Animation");
        this.sprite = sprite;
        this.animation = animation;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.sprite.addAnimation(this.animation);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.sprite.addAnimation(this.animation);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.sprite.removeAnimation(this.animation);
        return Status.OK_STATUS;
    }

}
