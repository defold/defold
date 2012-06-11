package com.dynamo.cr.target.sign;

import org.eclipse.jface.dialogs.IDialogConstants;
import org.eclipse.jface.dialogs.TitleAreaDialog;
import org.eclipse.swt.SWT;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.FileDialog;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Combo;
import org.eclipse.jface.viewers.ArrayContentProvider;
import org.eclipse.jface.viewers.ComboViewer;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.swt.widgets.Text;
import org.eclipse.swt.events.SelectionAdapter;
import org.eclipse.swt.events.SelectionEvent;

public class SignDialog extends TitleAreaDialog implements ISignView {

    public interface IPresenter {
        public void signAndUpload();

        public void setProvisioningProfile(String profile);

        public void start();

        public void setIdentity(String firstElement);
    }

    private Text profileText;
    private Button signAndUpload;
    private ComboViewer identitiesComboViewer;
    private IPresenter presenter;

    /**
     * Create the dialog.
     *
     * @param parentShell
     */
    public SignDialog(Shell parentShell) {
        super(parentShell);
        setShellStyle(SWT.DIALOG_TRIM | SWT.RESIZE);
    }

    @Override
    public void setPresenter(IPresenter presenter) {
        this.presenter = presenter;
    }

    /**
     * Create contents of the dialog.
     *
     * @param parent
     */
    @Override
    protected Control createDialogArea(Composite parent) {
        setTitle("Sign iOS Executable"); //$NON-NLS-1$
        setMessage(Messages.SignDialog_DIALOG_MESSAGE);
        Composite area = (Composite) super.createDialogArea(parent);
        Composite container = new Composite(area, SWT.NONE);
        container.setLayout(new GridLayout(3, false));
        container.setLayoutData(new GridData(GridData.FILL_BOTH));

        Label signingIdentityLabel = new Label(container, SWT.NONE);
        signingIdentityLabel.setLayoutData(new GridData(SWT.RIGHT,
                SWT.CENTER, false, false, 1, 1));
        signingIdentityLabel.setText("Code signing identity:"); //$NON-NLS-1$

        identitiesComboViewer = new ComboViewer(container, SWT.READ_ONLY);
        identitiesComboViewer.setContentProvider(new ArrayContentProvider());
        Combo combo = identitiesComboViewer.getCombo();
        combo.addSelectionListener(new SelectionAdapter() {
            @Override
            public void widgetSelected(SelectionEvent e) {
                ISelection selection = identitiesComboViewer.getSelection();
                if (!selection.isEmpty()) {
                    IStructuredSelection structSelection = (IStructuredSelection) selection;
                    presenter.setIdentity((String) structSelection
                            .getFirstElement());
                }
            }
        });
        combo.setLayoutData(new GridData(SWT.FILL, SWT.CENTER, true, false, 1, 1));
        new Label(container, SWT.NONE);

        Label profileLabel = new Label(container, SWT.NONE);
        profileLabel.setLayoutData(new GridData(SWT.RIGHT, SWT.CENTER, false,
                false, 1, 1));
        profileLabel.setText("Provisioning profile:"); //$NON-NLS-1$

        profileText = new Text(container, SWT.BORDER);
        profileText.setEditable(false);
        profileText.setLayoutData(new GridData(SWT.FILL, SWT.CENTER, true,
                false, 1, 1));

        Button selectProfileButton = new Button(container, SWT.NONE);
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
        selectProfileButton.setText("..."); //$NON-NLS-1$

        return area;
    }

    /**
     * Create contents of the button bar.
     *
     * @param parent
     */
    @Override
    protected void createButtonsForButtonBar(Composite parent) {
        signAndUpload = createButton(parent, IDialogConstants.OK_ID,
                "Sign and Upload", true); //$NON-NLS-1$
        signAndUpload.addSelectionListener(new SelectionAdapter() {
            @Override
            public void widgetSelected(SelectionEvent e) {
                presenter.signAndUpload();
            }
        });
    }

    /**
     * Return the initial size of the dialog.
     */
    @Override
    protected Point getInitialSize() {
        return new Point(500, 311);
    }

    @Override
    public void create() {
        super.create();
        presenter.start();
    }

    @Override
    public void setEnabled(boolean enabled) {
        signAndUpload.setEnabled(enabled);
    }

    @Override
    public void setIdentities(String[] identities) {
        this.identitiesComboViewer.setInput(identities);
    }

}
