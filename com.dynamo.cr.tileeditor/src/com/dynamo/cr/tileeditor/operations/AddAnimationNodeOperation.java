package com.dynamo.cr.tileeditor.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.StructuredSelection;

import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.NodeUtil;
import com.dynamo.cr.tileeditor.scene.AnimationNode;
import com.dynamo.cr.tileeditor.scene.TileSetNode;

public class AddAnimationNodeOperation extends AbstractOperation {

    final private TileSetNode tileSet;
    final private AnimationNode animation;
    final private IStructuredSelection oldSelection;

    public AddAnimationNodeOperation(TileSetNode tileSet, AnimationNode animation, IPresenterContext presenterContext) {
        super(Messages.AddAnimationNodeOperation_label);
        this.tileSet = tileSet;
        this.animation = animation;
        this.oldSelection = presenterContext.getSelection();
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
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.tileSet.addAnimation(this.animation);
        this.tileSet.getModel().setSelection(new StructuredSelection(this.animation));
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.tileSet.addAnimation(this.animation);
        this.tileSet.getModel().setSelection(new StructuredSelection(this.animation));
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.tileSet.removeAnimation(this.animation);
        this.tileSet.getModel().setSelection(this.oldSelection);
        return Status.OK_STATUS;
    }

}
