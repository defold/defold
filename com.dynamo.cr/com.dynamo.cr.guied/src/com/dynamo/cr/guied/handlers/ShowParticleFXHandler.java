package com.dynamo.cr.guied.handlers;

import java.util.Map;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.jface.preference.IPreferenceStore;
import org.eclipse.ui.commands.IElementUpdater;
import org.eclipse.ui.handlers.HandlerUtil;
import org.eclipse.ui.menus.UIElement;
import com.dynamo.cr.sceneed.Activator;
import com.dynamo.cr.guied.ui.ParticleFXNodeRenderer;

public class ShowParticleFXHandler extends AbstractHandler implements IElementUpdater {
    public static final String PREFERENCE_PREFIX = "showParticleFX";

    private void showParticleFX(boolean show) {
        ParticleFXNodeRenderer.setRenderOutlines(show);
    }

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        String prefsName = getPreferenceName(HandlerUtil.getActiveEditorId(event));
        boolean enabled = store.getBoolean(prefsName);
        store.setValue(prefsName, !enabled);
        showParticleFX(enabled);
        return null;
    }

    @Override
    public void updateElement(UIElement element, @SuppressWarnings("rawtypes") Map parameters) {
        IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        boolean enabled = store.getBoolean(getPreferenceName(Activator.getDefault().getWorkbench().getActiveWorkbenchWindow().getActivePage().getActiveEditor().getEditorSite().getId()));
        element.setChecked(!enabled);
        showParticleFX(!enabled);
    }

    public static String getPreferenceName(String editorId) {
        return PREFERENCE_PREFIX + "_" + editorId;
    }
}
