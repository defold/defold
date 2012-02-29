package com.dynamo.cr.editor.preferences;

import org.eclipse.jface.preference.BooleanFieldEditor;
import org.eclipse.jface.preference.FieldEditorPreferencePage;
import org.eclipse.jface.preference.FileFieldEditor;
import org.eclipse.jface.preference.IPreferenceStore;
import org.eclipse.jface.preference.IntegerFieldEditor;
import org.eclipse.jface.preference.StringFieldEditor;
import org.eclipse.jface.util.PropertyChangeEvent;
import org.eclipse.ui.IWorkbench;
import org.eclipse.ui.IWorkbenchPreferencePage;

import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.core.EditorCorePlugin;

public class PreferencePage
	extends FieldEditorPreferencePage
	implements IWorkbenchPreferencePage {

	private BooleanFieldEditor customApplicationField;
    private FileFieldEditor applicationField;

    public PreferencePage() {
		super(GRID);
		setPreferenceStore(Activator.getDefault().getPreferenceStore());
		setDescription("Specify connection parameters:");
	}

	@Override
    public void createFieldEditors() {
        addField(new StringFieldEditor(PreferenceConstants.P_SERVER_URI, "Resource server URI:", getFieldEditorParent()));
        addField(new StringFieldEditor(PreferenceConstants.P_SOCKS_PROXY, "Socks proxy:", getFieldEditorParent()));
        addField(new IntegerFieldEditor(PreferenceConstants.P_SOCKS_PROXY_PORT, "Socks proxy port:", getFieldEditorParent()));
        if (!EditorCorePlugin.getPlatform().equals("win32")) {
            addField(new BooleanFieldEditor(PreferenceConstants.P_USE_LOCAL_BRANCHES, "Use local branches", getFieldEditorParent()));
        }

        customApplicationField = new BooleanFieldEditor(PreferenceConstants.P_CUSTOM_APPLICATION, "Custom application", getFieldEditorParent());
        addField(customApplicationField);
        applicationField = new FileFieldEditor(PreferenceConstants.P_APPLICATION, "Application:", getFieldEditorParent());

        IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        applicationField.setEnabled(store.getBoolean(PreferenceConstants.P_CUSTOM_APPLICATION), getFieldEditorParent());
        addField(applicationField);
	}

	@Override
	public void propertyChange(PropertyChangeEvent event) {
	    super.propertyChange(event);
	    if (event.getSource() == customApplicationField) {
	        applicationField.setEnabled(customApplicationField.getBooleanValue(), getFieldEditorParent());
	    }
	}

	/* (non-Javadoc)
	 * @see org.eclipse.ui.IWorkbenchPreferencePage#init(org.eclipse.ui.IWorkbench)
	 */
	@Override
    public void init(IWorkbench workbench) {
	}

}