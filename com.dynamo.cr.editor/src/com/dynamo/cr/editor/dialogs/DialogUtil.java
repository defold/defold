package com.dynamo.cr.editor.dialogs;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.dialogs.ErrorDialog;
import org.eclipse.swt.widgets.Display;

import com.dynamo.cr.editor.Activator;

public class DialogUtil {

    public static void openError(final String title, final String message,
            final Throwable e) {

        final Display display = Display.getDefault();
        ErrorDialog.openError(display.getActiveShell(), title, message,
                new Status(IStatus.ERROR, Activator.PLUGIN_ID, e.getMessage(), e));
    }

    public static void openErrorAsync(final String title, final String message,
            final Throwable e) {

        final Display display = Display.getDefault();
        display.asyncExec(new Runnable() {

            @Override
            public void run() {
                ErrorDialog.openError(display.getActiveShell(), title, message,
                        new Status(IStatus.ERROR, Activator.PLUGIN_ID, e.getMessage(), e));
            }
        });
    }

}
