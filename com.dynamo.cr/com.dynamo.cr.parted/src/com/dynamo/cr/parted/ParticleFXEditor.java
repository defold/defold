package com.dynamo.cr.parted;

import com.dynamo.cr.sceneed.ui.SceneEditor;

public class ParticleFXEditor extends SceneEditor {

    private ParticleFXCurveEditorPage curveEditorPage;

    @Override
    public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
        if (adapter == ICurveEditorPage.class) {
            if (curveEditorPage == null) {
                curveEditorPage = new ParticleFXCurveEditorPage(undoHandler, redoHandler);
            }
            return curveEditorPage;
        } else {
            return super.getAdapter(adapter);
        }
    }
}
