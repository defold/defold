package com.dynamo.cr.sceneed.ui.preferences;

import org.eclipse.jface.preference.ColorFieldEditor;
import org.eclipse.jface.preference.FieldEditorPreferencePage;
import org.eclipse.ui.IWorkbench;
import org.eclipse.ui.IWorkbenchPreferencePage;

import com.dynamo.cr.sceneed.Activator;
import com.dynamo.cr.sceneed.ui.Messages;

public class PreferencePage
extends FieldEditorPreferencePage
implements IWorkbenchPreferencePage {

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
    }

    @Override
    public void init(IWorkbench workbench) {
    }

}