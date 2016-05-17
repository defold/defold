package com.dynamo.cr.target.bundle;

import org.eclipse.jface.dialogs.IDialogConstants;
import org.eclipse.jface.dialogs.IMessageProvider;
import org.eclipse.jface.dialogs.TitleAreaDialog;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.SelectionAdapter;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Shell;

public class BundleGenericDialog extends TitleAreaDialog implements
        IBundleGenericView {

    public interface IPresenter {
        public void start();
        public void releaseModeSelected(boolean selection);
        public void generateReportSelected(boolean selection);
    }

    private Button packageApplication;
    private IPresenter presenter;
    private Button releaseMode;
    private Button generateReport;
    private String title = "";

    public BundleGenericDialog(Shell parentShell) {
        super(parentShell);
        setShellStyle(SWT.DIALOG_TRIM | SWT.RESIZE);
    }

    @Override
    public void setPresenter(IPresenter presenter) {
        this.presenter = presenter;
    }

    @Override
    public void setWarningMessage(String msg) {
        setMessage(msg, IMessageProvider.WARNING);
    }

    @Override
    protected Control createDialogArea(Composite parent) {
        getShell().setText(this.title); //$NON-NLS-1$
        super.setTitle(title); //$NON-NLS-1$
        setMessage(Messages.BundleGenericPresenter_DIALOG_MESSAGE);
        Composite area = (Composite) super.createDialogArea(parent);
        Composite container = new Composite(area, SWT.NONE);
        GridLayout containerLayout = new GridLayout(3, false);
        containerLayout.marginRight = 6;
        containerLayout.marginLeft = 6;
        containerLayout.marginTop = 10;
        container.setLayout(containerLayout);
        container.setLayoutData(new GridData(GridData.FILL_BOTH));

        releaseMode = new Button(container, SWT.CHECK);
        releaseMode.setText("Release mode");
        releaseMode.setLayoutData(new GridData(SWT.FILL, SWT.CENTER, true, false, 3, 1));
        releaseMode.addSelectionListener(new SelectionAdapter() {
            @Override
            public void widgetSelected(SelectionEvent e) {
                presenter.releaseModeSelected(releaseMode.getSelection());
            }
        });

        generateReport = new Button(container, SWT.CHECK);
        generateReport.setText("Generate build report");
        generateReport.setLayoutData(new GridData(SWT.FILL, SWT.CENTER, true, false, 3, 1));
        generateReport.addSelectionListener(new SelectionAdapter() {
            @Override
            public void widgetSelected(SelectionEvent e) {
                presenter.generateReportSelected(generateReport.getSelection());
            }
        });

        return area;
    }

    @Override
    protected void createButtonsForButtonBar(Composite parent) {
        packageApplication = createButton(parent, IDialogConstants.OK_ID, "Package", true); //$NON-NLS-1$
    }

    @Override
    protected Point getInitialSize() {
        return new Point(500, 270);
    }

    @Override
    public void create() {
        super.create();
        presenter.start();
    }

    @Override
    public void setEnabled(boolean enabled) {
        packageApplication.setEnabled(enabled);
    }

    @Override
    public void setTitle(String title) {
        this.title = title;
        super.setTitle(title);
    }

    public static void main(String[] args) {
        Display display = Display.getDefault();
        BundleGenericDialog dialog = new BundleGenericDialog(new Shell(display));
        dialog.setPresenter(new BundleGenericPresenter(dialog));
        dialog.open();
    }

}
