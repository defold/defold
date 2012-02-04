package com.dynamo.cr.sceneed.ui.preferences;

import org.eclipse.jface.preference.ColorFieldEditor;
import org.eclipse.jface.preference.FieldEditorPreferencePage;
import org.eclipse.jface.preference.IntegerFieldEditor;
import org.eclipse.jface.preference.RadioGroupFieldEditor;
import org.eclipse.ui.IWorkbench;
import org.eclipse.ui.IWorkbenchPreferencePage;

import com.dynamo.cr.sceneed.Activator;
import com.dynamo.cr.sceneed.ui.Messages;

public class PreferencePage
extends FieldEditorPreferencePage
implements IWorkbenchPreferencePage {

    private RadioGroupFieldEditor gridField;
    private IntegerFieldEditor gridSizeField;

    public PreferencePage() {
        super(GRID);
        setPreferenceStore(Activator.getDefault().getPreferenceStore());
    }

    @Override
    public void createFieldEditors() {
        final String labelFormat = "%s:";

        ColorFieldEditor topBkgdColorField = new ColorFieldEditor(PreferenceConstants.P_TOP_BKGD_COLOR,
                String.format(labelFormat, Messages.PreferencePage_TopBkgdColor),
                getFieldEditorParent());
        addField(topBkgdColorField);

        ColorFieldEditor bottomBkgdColorField = new ColorFieldEditor(PreferenceConstants.P_BOTTOM_BKGD_COLOR,
                String.format(labelFormat, Messages.PreferencePage_BottomBkgdColor),
                getFieldEditorParent());
        addField(bottomBkgdColorField);

        this.gridField = new RadioGroupFieldEditor(PreferenceConstants.P_GRID,
                String.format(labelFormat, Messages.PreferencePage_Grid),
                2,
                new String[][] {
                {Messages.PreferencePage_GridAutoValue, PreferenceConstants.P_GRID_AUTO_VALUE},
                {Messages.PreferencePage_GridManualValue, PreferenceConstants.P_GRID_MANUAL_VALUE}
            }, getFieldEditorParent());
        addField(this.gridField);

        this.gridSizeField = new IntegerFieldEditor(PreferenceConstants.P_GRID_SIZE,
                String.format(labelFormat, Messages.PreferencePage_GridSize),
                this.gridField.getRadioBoxControl(getFieldEditorParent()));
        this.gridSizeField.setValidRange(1, 1000);
        this.gridSizeField.setEnabled(false, this.gridField.getRadioBoxControl(getFieldEditorParent()));
        addField(this.gridSizeField);

        ColorFieldEditor gridColorField = new ColorFieldEditor(PreferenceConstants.P_GRID_COLOR,
                String.format(labelFormat, Messages.PreferencePage_GridColor),
                this.gridField.getRadioBoxControl(getFieldEditorParent()));
        addField(gridColorField);
    }

    @Override
    public void init(IWorkbench workbench) {
    }

}