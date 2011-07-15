package com.dynamo.cr.editor.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.jface.preference.IPreferenceStore;

import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.preferences.PreferenceConstants;

public class SwitchOpenIDHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        String authCookie = store.getString(PreferenceConstants.P_AUTH_COOKIE);
        store.setValue(PreferenceConstants.P_AUTH_COOKIE, "");
        boolean ret = Activator.getDefault().connectProjectClient();
        if (!ret) {
            // Set back auth-cookie if connectProjectClient fails
            store.setValue(PreferenceConstants.P_AUTH_COOKIE, authCookie);
        }
        return null;
    }

}
