package com.dynamo.cr.editor.dialogs;

import org.eclipse.jface.dialogs.IDialogConstants;
import org.eclipse.jface.dialogs.MessageDialog;
import org.eclipse.jface.dialogs.TitleAreaDialog;
import org.eclipse.jface.resource.ImageDescriptor;
import org.eclipse.jface.text.Document;
import org.eclipse.jface.text.ITextListener;
import org.eclipse.jface.text.TextEvent;
import org.eclipse.jface.text.TextViewer;
import org.eclipse.jface.viewers.IStructuredContentProvider;
import org.eclipse.jface.viewers.LabelProvider;
import org.eclipse.jface.viewers.TableViewer;
import org.eclipse.jface.viewers.TableViewerColumn;
import org.eclipse.jface.viewers.Viewer;
import org.eclipse.swt.SWT;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.Shell;

import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.CommitPresenter;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus.Status;

public class CommitDialog extends TitleAreaDialog implements ITextListener, CommitPresenter.IDisplay {

    private TextViewer commitMessageViewer;
    private String commitMessage;
    private TableViewer filesViewer;
    private CommitPresenter presenter;

    public CommitDialog(Shell parentShell) {
        super(parentShell);
    }

    public void setPresenter(CommitPresenter presenter) {
        this.presenter = presenter;
    }

    @Override
    protected boolean isResizable() {
        return true;
    }

    @Override
    protected Control createDialogArea(Composite parent) {
        Composite composite = new Composite((Composite) super.createDialogArea(parent), SWT.NONE);

        GridLayout layout = new GridLayout();
        layout.marginHeight = 8;
        layout.marginWidth = 8;
        layout.verticalSpacing = 0;
        layout.horizontalSpacing = 0;
        composite.setLayout(layout);
        composite.setLayoutData(new GridData(GridData.FILL_BOTH));

        Label label = new Label(composite, SWT.NONE);
        label.setText("Commit message:");

        new Label(composite, SWT.NONE);

        commitMessageViewer = new TextViewer(composite, SWT.MULTI | SWT.BORDER);
        GridData gd = new GridData(GridData.FILL_BOTH);
        gd.widthHint = 300;
        gd.heightHint = 200;
        commitMessageViewer.getControl().setLayoutData(gd);
        commitMessageViewer.addTextListener(this);
        commitMessageViewer.setDocument(new Document());

        filesViewer = new TableViewer(composite, SWT.SINGLE | SWT.V_SCROLL | SWT.BORDER);

        TableViewerColumn column = new TableViewerColumn(filesViewer, SWT.NONE);
        column.getColumn().setText("File");
        column.getColumn().setWidth(gd.widthHint);

        filesViewer.setLabelProvider(new LabelProvider() {
            @Override
            public String getText(Object element) {
                return ((Status) element).getName();
            }

            @Override
            public Image getImage(Object element) {
                Status s = (Status) element;
                ImageDescriptor desc = null;
                switch (s.getIndexStatus().charAt(0)) {
                    case 'A':
                        desc = Activator.getImageDescriptor("/icons/add_obj.gif");
                        break;
                    case 'D':
                        desc = Activator.getImageDescriptor("/icons/delete_obj.gif");
                        break;
                    case 'M':
                    default:
                        desc = Activator.getImageDescriptor("/icons/change_obj.gif");
                        break;
                }
                return desc.createImage();
            }
        });

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

        filesViewer.getTable().setHeaderVisible(true);
        filesViewer.getControl().setLayoutData(gd);
        filesViewer.getControl().pack();

        return composite;
    }

    @Override
    public boolean close() {
        commitMessage = commitMessageViewer.getDocument().get();
        return super.close();
    }

    @Override
    public void create() {
        super.create();
        setTitle("Commit new changes");
        setMessage("Enter commit message for your changes");
        setTitleImage(Activator.getImageDescriptor("/icons/createpatch_wizban.png").createImage());
        getButton(IDialogConstants.OK_ID).setEnabled(false);
        presenter.init();
    }

    @Override
    public void textChanged(TextEvent event) {
        presenter.onCommitMessage(commitMessageViewer.getDocument().get());
    }

    public String getCommitMessage() {
        return commitMessage;
    }

    @Override
    public void setBranchStatus(BranchStatus status) {
        filesViewer.setInput(status);
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
    public void setCommitMessageEnabled(boolean enabled) {
        commitMessageViewer.getControl().setEnabled(enabled);
    }
}
