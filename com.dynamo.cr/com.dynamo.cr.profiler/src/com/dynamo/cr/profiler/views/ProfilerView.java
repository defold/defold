package com.dynamo.cr.profiler.views;


import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Label;
import org.eclipse.ui.part.ViewPart;

public class ProfilerView extends ViewPart {
	public static final String ID = "com.dynamo.cr.profiler.views.ProfilerView";

    private Label label;

	public void createPartControl(Composite parent) {
	    label = new Label(parent, SWT.NONE);
	    label.setText("Profiler is WIP. Reusult is currently dumped to the console");
	}

	public void setFocus() {
		label.setFocus();
	}
}
