package com.dynamo.cr.tileeditor.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.NodeUtil;
import com.dynamo.cr.sceneed.core.operations.AbstractSelectOperation;
import com.dynamo.cr.tileeditor.scene.AnimationNode;
import com.dynamo.cr.tileeditor.scene.TileSetNode;

public class RemoveAnimationNodeOperation extends AbstractSelectOperation {

    final private TileSetNode tileSet;
    final private AnimationNode animation;

    public RemoveAnimationNodeOperation(AnimationNode animation, IPresenterContext presenterContext) {
        super(Messages.RemoveAnimationNodeOperation_label, NodeUtil.getSelectionReplacement(animation), presenterContext);
        this.tileSet = animation.getTileSetNode();
        this.animation = animation;
    }

    @Override
    protected IStatus doExecute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.tileSet.removeAnimation(this.animation);
        return Status.OK_STATUS;
    }

    @Override
    protected IStatus doRedo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.tileSet.removeAnimation(this.animation);
        return Status.OK_STATUS;
    }

    @Override
    protected IStatus doUndo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.tileSet.addAnimation(this.animation);
        return Status.OK_STATUS;
    }

}
