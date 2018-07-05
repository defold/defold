package com.dynamo.cr.editor.liveupdate;

import java.util.HashMap;
import java.util.Map;

import org.eclipse.jface.dialogs.IDialogConstants;
import org.eclipse.jface.dialogs.TitleAreaDialog;
import org.eclipse.jface.viewers.ArrayContentProvider;
import org.eclipse.jface.viewers.ComboViewer;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.FocusEvent;
import org.eclipse.swt.events.FocusListener;
import org.eclipse.swt.events.SelectionAdapter;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Combo;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.DirectoryDialog;
import org.eclipse.swt.widgets.FileDialog;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.swt.widgets.Text;

import com.dynamo.bob.archive.publisher.PublisherSettings;

public class LiveUpdateDialog extends TitleAreaDialog {

    private interface IInputAction {
        public void setInput(String value);
    }

    private LiveUpdatePresenter presenter;

    private Combo modeCombo = null;
    private Combo bucketCombo = null;
    private Map<Text, IInputAction> textComponents = new HashMap<Text, IInputAction>();

    public LiveUpdateDialog(Shell parentShell) {
        super(parentShell);
        setShellStyle(SWT.DIALOG_TRIM | SWT.RESIZE);
    }

    public void setPresenter(LiveUpdatePresenter presenter) {
        this.presenter = presenter;
        this.presenter.start(this);
    }

    private Combo CreateDropdown(Composite container, final String labelText, final IInputAction action) {
        Label labelControl = new Label(container, SWT.NONE);
        labelControl.setText(labelText);

        final ComboViewer comboViewerControl = new ComboViewer(container, SWT.READ_ONLY);
        comboViewerControl.setContentProvider(new ArrayContentProvider());
        final Combo comboControl = comboViewerControl.getCombo();
        comboControl.setLayoutData(new GridData(SWT.FILL, SWT.CENTER, true, false, 2, 1));

        comboControl.addSelectionListener(new SelectionAdapter() {

            @Override
            public void widgetSelected(SelectionEvent e) {
                int index = comboControl.getSelectionIndex();
                String bucket = comboControl.getItem(index);
                action.setInput(bucket);
            }
        });

        return comboControl;
    }

    private void CreateTextInput(Composite container, final String label, boolean password, String text, final IInputAction action) {
        Label labelControl = new Label(container, SWT.NONE);
        labelControl.setText(label);

        int style = password ? SWT.BORDER | SWT.PASSWORD : SWT.BORDER;
        final Text textControl = new Text(container, style);
        textControl.setLayoutData(new GridData(SWT.FILL, SWT.CENTER, true, false, 2, 1));
        textControl.setEnabled(true);
        textControl.setText(text != null ? text : "");
        this.textComponents.put(textControl, action);
        textControl.addFocusListener(new FocusListener() {

            @Override
            public void focusLost(FocusEvent e) {
                action.setInput(textControl.getText());
            }

            @Override
            public void focusGained(FocusEvent e) {

            }
        });
    }

    private void CreateHorizontalSpacing(Composite container) {
        Label labelControl = new Label(container, SWT.NONE);
        labelControl.setLayoutData(new GridData(SWT.FILL, SWT.CENTER, true, false, 3, 1));
    }

    private void CreateFileInput(Composite container, final String label, String text, final boolean selectFile, final IInputAction action) {
        Label labelControl = new Label(container, SWT.NONE);
        labelControl.setText(label);

        final Text textControl = new Text(container, SWT.BORDER);
        textControl.setLayoutData(new GridData(SWT.FILL, SWT.CENTER, true, false, 1, 1));
        textControl.setEditable(false);
        textControl.setText(text != null ? text : "");

        Button buttonControl = new Button(container, SWT.FLAT);
        buttonControl.setText("...");
        buttonControl.setLayoutData(new GridData(SWT.LEFT, SWT.FILL, false, false, 1, 1));
        buttonControl.addSelectionListener(new SelectionAdapter() {
            @Override
            public void widgetSelected(SelectionEvent e) {
                if (selectFile) {
                    FileDialog fileDialog = new FileDialog(getShell());
                    String filepath = fileDialog.open();
                    if (filepath != null) {
                        textControl.setText(filepath);
                        action.setInput(filepath);
                    }

                } else {
                    DirectoryDialog directoryDialog = new DirectoryDialog(getShell());
                    String filepath = directoryDialog.open();
                    if (filepath != null) {
                        textControl.setText(filepath);
                        action.setInput(filepath);
                    }
                }
            }
        });
    }

