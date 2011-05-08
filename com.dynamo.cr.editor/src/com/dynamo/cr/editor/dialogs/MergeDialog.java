package com.dynamo.cr.editor.dialogs;

import org.eclipse.jface.dialogs.IDialogConstants;
import org.eclipse.jface.dialogs.MessageDialog;
import org.eclipse.jface.dialogs.TitleAreaDialog;
import org.eclipse.jface.viewers.DecoratingLabelProvider;
import org.eclipse.jface.viewers.ILabelDecorator;
import org.eclipse.jface.viewers.ILabelProvider;
import org.eclipse.jface.viewers.IStructuredContentProvider;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.LabelProvider;
import org.eclipse.jface.viewers.TableViewer;
import org.eclipse.jface.viewers.Viewer;
import org.eclipse.swt.SWT;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.layout.FillLayout;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.PlatformUI;

import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.MergePresenter;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus.Status;
import com.dynamo.cr.protocol.proto.Protocol.ResolveStage;

public class MergeDialog extends TitleAreaDialog implements Listener, MergePresenter.IDisplay {

    private TableViewer filesViewer;
    private Button yoursButton;
    private Button theirsButton;
    private MergePresenter presenter;

    public MergeDialog(Shell parentShell) {
        super(parentShell);
    }

    @Override
    public void create() {
        super.create();
        setTitle("Merge");
        setMessage("Merge conflicting files below");
        setTitleImage(Activator.getImageDescriptor("/icons/share_wizban.png").createImage());
        getButton(IDialogConstants.OK_ID).setEnabled(false);
        setShellStyle(SWT.MIN | SWT.MAX | SWT.CLOSE | SWT.APPLICATION_MODAL);

        presenter.init();
    }

    @Override
    protected Control createDialogArea(Composite parent) {
        Composite composite = new Composite((Composite) super.createDialogArea(parent), SWT.NONE);

        GridLayout gl = new GridLayout();
        int ncol = 2;
        gl.numColumns = ncol;
        composite.setLayout(gl);
        composite.setLayoutData(new GridData(GridData.FILL_BOTH));

        filesViewer = new TableViewer(composite, SWT.SINGLE | SWT.V_SCROLL | SWT.BORDER);
        ILabelProvider labelProvider = new LabelProvider() {
            @Override
            public String getText(Object element) {
                return ((Status) element).getName();
            }

            @Override
            public Image getImage(Object element) {
                if (element instanceof Status) {
                    Status status = (Status) element;
                    if (status.getName().lastIndexOf('.') != -1) {
                        return PlatformUI.getWorkbench().getEditorRegistry().getImageDescriptor(status.getName().substring(status.getName().lastIndexOf('.'))).createImage();
                    }
                }
                return super.getImage(element);
            }
        };
        ILabelDecorator decorator = PlatformUI.getWorkbench().getDecoratorManager().getLabelDecorator();
        filesViewer.setLabelProvider(new DecoratingLabelProvider(labelProvider, decorator));

        filesViewer.setContentProvider(new IStructuredContentProvider() {

            @Override
            public void dispose() { }

            @Override
            public void inputChanged(Viewer viewer, Object oldInput,
                    Object newInput) {
            }

            @Override
            public Object[] getElements(Object inputElement) {
                BranchStatus status = (BranchStatus) inputElement;
                return status.getFileStatusList().toArray();
            }

        });


        GridData gd = new GridData(GridData.FILL_BOTH);
        gd.heightHint = 250;
        gd.widthHint = 400;
        filesViewer.getControl().setLayoutData(gd);
        filesViewer.getControl().addListener(SWT.Selection, this);

        Composite buttons = new Composite(composite, SWT.NONE);
        buttons.setLayoutData(new GridData(GridData.VERTICAL_ALIGN_BEGINNING));

        buttons.setLayout(new FillLayout(SWT.VERTICAL));
        yoursButton = new Button(buttons, SWT.PUSH);
        yoursButton.setText("My version");
        yoursButton.addListener(SWT.Selection, this);

        theirsButton = new Button(buttons, SWT.PUSH);
        theirsButton.setText("Their version");
        theirsButton.addListener(SWT.Selection, this);

        return composite;
    }

    @Override
    public void handleEvent(Event e)
    {
        IStructuredSelection selection = (IStructuredSelection) filesViewer.getSelection();

        Status status = (Status) selection.getFirstElement();
        if (e.widget == yoursButton)
            presenter.resolve(status.getName(), ResolveStage.YOURS);
        else if (e.widget == theirsButton)
            presenter.resolve(status.getName(), ResolveStage.THEIRS);
        else {
            updateButtons();
        }
    }

    void updateButtons() {
        // NOTE: The presenter should manage this...
        if (filesViewer.getSelection().isEmpty()) {
            yoursButton.setEnabled(false);
            theirsButton.setEnabled(false);
        }
        else {
            yoursButton.setEnabled(true);
            theirsButton.setEnabled(true);
        }
    }

    @Override
    public void setBranchStatus(BranchStatus status) {
        try {
            filesViewer.setInput(status);
        }
        finally {
            updateButtons();
        }
    }

    public void setPresenter(MergePresenter prestener) {
        this.presenter = prestener;
    }

    @Override
    public void setPageComplete(boolean completed) {
        getButton(IDialogConstants.OK_ID).setEnabled(completed);
    }

    @Override
    public void showExceptionDialog(Throwable e) {
        MessageDialog.openError(null, "Error", e.getMessage());
    }

    @Override
    protected boolean isResizable() {
        return true;
    }
}
