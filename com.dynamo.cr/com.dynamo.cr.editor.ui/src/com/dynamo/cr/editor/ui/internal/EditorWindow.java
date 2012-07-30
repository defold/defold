package com.dynamo.cr.editor.ui.internal;

import javax.inject.Inject;
import javax.inject.Provider;

import org.eclipse.core.runtime.jobs.Job;
import org.eclipse.jface.layout.GridDataFactory;
import org.eclipse.jface.window.ApplicationWindow;
import org.eclipse.swt.SWT;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Decorations;
import org.eclipse.swt.widgets.Menu;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.application.IWorkbenchWindowConfigurer;

import com.dynamo.cr.editor.ui.IEditorWindow;
import com.dynamo.cr.editor.ui.SimpleProgressProvider;
import com.dynamo.cr.editor.ui.tip.ITipManager;

/**
 * Main window abstraction. Responsible for creating the top-level window.
 *
 * |---------------------------------------------
 * |                 Message Area               |
 * |--------------------------------------------|
 * |                                            |
 * |                                            |
 * |                Page composite              |
 * |                                            |
 * |                                            |
 * |--------------------------------------------|
 * | Progress |                    Status Line  |
 * |--------------------------------------------|
 *
 * @author chmu
 *
 */
public class EditorWindow implements IEditorWindow {

    // It's crucial that we use a provider here for lazy binding
    // IWorkbenchWindowConfigurer is initially null, see
    // EditorUIPlugin
    @Inject
    private Provider<IWorkbenchWindowConfigurer> windowConfigurer;
    private Shell shell;
    private Control tipControl;
    @Inject
    private ITipManager tipManager;

    public EditorWindow() {
    }


    public IWorkbenchWindowConfigurer getWindowConfigurer() {
        return windowConfigurer.get();
    }

    @Override
    public void setMessageAreaVisible(boolean visible) {
        GridData gd = (GridData) tipControl.getLayoutData();
        int newHeight = visible ? SWT.DEFAULT : 0;

        Point preferredSize = tipControl.computeSize(SWT.DEFAULT, SWT.DEFAULT, true);
        Point actualSize = tipControl.getSize();

        if (newHeight != gd.heightHint || (visible && preferredSize.y != actualSize.y)) {
            // Re-layout if visibility changed or preferred height changed
            gd.heightHint = newHeight;
            shell.layout();
        }
    }

    @Override
    public void createContents(final Shell shell) {
        this.shell = shell;
        GridLayout layout = new GridLayout(1, false);
        layout.marginWidth = 0;
        layout.marginHeight = 0;
        layout.verticalSpacing = 0;
        layout.horizontalSpacing = 0;
        shell.setLayout(layout);
        ApplicationWindow window = (ApplicationWindow) getWindowConfigurer().getWindow();
        Menu menuBar = window.getMenuBarManager().createMenuBar((Decorations) shell);
        shell.setMenuBar(menuBar);

        tipControl = tipManager.createControl(shell);
        tipControl.setLayoutData(GridDataFactory.fillDefaults().grab(true, false).align(SWT.FILL, SWT.FILL).hint(SWT.DEFAULT, 0).create());

        Control pageComposite = getWindowConfigurer().createPageComposite(shell);
        pageComposite.setLayoutData(GridDataFactory.fillDefaults().grab(true, true).align(SWT.FILL, SWT.FILL).create());

        Composite statusComposite = new Composite(shell, SWT.NONE);
        statusComposite.setLayoutData(GridDataFactory.fillDefaults().grab(true, false).align(SWT.FILL, SWT.FILL).create());
        GridLayout statusLayout = new GridLayout(2, true);
        statusComposite.setLayout(statusLayout);

        SimpleProgressProvider progressProvider = new SimpleProgressProvider();
        Job.getJobManager().setProgressProvider(progressProvider);
        Control progressControl = progressProvider.createControl(statusComposite);
        progressControl.setLayoutData(GridDataFactory.fillDefaults().grab(false, false).align(SWT.BEGINNING, SWT.CENTER).hint(120, SWT.DEFAULT).create());

        Control statusLine = getWindowConfigurer().createStatusLineControl(statusComposite);
        statusLine.setLayoutData(GridDataFactory.fillDefaults().grab(true, false).align(SWT.FILL, SWT.FILL).create());

        shell.layout(true);
        shell.pack(true);
    }

    public void dispose() {
        tipManager.dispose();
    }

}
