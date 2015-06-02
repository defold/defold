package com.dynamo.cr.target.bundle;

import org.eclipse.jface.dialogs.IDialogConstants;
import org.eclipse.jface.dialogs.IMessageProvider;
import org.eclipse.jface.dialogs.TitleAreaDialog;
import org.eclipse.jface.viewers.ArrayContentProvider;
import org.eclipse.jface.viewers.ComboViewer;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.SelectionAdapter;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Combo;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.FileDialog;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.swt.widgets.Text;

import com.dynamo.cr.target.sign.IdentityLister;

public class BundleiOSDialog extends TitleAreaDialog implements
        IBundleiOSView {

    public interface IPresenter {
        public void start();
        public void setIdentity(String identity);
        public void setProvisioningProfile(String profile);
		public void releaseModeSelected(boolean selection);
    }

    private Text profileText;
    private Button packageApplication;
    private ComboViewer identitiesComboViewer;
    private IPresenter presenter;
    private Button releaseMode;
    
    public BundleiOSDialog(Shell parentShell) {
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
        getShell().setText("Package iOS Application"); //$NON-NLS-1$
        setTitle("Package iOS Application"); //$NON-NLS-1$
        setMessage(Messages.BundleiOSPresenter_DIALOG_MESSAGE);
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

        Label signingIdentityLabel = new Label(container, SWT.NONE);
        signingIdentityLabel.setText("Code signing identity:");

        identitiesComboViewer = new ComboViewer(container, SWT.READ_ONLY);
        identitiesComboViewer.setContentProvider(new ArrayContentProvider());
        Combo signingIdentity = identitiesComboViewer.getCombo();
        signingIdentity.setLayoutData(new GridData(SWT.FILL, SWT.CENTER, true, false, 1, 1));
        signingIdentity.addSelectionListener(new SelectionAdapter() {
            @Override
            public void widgetSelected(SelectionEvent e) {
                ISelection selection = identitiesComboViewer.getSelection();
                if (!selection.isEmpty()) {
                    IStructuredSelection structSelection = (IStructuredSelection) selection;
                    presenter.setIdentity((String) structSelection.getFirstElement());
                }
            }
        });
        new Label(container, SWT.NONE);

        Label profileLabel = new Label(container, SWT.NONE);
        profileLabel.setText("Provisioning profile:");

        profileText = new Text(container, SWT.BORDER);
        profileText.setLayoutData(new GridData(SWT.FILL, SWT.CENTER, true, false, 1, 1));
        profileText.setEditable(false);

        Button selectProfileButton = new Button(container, SWT.FLAT);
        GridData gd = new GridData(SWT.LEFT, SWT.FILL, false, false, 1, 1);
        gd.heightHint = 1;
        selectProfileButton.setLayoutData(gd);
        selectProfileButton.addSelectionListener(new SelectionAdapter() {
            @Override
            public void widgetSelected(SelectionEvent e) {
                FileDialog fileDialog = new FileDialog(getShell());
                String profile = fileDialog.open();
                if (profile != null) {
                    profileText.setText(profile);
                    presenter.setProvisioningProfile(profile);
                }
            }
        });
        selectProfileButton.setText("...");

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
    public void setIdentities(String[] identities) {
        this.identitiesComboViewer.setInput(identities);
    }

    public static void main(String[] args) {
        Display display = Display.getDefault();
        BundleiOSDialog dialog = new BundleiOSDialog(new Shell(display));
        dialog.setPresenter(new BundleiOSPresenter(dialog, new IdentityLister()));
        dialog.open();
    }

}
