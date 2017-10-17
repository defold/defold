package com.dynamo.cr.editor.preferences;

import org.eclipse.jface.preference.BooleanFieldEditor;
import org.eclipse.jface.preference.FieldEditorPreferencePage;
import org.eclipse.jface.preference.FileFieldEditor;
import org.eclipse.jface.preference.IPreferenceStore;
import org.eclipse.jface.preference.StringFieldEditor;
import org.eclipse.jface.util.PropertyChangeEvent;
import org.eclipse.ui.IWorkbench;
import org.eclipse.ui.IWorkbenchPreferencePage;

import com.dynamo.cr.editor.Activator;

public class PreferencePage
	extends FieldEditorPreferencePage
	implements IWorkbenchPreferencePage {

	private BooleanFieldEditor customApplicationField;
    private FileFieldEditor applicationField;
    private BooleanFieldEditor anonymousLogging;
    private BooleanFieldEditor enableTextureProfiles;
    private BooleanFieldEditor askBundlingOverwrite;

    public PreferencePage() {
		super(GRID);
		setPreferenceStore(Activator.getDefault().getPreferenceStore());
	}

	@Override
    public void createFieldEditors() {
        customApplicationField = new BooleanFieldEditor(PreferenceConstants.P_CUSTOM_APPLICATION, "Custom application", getFieldEditorParent());
        addField(customApplicationField);
        applicationField = new FileFieldEditor(PreferenceConstants.P_APPLICATION, "Application:", getFieldEditorParent());

        IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        applicationField.setEnabled(store.getBoolean(PreferenceConstants.P_CUSTOM_APPLICATION), getFieldEditorParent());
        addField(applicationField);

        anonymousLogging = new BooleanFieldEditor(PreferenceConstants.P_ANONYMOUS_LOGGING, "Enable anonymous logging", getFieldEditorParent());
        addField(anonymousLogging);

        askBundlingOverwrite = new BooleanFieldEditor(PreferenceConstants.P_CHECK_BUNDLUNG_OVERWRITE, "Check if output directory exists when bundling", getFieldEditorParent());
        addField(askBundlingOverwrite);

        enableTextureProfiles = new BooleanFieldEditor(PreferenceConstants.P_TEXTURE_PROFILES, "Use compression in texture profiles", getFieldEditorParent());
        addField(enableTextureProfiles);

        addField(new BooleanFieldEditor(PreferenceConstants.P_QUIT_ON_ESC, "Quit application on Esc key press", getFieldEditorParent()));

        addField(new StringFieldEditor(PreferenceConstants.P_NATIVE_EXT_SERVER_URI, "Native Extension Server URI:", getFieldEditorParent()));
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
