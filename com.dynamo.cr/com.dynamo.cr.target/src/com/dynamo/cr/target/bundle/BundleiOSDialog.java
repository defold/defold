package com.dynamo.cr.target.bundle;

import org.eclipse.jface.dialogs.IDialogConstants;
import org.eclipse.jface.dialogs.IMessageProvider;
import org.eclipse.jface.dialogs.MessageDialog;
import org.eclipse.jface.dialogs.TitleAreaDialog;
import org.eclipse.jface.viewers.ArrayContentProvider;
import org.eclipse.jface.viewers.ComboViewer;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.swt.SWT;
import org.eclipse.swt.SWTException;
import org.eclipse.swt.events.SelectionAdapter;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.layout.FillLayout;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Combo;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.FileDialog;
import org.eclipse.swt.widgets.Group;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.swt.widgets.Text;

import com.dynamo.cr.target.bundle.BundleiOSPresenter.IconType;
import com.dynamo.cr.target.sign.IdentityLister;
import org.eclipse.swt.events.FocusAdapter;
import org.eclipse.swt.events.FocusEvent;

public class BundleiOSDialog extends TitleAreaDialog implements
        IBundleiOSView {

    public interface IPresenter {
        public void start();
        public void setIdentity(String identity);
        public void setProvisioningProfile(String profile);
        public void setApplicationName(String applicationName);
        public void setApplicationIdentifier(String applicationIdentifier);
        public void setIcon(String fileName, IconType iconType);
        public void setPrerendered(boolean preRendered);
        public void setVersion(String text);
    }

    private class SelectIconButton extends Composite implements SelectionListener
    {
        private Button button;
        private IconType iconType;

        public SelectIconButton(Composite parent, IconType iconType, int style) {
            super(parent, style);
            this.iconType = iconType;
            setLayout(new FillLayout());
            button = new Button(this, style);
            button.addSelectionListener(this);
        }

        @Override
        public void dispose() {
            super.dispose();
            button.removeSelectionListener(this);
        }

        @Override
        public void widgetSelected(SelectionEvent e) {
            FileDialog fileDialog = new FileDialog(getShell());
            fileDialog.setFilterExtensions(new String[] {"png"});
            fileDialog.setText(String.format("Select a %dx%d icon", iconType.getSize(), iconType.getSize()));
            String iconFile = fileDialog.open();
            if (iconFile != null) {
                try {
                    loadImage(iconFile);
                } catch (SWTException ex) {
                    MessageDialog.openError(getShell(), "Unable to open icon file", ex.getMessage());
                }
                presenter.setIcon(iconFile, iconType);
            }
        }

        private void loadImage(String iconFile) {
            Image image = new Image(getDisplay(), iconFile);
            int width = image.getImageData().width;
            int height = image.getImageData().height;
            if (width == iconType.getSize() && height == iconType.getSize())
            {
                if (iconType.isHigh())
                {
                    Image imagePrim = new Image(getDisplay(), image.getImageData().scaledTo(iconType.getSize() / 2, iconType.getSize() / 2));
                    image.dispose();
                    image = imagePrim;
                }
                button.setImage(image);
                button.setText("");
            }
            else
            {
                String msg = String.format("Icon dimensions %dx%d doens't match the required size %dx%d.", width, height, iconType.getSize(), iconType.getSize());
                MessageDialog.openError(getShell(), "Invalid icon dimensions", msg);
            }
        }

        @Override
        public void widgetDefaultSelected(SelectionEvent e) {
        }

        public void setText(String text) {
            button.setText(text);
        }

    }

    private Text profileText;
    private Button packageApplication;
    private ComboViewer identitiesComboViewer;
    private IPresenter presenter;
    private Text applicationName;
    private Text identifier;
    private Text version;

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

        Label applicationNameLabel = new Label(container, SWT.NONE);
        applicationNameLabel.setText("Application name:");

        applicationName = new Text(container, SWT.BORDER);
        applicationName.addFocusListener(new FocusAdapter() {
            @Override
            public void focusLost(FocusEvent e) {
                presenter.setApplicationName(applicationName.getText());
            }
        });
        applicationName.setLayoutData(new GridData(SWT.FILL, SWT.CENTER, false, false, 1, 1));
        applicationName.setText("");
        new Label(container, SWT.NONE);

        Label identifierLabel = new Label(container, SWT.NONE);
        identifierLabel.setText("Application identifier:");

        identifier = new Text(container, SWT.BORDER);
        identifier.addFocusListener(new FocusAdapter() {
            @Override
            public void focusLost(FocusEvent e) {
                presenter.setApplicationIdentifier(identifier.getText());
            }
        });
        identifier.setLayoutData(new GridData(SWT.FILL, SWT.CENTER, false, false, 1, 1));
        identifier.setText("");
        new Label(container, SWT.NONE);

        Label versionLabel = new Label(container, SWT.NONE);
        versionLabel.setText("Version:");

        version = new Text(container, SWT.BORDER);
        version.addFocusListener(new FocusAdapter() {
            @Override
            public void focusLost(FocusEvent e) {
                presenter.setVersion(version.getText());
            }
        });
        version.setText("");
        version.setLayoutData(new GridData(SWT.FILL, SWT.CENTER, true, false, 1, 1));
        new Label(container, SWT.NONE);
        new Label(container, SWT.NONE);
        new Label(container, SWT.NONE);
        new Label(container, SWT.NONE);

        Group group = new Group(container, SWT.NONE);
        GridLayout iconGroup = new GridLayout(4, false);
        iconGroup.marginLeft = 6;
        iconGroup.marginRight = 6;
        iconGroup.marginBottom = 6;
        iconGroup.marginTop = 6;
        group.setLayout(iconGroup);
        group.setLayoutData(new GridData(SWT.FILL, SWT.CENTER, true, false, 3, 1));
        group.setText("Icons");

        SelectIconButton icon57 = new SelectIconButton(group, IconType.IPHONE, SWT.FLAT);
        gd = new GridData(SWT.CENTER, SWT.CENTER, false, false, 1, 1);
        gd.widthHint = 70;
        gd.heightHint = 70;
        icon57.setLayoutData(gd);
        icon57.setText("iPhone\n57x57");

        SelectIconButton icon114 = new SelectIconButton(group, IconType.IPHONE_HIGH, SWT.FLAT);
        gd = new GridData(SWT.CENTER, SWT.CENTER, false, false, 1, 1);
        gd.widthHint = 70;
        gd.heightHint = 70;
        icon114.setLayoutData(gd);
        icon114.setText("iPhone\n114x114");

        SelectIconButton icon72 = new SelectIconButton(group, IconType.IPAD, SWT.FLAT);
        gd = new GridData(SWT.LEFT, SWT.CENTER, false, false, 1, 1);
        gd.widthHint = 85;
        gd.heightHint = 85;
        icon72.setLayoutData(gd);
        icon72.setText("iPad\n72x72");

        SelectIconButton icon144 = new SelectIconButton(group, IconType.IPAD_HIGH, SWT.FLAT);
        gd = new GridData(SWT.LEFT, SWT.CENTER, false, false, 1, 1);
        gd.widthHint = 85;
        gd.heightHint = 85;
        icon144.setLayoutData(gd);
        icon144.setText("iPad\n144x144");
        new Label(group, SWT.NONE);
        new Label(group, SWT.NONE);
        new Label(group, SWT.NONE);
        new Label(group, SWT.NONE);

        Button prerendered = new Button(group, SWT.CHECK);
        prerendered.setLayoutData(new GridData(SWT.LEFT, SWT.CENTER, false,
                false, 4, 1));
        prerendered.setText("Prerendered icons?");
        return area;
    }

    @Override
    protected void createButtonsForButtonBar(Composite parent) {
        packageApplication = createButton(parent, IDialogConstants.OK_ID, "Package", true); //$NON-NLS-1$
    }

    @Override
    protected Point getInitialSize() {
        return new Point(568, 480);
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

    @Override
    public void setVersion(String version) {
        this.version.setText(version);
    }

    public static void main(String[] args) {
        Display display = Display.getDefault();
        BundleiOSDialog dialog = new BundleiOSDialog(new Shell(display));
        dialog.setPresenter(new BundleiOSPresenter(dialog, new IdentityLister()));
        dialog.open();
    }

}