    @Override
    protected Control createDialogArea(Composite parent) {
        getShell().setText("LiveUpdate Settings"); //$NON-NLS-1$
        super.setTitle("LiveUpdate Settings"); //$NON-NLS-1$
        Composite area = (Composite) super.createDialogArea(parent);
        Composite container = new Composite(area, SWT.NONE);
        GridLayout containerLayout = new GridLayout(3, false);
        containerLayout.marginRight = 6;
        containerLayout.marginLeft = 6;
        containerLayout.marginTop = 10;
        container.setLayout(containerLayout);
        container.setLayoutData(new GridData(GridData.FILL_BOTH));

        // -------------------------------------------------------------------
        // Publish mode
        // -------------------------------------------------------------------
        this.modeCombo = CreateDropdown(container, "Mode:", new IInputAction() {
            @Override
            public void setInput(String value) {
                presenter.setMode(PublisherSettings.PublishMode.valueOf(value));
            }
        });
        
        
        // -------------------------------------------------------------------
        // Amazon S3
        // -------------------------------------------------------------------
        CreateHorizontalSpacing(container);
        
        CreateTextInput(container, "(Amazon) Credential profile:", false, presenter.getAmazonCredentialProfile(), new IInputAction() {
            @Override
            public void setInput(String value) {
                presenter.setAmazonCredentialProfile(value);
            }
        });

        this.bucketCombo = CreateDropdown(container, "(Amazon) Bucket:", new IInputAction() {
            @Override
            public void setInput(String value) {
                presenter.setAmazonBucket(value);
            }
        });

        CreateTextInput(container, "(Amazon) Prefix:", false, presenter.getAmazonPrefix(), new IInputAction() {
            @Override
            public void setInput(String value) {
                presenter.setAmazonPrefix(value);
            }
        });


        // -------------------------------------------------------------------
        // Zip Archive
        // -------------------------------------------------------------------
        CreateHorizontalSpacing(container);

        CreateFileInput(container, "(Zip) Export path:", presenter.getZipFilepath(), false, new IInputAction() {
            @Override
            public void setInput(String value) {
                presenter.setZipFilepath(value);

            }
        });

        // -------------------------------------------------------------------
        // Manifest
        // -------------------------------------------------------------------
        CreateHorizontalSpacing(container);

        CreateFileInput(container, "Manifest public key:", presenter.getManifestPublicKey(), true, new IInputAction() {
            @Override
            public void setInput(String value) {
                presenter.setManifestPublicKey(value);

            }
        });

        CreateFileInput(container, "Manifest private key:", presenter.getManifestPrivateKey(), true, new IInputAction() {
            @Override
            public void setInput(String value) {
                presenter.setManifestPrivateKey(value);

            }
        });

        CreateTextInput(container, "Manifest supported versions:", false, presenter.getSupportedVersions(), new IInputAction() {
            @Override
            public void setInput(String value) {
                presenter.setSupportedVersions(value);
            }
        });

        presenter.updateMode();
        presenter.updateBuckets(false);
        return area;
    }

    @Override
    protected void createButtonsForButtonBar(Composite parent) {
        createButton(parent, IDialogConstants.CANCEL_ID, "Cancel", false); //$NON-NLS-1$
        createButton(parent, IDialogConstants.OK_ID, "Save", true); //$NON-NLS-1$
    }

    @Override
    protected void okPressed() {
        // This will ensure we save text fields even if the user has changed the text without changing focus of the component.
        for (Text text : this.textComponents.keySet()) {
            IInputAction action = this.textComponents.get(text);
            action.setInput(text.getText());
        }

        this.presenter.save();
        setReturnCode(OK);
        close();
    }

    @Override
    protected void cancelPressed() {
        setReturnCode(CANCEL);
        close();
    }


    @Override
    protected Point getInitialSize() {
        return new Point(500, 500);
    }

    @Override
    public void create() {
        super.create();
        // presenter.start();
    }

    public void info(String message) {
        this.setMessage(message, 1);
    }

    public void warning(String message) {
        this.setMessage(message, 2);
    }
    
    public void setModeSelection(String mode) {
    	if (mode != null) {
    		String[] items = this.modeCombo.getItems();
    		for (int i = 0; i < items.length; ++i) {
    			if (mode.equals(items[i])) {
    				this.modeCombo.select(i);
    				break;
    			}
    		}
    	}
    }
    
    public void addMode(String mode) {
    	if (this.modeCombo != null) {
    		this.modeCombo.add(mode);
    	}
    }

    public void setBucketSelection(String bucket) {
        if (bucket != null) {
            String[] items = this.bucketCombo.getItems();
            for (int i = 0; i < items.length; ++i) {
                if (bucket.equals(items[i])) {
                    this.bucketCombo.select(i);
                    break;
                }
            }
        }
    }

    public void addBucket(String bucket) {
        if (this.bucketCombo != null) {
            this.bucketCombo.add(bucket);
        }
    }

    public void clearBuckets() {
        if (this.bucketCombo != null) {
            this.bucketCombo.removeAll();
        }
    }

}
