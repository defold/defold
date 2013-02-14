package com.dynamo.cr.properties;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.MultiStatus;
import org.eclipse.swt.SWT;
import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Label;

public class StatusLabel extends Composite {

    private Label label;
    private Color errorBackgroundColor;
    private Color errorTextColor;
    private Color infoBackgroundColor;
    private Color infoTextColor;

    public StatusLabel(Composite parent, int style) {
        super(parent, SWT.BORDER);
        GridLayout layout = new GridLayout();
        setLayout(layout);
        label = new Label(this, style | SWT.WRAP);
        GridData gd = new GridData(SWT.FILL, SWT.FILL, true, true);
        label.setLayoutData(gd);

        errorBackgroundColor = new Color(null, 255, 234, 232);
        errorTextColor = new Color(null, 181, 0, 0);

        infoBackgroundColor = new Color(null, 243, 243, 243);
        infoTextColor = new Color(null, 0, 0, 181);
    }

    @Override
    public void dispose() {
        super.dispose();
        errorBackgroundColor.dispose();
        errorTextColor.dispose();
        infoBackgroundColor.dispose();
        infoTextColor.dispose();
    }

    public Label getLabel() {
        return label;
    }

    public void setStatus(IStatus status) {
        String msg = null;

        if (status instanceof MultiStatus) {
            MultiStatus multiStatus = (MultiStatus) status;
            StringBuilder sb = new StringBuilder();
            IStatus[] children = multiStatus.getChildren();
            for (IStatus c : children) {
                sb.append("* ");
                sb.append(c.getMessage());
                sb.append("\n");
            }
            msg = sb.substring(0, sb.length() - 1);
        } else {
            msg = status.getMessage();
        }
        label.setText(msg);

        switch (status.getSeverity()) {
        case IStatus.ERROR:
        case IStatus.WARNING:
            label.setForeground(errorTextColor);
            label.setBackground(errorBackgroundColor);
            setBackground(errorBackgroundColor);
            break;
        case IStatus.INFO:
            label.setForeground(infoTextColor);
            label.setBackground(infoBackgroundColor);
            setBackground(infoBackgroundColor);
            break;
        default:
            assert false;
        }
    }


}
