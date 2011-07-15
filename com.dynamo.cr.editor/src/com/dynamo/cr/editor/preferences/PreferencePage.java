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

public class PreferencePage
	extends FieldEditorPreferencePage
	implements IWorkbenchPreferencePage {

	private BooleanFieldEditor downloadApplicationField;
    private FileFieldEditor applicationField;

    public PreferencePage() {
		super(GRID);
		setPreferenceStore(Activator.getDefault().getPreferenceStore());
		setDescription("Specify connection parameters:");
	}

	public void createFieldEditors() {
        addField(new StringFieldEditor(PreferenceConstants.P_SERVER_URI, "Resource server URI:", getFieldEditorParent()));
        addField(new StringFieldEditor(PreferenceConstants.P_SOCKSPROXY, "Socks proxy:", getFieldEditorParent()));
        addField(new IntegerFieldEditor(PreferenceConstants.P_SOCKSPROXYPORT, "Socks proxy port:", getFieldEditorParent()));

        downloadApplicationField = new BooleanFieldEditor(PreferenceConstants.P_DOWNLOADAPPLICATION, "Download application:", getFieldEditorParent());
        addField(downloadApplicationField);
        applicationField = new FileFieldEditor(PreferenceConstants.P_APPLICATION, "Application:", getFieldEditorParent());

        IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        applicationField.setEnabled(!store.getBoolean(PreferenceConstants.P_DOWNLOADAPPLICATION), getFieldEditorParent());
        addField(applicationField);
	}

	@Override
	public void propertyChange(PropertyChangeEvent event) {
	    super.propertyChange(event);
	    if (event.getSource() == downloadApplicationField) {
	        applicationField.setEnabled(!downloadApplicationField.getBooleanValue(), getFieldEditorParent());
	    }
	}

	/* (non-Javadoc)
	 * @see org.eclipse.ui.IWorkbenchPreferencePage#init(org.eclipse.ui.IWorkbench)
	 */
	public void init(IWorkbench workbench) {
	}

}