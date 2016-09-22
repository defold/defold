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
import org.eclipse.swt.widgets.FileDialog;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.swt.widgets.Text;

public class BundleAndroidDialog extends TitleAreaDialog implements
        IBundleAndroidView {

    public interface IPresenter {
        public void start();
        public void setKey(String key);
        public void setKey(String key, boolean validate);
        public void setCertificate(String certificate);
        public void setCertificate(String certificate, boolean validate);
        public void releaseModeSelected(boolean selection);
        public void releaseModeSelected(boolean selection, boolean validate);
        public void generateReportSelected(boolean selection);
        public void generateReportSelected(boolean selection, boolean validate);
    }

    private Button packageApplication;
    private IPresenter presenter;
    private Button releaseMode;
    private Button generateReport;

    private static String persistentCertificate = null;
    private static String persistentKey = null;
    private static boolean persistentReleaseMode = false;
    private static boolean persistentGenerateReport = false;

    public BundleAndroidDialog(Shell parentShell) {
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
        getShell().setText("Package Android Application"); //$NON-NLS-1$
        setTitle("Package Android Application"); //$NON-NLS-1$
        setMessage(Messages.BundleAndroidPresenter_DIALOG_MESSAGE);
        Composite area = (Composite) super.createDialogArea(parent);
        Composite container = new Composite(area, SWT.NONE);
        GridLayout containerLayout = new GridLayout(3, false);
        containerLayout.marginRight = 6;
        containerLayout.marginLeft = 6;
        containerLayout.marginTop = 10;
        container.setLayout(containerLayout);
        container.setLayoutData(new GridData(GridData.FILL_BOTH));

        Label certificateLabel = new Label(container, SWT.NONE);
        certificateLabel.setText("Certificate:");

        final Text certificateText = new Text(container, SWT.BORDER);
        certificateText.setLayoutData(new GridData(SWT.FILL, SWT.CENTER, true, false, 1, 1));
        certificateText.setEditable(false);
        if (persistentCertificate != null) {
            certificateText.setText(persistentCertificate);
            presenter.setCertificate(persistentCertificate, false);
        }

        Button selectCertificateButton = new Button(container, SWT.FLAT);
        GridData gd = new GridData(SWT.LEFT, SWT.FILL, false, false, 1, 1);
        gd.heightHint = 1;
        selectCertificateButton.setLayoutData(gd);
        selectCertificateButton.addSelectionListener(new SelectionAdapter() {
            @Override
            public void widgetSelected(SelectionEvent e) {
                FileDialog fileDialog = new FileDialog(getShell());
                String certificate = fileDialog.open();
                if (certificate != null) {
                    persistentCertificate = certificate;
                    certificateText.setText(certificate);
                    presenter.setCertificate(certificate);
                }
            }
        });
        selectCertificateButton.setText("...");

        Label keyLabel = new Label(container, SWT.NONE);
        keyLabel.setText("Private key:");

        final Text keyText = new Text(container, SWT.BORDER);
        keyText.setLayoutData(new GridData(SWT.FILL, SWT.CENTER, true, false, 1, 1));
        keyText.setEditable(false);
        if (persistentKey != null) {
            keyText.setText(persistentKey);
            presenter.setKey(persistentKey, false);
        }

        Button selectKeyButton = new Button(container, SWT.FLAT);
        gd = new GridData(SWT.LEFT, SWT.FILL, false, false, 1, 1);
        gd.heightHint = 1;
        selectKeyButton.setLayoutData(gd);
        selectKeyButton.addSelectionListener(new SelectionAdapter() {
            @Override
            public void widgetSelected(SelectionEvent e) {
                FileDialog fileDialog = new FileDialog(getShell());
                String key = fileDialog.open();
                if (key != null) {
                    persistentKey = key;
                    keyText.setText(key);
                    presenter.setKey(key);
                }
            }
        });
        selectKeyButton.setText("...");

        releaseMode = new Button(container, SWT.CHECK);
        releaseMode.setText("Release mode");
        releaseMode.setLayoutData(new GridData(SWT.FILL, SWT.CENTER, true, false, 3, 1));
        if (persistentReleaseMode == true) {
            releaseMode.setSelection(persistentReleaseMode);
            presenter.releaseModeSelected(persistentReleaseMode, false);
        }
        releaseMode.addSelectionListener(new SelectionAdapter() {
            @Override
            public void widgetSelected(SelectionEvent e) {
                persistentReleaseMode = releaseMode.getSelection();
                presenter.releaseModeSelected(persistentReleaseMode);
            }
        });

        generateReport = new Button(container, SWT.CHECK);
        generateReport.setText("Generate build report");
        generateReport.setLayoutData(new GridData(SWT.FILL, SWT.CENTER, true, false, 3, 1));
        if (persistentGenerateReport == true) {
            generateReport.setSelection(persistentGenerateReport);
            presenter.generateReportSelected(persistentGenerateReport, false);
        }
        generateReport.addSelectionListener(new SelectionAdapter() {
            @Override
            public void widgetSelected(SelectionEvent e) {
                persistentGenerateReport = generateReport.getSelection();
                presenter.generateReportSelected(persistentGenerateReport);
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

}
