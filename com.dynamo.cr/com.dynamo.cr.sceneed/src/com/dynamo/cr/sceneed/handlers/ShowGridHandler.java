package com.dynamo.cr.sceneed.handlers;

import java.util.Map;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.jface.preference.IPreferenceStore;
import org.eclipse.ui.commands.IElementUpdater;
import org.eclipse.ui.menus.UIElement;

import com.dynamo.cr.sceneed.Activator;

public class ShowGridHandler extends AbstractHandler implements IElementUpdater {
    public static final String PREFERENCE_ID = "showGrid";

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        boolean enabled = store.getBoolean(PREFERENCE_ID);
        store.setValue(PREFERENCE_ID, !enabled);
        return null;
    }

    @Override
    public void updateElement(UIElement element, @SuppressWarnings("rawtypes") Map parameters) {
        IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        boolean enabled = store.getBoolean(PREFERENCE_ID);
        element.setChecked(!enabled);
    }

}
