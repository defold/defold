package com.dynamo.cr.contenteditor.preferences;

import org.eclipse.jface.preference.FieldEditorPreferencePage;
import org.eclipse.jface.preference.IntegerFieldEditor;
import org.eclipse.jface.preference.RadioGroupFieldEditor;
import org.eclipse.jface.util.PropertyChangeEvent;
import org.eclipse.ui.IWorkbench;
import org.eclipse.ui.IWorkbenchPreferencePage;

import com.dynamo.cr.contenteditor.Activator;

public class PreferencePage
	extends FieldEditorPreferencePage
	implements IWorkbenchPreferencePage {

    private RadioGroupFieldEditor gridField;
    private IntegerFieldEditor gridSizeField;

    public PreferencePage() {
		super(GRID);
		setPreferenceStore(Activator.getDefault().getPreferenceStore());
		setDescription("Specify collection editor parameters:");
	}

	public void createFieldEditors() {
        this.gridField = new RadioGroupFieldEditor(PreferenceConstants.P_GRID, "Grid:", 2,
                new String[][] {
                {"Auto", PreferenceConstants.P_GRID_AUTO_VALUE},
                {"Manual", PreferenceConstants.P_GRID_MANUAL_VALUE}
            }, getFieldEditorParent());
        addField(this.gridField);

        this.gridSizeField = new IntegerFieldEditor(PreferenceConstants.P_GRID_SIZE, "Grid Size:", this.gridField.getRadioBoxControl(getFieldEditorParent()));
        this.gridSizeField.setValidRange(1, 1000);
        this.gridSizeField.setEnabled(false, this.gridField.getRadioBoxControl(getFieldEditorParent()));
        addField(this.gridSizeField);
	}

	@Override
	public void propertyChange(PropertyChangeEvent event) {
	    super.propertyChange(event);
	    if (event.getSource() == this.gridField) {
	        this.gridSizeField.setEnabled(event.getNewValue().equals(PreferenceConstants.P_GRID_MANUAL_VALUE), this.gridField.getRadioBoxControl(getFieldEditorParent()));
	    }
	}

    @Override
    public void init(IWorkbench workbench) {
        // TODO Auto-generated method stub

    }

}