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
        public void setCertificate(String certificate);
        public void releaseModeSelected(boolean selection);
        public void generateReportSelected(boolean selection);
    }

    private Button packageApplication;
    private IPresenter presenter;
    private Button releaseMode;
    private Button generateReport;

    private static String persistentCertificate = null;
    private static String persistentKey = null;

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
        if (BundleAndroidDialog.persistentCertificate != null) {
            certificateText.setText(BundleAndroidDialog.persistentCertificate);
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
                    BundleAndroidDialog.persistentCertificate = certificate;
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
        if (BundleAndroidDialog.persistentKey != null) {
            keyText.setText(BundleAndroidDialog.persistentKey);
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
                    BundleAndroidDialog.persistentKey = key;
                    keyText.setText(key);
                    presenter.setKey(key);
                }
            }
        });
        selectKeyButton.setText("...");

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

}
