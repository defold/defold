package com.dynamo.cr.parted.nodes;

import org.eclipse.jface.viewers.IStructuredSelection;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.parted.operations.AddEmitterOperation;
import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;

public class ParticleFXPresenter implements ISceneView.INodePresenter<ParticleFXNode> {


    private static Logger logger = LoggerFactory.getLogger(ParticleFXPresenter.class);
    public ParticleFXPresenter() {
    }

    public void onAddEmitter(IPresenterContext presenterContext, ILoaderContext loaderContext) {
        IStructuredSelection selection = presenterContext.getSelection();
        if (selection.isEmpty())
            return;

        Node parent = (Node) selection.getFirstElement();
        while (parent.getParent() != null) {
            parent = parent.getParent();
        }

        try {
            EmitterNode emitter = (EmitterNode) loaderContext.loadNodeFromTemplate("emitter");
            presenterContext.executeOperation(new AddEmitterOperation((ParticleFXNode) parent, emitter, presenterContext));
        } catch (Exception e) {
            logger.error("Failed to load emitter template", e);
        }
    }

}
