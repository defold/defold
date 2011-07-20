package com.dynamo.cr.editor.preferences;

import org.eclipse.jface.preference.BooleanFieldEditor;
import org.eclipse.jface.preference.FieldEditorPreferencePage;
import org.eclipse.jface.util.PropertyChangeEvent;
import org.eclipse.ui.IWorkbench;
import org.eclipse.ui.IWorkbenchPreferencePage;

import com.dynamo.cr.editor.Activator;

public class DebugPreferencePage extends FieldEditorPreferencePage implements IWorkbenchPreferencePage {

    private BooleanFieldEditor runInDebuggerField;
    private BooleanFieldEditor autoRunDebuggerField;

    public DebugPreferencePage() {
        super(GRID);
        setPreferenceStore(Activator.getDefault().getPreferenceStore());
        setDescription("Debug settings:");
    }

    @Override
    public void init(IWorkbench workbench) {
    }

    @Override
    protected void createFieldEditors() {
        runInDebuggerField = new BooleanFieldEditor(PreferenceConstants.P_RUN_IN_DEBUGGER, "Run in debugger", getFieldEditorParent());
        addField(runInDebuggerField);

        autoRunDebuggerField = new BooleanFieldEditor(PreferenceConstants.P_AUTO_RUN_DEBUGGER, "Auto-run in debugger", getFieldEditorParent());
        addField(autoRunDebuggerField);

        autoRunDebuggerField.setEnabled(getPreferenceStore().getBoolean(PreferenceConstants.P_RUN_IN_DEBUGGER), getFieldEditorParent());

        if (Activator.getPlatform().equals("win32")) {
            runInDebuggerField.setEnabled(false, getFieldEditorParent());
            autoRunDebuggerField.setEnabled(false, getFieldEditorParent());
        }
    }

    @Override
    public void propertyChange(PropertyChangeEvent event) {
        super.propertyChange(event);
        if (event.getSource() == runInDebuggerField) {
            autoRunDebuggerField.setEnabled(runInDebuggerField.getBooleanValue(), getFieldEditorParent());
        }
    }
}
