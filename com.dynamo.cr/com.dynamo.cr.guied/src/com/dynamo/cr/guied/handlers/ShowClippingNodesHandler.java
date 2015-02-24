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
import com.dynamo.cr.guied.util.Clipping;

public class ShowClippingNodesHandler extends AbstractHandler implements IElementUpdater {
    public static final String PREFERENCE_PREFIX = "showClippingNodes";

    private void showClippingNodes(boolean show) {
        Clipping.showClippingNodes(show);
    }

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        String prefsName = getPreferenceName(HandlerUtil.getActiveEditorId(event));
        boolean enabled = store.getBoolean(prefsName);
        store.setValue(prefsName, !enabled);
        showClippingNodes(enabled);
        return null;
    }

    @Override
    public void updateElement(UIElement element, @SuppressWarnings("rawtypes") Map parameters) {
        IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        boolean enabled = store.getBoolean(getPreferenceName(Activator.getDefault().getWorkbench().getActiveWorkbenchWindow().getActivePage().getActiveEditor().getEditorSite().getId()));
        element.setChecked(!enabled);
        showClippingNodes(!enabled);
    }

    public static String getPreferenceName(String editorId) {
        return PREFERENCE_PREFIX + "_" + editorId;
    }
}
