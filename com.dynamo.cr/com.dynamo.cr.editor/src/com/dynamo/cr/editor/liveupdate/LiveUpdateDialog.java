package com.dynamo.cr.editor.liveupdate;

import org.eclipse.jface.dialogs.IDialogConstants;
import org.eclipse.jface.dialogs.IMessageProvider;
import org.eclipse.jface.dialogs.TitleAreaDialog;
import org.eclipse.jface.resource.JFaceResources;
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
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.FileDialog;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.swt.widgets.Text;

import com.dynamo.bob.archive.publisher.PublisherSettings;

public class LiveUpdateDialog extends TitleAreaDialog {

	public enum Mode {
		EXPORT_ZIP, UPLOAD_DEFOLD, UPLOAD_AWS
	}
	
	private interface IInputAction {
		public void setInput(String value);
	}
	
	private interface IRadioAction {
		public void select();
	}
	
	private LiveUpdatePresenter presenter;
	
	private Button saveButton = null;
	private Button cancelButton = null;
	private Combo bucketCombo = null;
	
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
    
    private void CreateCheckbox(Composite container, final String label, final IInputAction action) {
    	Label labelControl = new Label(container, SWT.NONE);
    	
    	final Button buttonControl = new Button(container, SWT.CHECK);
    	buttonControl.setLayoutData(new GridData(SWT.FILL, SWT.CENTER, true, false, 2, 1));
    	buttonControl.setText(label);
    	buttonControl.setSelection(false);
    	buttonControl.addSelectionListener(new SelectionAdapter() {
    		
    		@Override
    		public void widgetSelected(SelectionEvent e) {
    			if (buttonControl.getSelection()) {
    				action.setInput("yes");
    			} else {
    				action.setInput("no");
    			}
    		}
    	});
    }
    
    private void CreateRadioButton(Composite container, final String label, boolean selected, final IRadioAction action) {
    	final Button buttonControl = new Button(container, SWT.RADIO);
    	buttonControl.setLayoutData(new GridData(SWT.FILL, SWT.CENTER, true, false, 3, 1));
    	buttonControl.setText(label);
    	buttonControl.setSelection(selected);
    	buttonControl.addSelectionListener(new SelectionAdapter() {
    		@Override
    		public void widgetDefaultSelected(SelectionEvent e) {
    			action.select();
    		}
    		
    		@Override
    		public void widgetSelected(SelectionEvent e) {
    			action.select();
    		}
    	});
    }
    
    private void CreateTextInput(Composite container, final String label, boolean password, String text, final IInputAction action) {
    	Label labelControl = new Label(container, SWT.NONE);
    	labelControl.setText(label);
    	
    	int style = password ? SWT.BORDER | SWT.PASSWORD : SWT.BORDER;
    	final Text textControl = new Text(container, style);
    	textControl.setLayoutData(new GridData(SWT.FILL, SWT.CENTER, true, false, 2, 1));
    	textControl.setEnabled(true);
    	textControl.setText(text != null ? text : "");
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
    
    private void CreateFileInput(Composite container, final String label, String text, final IInputAction action) {
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
    			FileDialog fileDialog = new FileDialog(getShell());
    			String filepath = fileDialog.open();
    			if (filepath != null) {
    				textControl.setText(filepath);
    				action.setInput(filepath);
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
        // Amazon S3
        // -------------------------------------------------------------------
        CreateRadioButton(container, "Upload to personal AWS account", true, new IRadioAction() {
        	@Override
    		public void select() {
        		presenter.setMode(PublisherSettings.PublishMode.Amazon);
        	}
        });
        
        CreateTextInput(container, "Access Key:", false, presenter.getAccessKey(), new IInputAction() {
			@Override
			public void setInput(String value) {
				presenter.setAccessKey(value);
			}
        });
        
        CreateTextInput(container, "Secret Key:", true, presenter.getSecretKey(), new IInputAction() {
			@Override
			public void setInput(String value) {
				presenter.setSecretKey(value);
			}
        });
        
        CreateCheckbox(container, "Store secret key in liveupdate.settings", new IInputAction() {
        	@Override
			public void setInput(String value) {
        		if (value == "yes") {
        			presenter.setPersistSecretKey(true);
        		} else {
        			presenter.setPersistSecretKey(false);
        		}
			}
        });
        
        this.bucketCombo = CreateDropdown(container, "Bucket:", new IInputAction() {
			@Override
			public void setInput(String value) {
				presenter.setBucket(value);
			}
        });
        presenter.updateBuckets();
        
        CreateTextInput(container, "Key Prefix:", false, presenter.getPrefix(), new IInputAction() {
			@Override
			public void setInput(String value) {
				presenter.setPrefix(value);
			}
        });

        
        // -------------------------------------------------------------------
        // Defold Service
        // -------------------------------------------------------------------
        CreateHorizontalSpacing(container);
        CreateRadioButton(container, "Upload to Defold AWS service", false, new IRadioAction() {
        	@Override
    		public void select() {
        		presenter.setMode(PublisherSettings.PublishMode.Defold);
        	}
        });

        
        // -------------------------------------------------------------------
        // Zip Archive
        // -------------------------------------------------------------------
        CreateHorizontalSpacing(container);
        CreateRadioButton(container, "Export as ZIP", false, new IRadioAction() {
        	@Override
    		public void select() {
        		presenter.setMode(PublisherSettings.PublishMode.Zip);
        	}
        });
        
        CreateFileInput(container, "Export path:", presenter.getExportPath(), new IInputAction() {	
			@Override
			public void setInput(String value) {
				presenter.setExportPath(value);
				
			}
		});
        
        return area;
    }

    @Override
    protected void createButtonsForButtonBar(Composite parent) {
        cancelButton = createButton(parent, IDialogConstants.CANCEL_ID, "Cancel", false); //$NON-NLS-1$
        saveButton = createButton(parent, IDialogConstants.OK_ID, "Save", true); //$NON-NLS-1$
    }

    @Override
    protected void okPressed() {
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
    
    public void setBucketSelection(String bucket) {
    	if (bucket != null) {
	    	String[] items = this.bucketCombo.getItems();
	    	for (int i = 0; i < items.length; ++i) {
	    		if (items[i].equals(bucket)) {
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
