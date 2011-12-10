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

public class AddAnimationNodeOperation extends AbstractSelectOperation {

    final private TileSetNode tileSet;
    final private AnimationNode animation;

    public AddAnimationNodeOperation(TileSetNode tileSet, AnimationNode animation, IPresenterContext presenterContext) {
        super(Messages.AddAnimationNodeOperation_label, animation, presenterContext);
        this.tileSet = tileSet;
        this.animation = animation;
        String id = "anim"; //$NON-NLS-1$
        id = NodeUtil.<AnimationNode>getUniqueId(this.tileSet.getAnimations(), id, new NodeUtil.IdFetcher<AnimationNode>() {
            @Override
            public String getId(AnimationNode node) {
                return node.getId();
            }
        });
        this.animation.setId(id);
    }

    @Override
    protected IStatus doExecute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.tileSet.addAnimation(this.animation);
        return Status.OK_STATUS;
    }

    @Override
    protected IStatus doRedo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.tileSet.addAnimation(this.animation);
        return Status.OK_STATUS;
    }

    @Override
    protected IStatus doUndo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.tileSet.removeAnimation(this.animation);
        return Status.OK_STATUS;
    }

}
