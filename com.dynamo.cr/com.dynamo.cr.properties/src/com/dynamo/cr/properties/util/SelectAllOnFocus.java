package com.dynamo.cr.properties.util;

import org.eclipse.swt.events.FocusEvent;
import org.eclipse.swt.events.FocusListener;
import org.eclipse.swt.widgets.Text;

// A focus listener that selects all in the text field when selected.
// See DEF-923
//
public class SelectAllOnFocus implements FocusListener {
    public void focusGained(FocusEvent e) {
        if (e.getSource() instanceof Text) {
            ((Text)e.getSource()).selectAll();
        }
    }

    public void focusLost(FocusEvent e) {
    }
}
