package com.dynamo.cr.parted;

import org.eclipse.ui.part.IPageBookViewPage;

import com.dynamo.cr.parted.curve.CurvePresenter;
import com.dynamo.cr.parted.curve.ICurveView;
import com.dynamo.cr.parted.views.ParticleFXCurveEditorPage;
import com.dynamo.cr.sceneed.core.IModelListener;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.ui.SceneEditor;
import com.google.inject.AbstractModule;
import com.google.inject.Singleton;

public class ParticleFXEditor extends SceneEditor {

    private IPageBookViewPage curveEditorPage;

    class Module extends AbstractModule {
        @Override
        protected void configure() {
            bind(ISceneView.IPresenter.class).to(ParticleFXEditorPresenter.class).in(Singleton.class);
            bind(IModelListener.class).to(ParticleFXEditorPresenter.class).in(Singleton.class);
            bind(IPageBookViewPage.class).to(ParticleFXCurveEditorPage.class).in(Singleton.class);
            bind(ICurveView.class).to(ParticleFXCurveEditorPage.class).in(Singleton.class);
            bind(ICurveView.IPresenter.class).to(CurvePresenter.class).in(Singleton.class);
        }
    }

    @Override
    public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
        if (adapter == IPageBookViewPage.class) {
            if (curveEditorPage == null) {
                curveEditorPage = getInjector().getInstance(IPageBookViewPage.class);
            }
            return curveEditorPage;
        } else {
            return super.getAdapter(adapter);
        }
    }

    @Override
    public String getContextID() {
        return ParticleEditorPlugin.PARTED_CONTEXT_ID;
    }

    @Override
    protected com.google.inject.Module createOverrideModule() {
        return new Module();
    }
}
