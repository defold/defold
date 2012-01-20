package com.dynamo.cr.editor.wizards;

import java.util.Collection;

import org.eclipse.jface.dialogs.IDialogPage;
import org.eclipse.jface.dialogs.IInputValidator;
import org.eclipse.jface.dialogs.InputDialog;
import org.eclipse.jface.window.Window;
import org.eclipse.jface.wizard.WizardPage;
import org.eclipse.swt.SWT;
import org.eclipse.swt.layout.FillLayout;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.List;
import org.eclipse.swt.widgets.Listener;

import com.dynamo.cr.editor.Activator;

/**
 * Wizard page shown when the user has chosen car as means of
 * transport
 */
public class ConnectionWizardBranchPageView extends WizardPage implements Listener, ConnectionWizardBranchPagePresenter.IDisplay
{
    private List branchList;
    private Button newBranchButton;
    private Button deleteBranchButton;
    private ConnectionWizardBranchPagePresenter presenter;

    /**
     * Constructor for HolidayDocumentPage.
     * @param pageName
     */
    public ConnectionWizardBranchPageView(String pageName) {
        super(pageName);
        setTitle("Open Project");
        setDescription("Select branch");
        setImageDescriptor(Activator.getImageDescriptor("/icons/newconnect_wizban.gif"));
    }

    /**
     * @see IDialogPage#createControl(Composite)
     */
    @Override
    public void createControl(Composite parent)
    {
        Composite composite = new Composite(parent, SWT.NONE);

        GridLayout gl = new GridLayout();
        int ncol = 2;
        gl.numColumns = ncol;
        composite.setLayout(gl);

        branchList = new List(composite, SWT.SINGLE | SWT.V_SCROLL | SWT.BORDER);
        branchList.setLayoutData(new GridData(GridData.FILL_BOTH));
        branchList.addListener(SWT.Selection, this);

        Composite buttons = new Composite(composite, SWT.NONE);
        buttons.setLayoutData(new GridData(GridData.VERTICAL_ALIGN_BEGINNING));

        buttons.setLayout(new FillLayout(SWT.VERTICAL));
        newBranchButton = new Button(buttons, SWT.PUSH);
        newBranchButton.setText("New branch");
        newBranchButton.addListener(SWT.Selection, this);

        deleteBranchButton = new Button(buttons, SWT.PUSH);
        deleteBranchButton.setText("Delete branch");
        deleteBranchButton.addListener(SWT.Selection, this);

        setControl(composite);
    }

    /*
     * Process the events: when the user has entered all information
     * the wizard can be finished
     */
    @Override
    public void handleEvent(Event e)
    {
        if (e.widget == newBranchButton)
            presenter.onNewBranch();
        else if (e.widget == deleteBranchButton)
            presenter.onDeleteBranch();
        else if (e.widget == branchList) {

            String[] selection = branchList.getSelection();
            if (selection.length == 0)
                presenter.onSelectBranch(null);
            else
                presenter.onSelectBranch(selection[0]);
        }
    }

    @Override
    public boolean canFlipToNextPage()
    {
        return false;
    }

    @Override
    public String getSelectedBranch() {
        String[] selection = branchList.getSelection();
        if (selection.length == 0)
            return null;
        else
            return selection[0];
    }

    public void setPresenter(ConnectionWizardBranchPagePresenter presenter) {
        this.presenter = presenter;

    }

    @Override
    public String openNewBranchDialog() {

        InputDialog dialog = new InputDialog(getShell(), "Create new branch", "Enter new branch name:", "", new IInputValidator() {
            @Override
            public String isValid(String newText) {
                if (newText.isEmpty()) {
                    // Error but no message, essentially disables the OK button
                    return "";
                }
                if (!newText.matches("^[\\w-]+$")) {
                    return "Only letters (a-z, A-Z), digits (0-9), underscore (_) and dash (-) are valid.";
                }
                return null;
            }
        });
        int ret = dialog.open();
        if (ret == Window.OK)
            return dialog.getValue();
        else
            return null;
    }

    @Override
    public void setNewBranchButtonEnabled(boolean enabled) {
        newBranchButton.setEnabled(enabled);
    }

    @Override
    public void setDeleteBranchButtonEnabled(boolean enabled) {
        deleteBranchButton.setEnabled(enabled);
    }

    @Override
    public boolean isNewBranchButtonEnabled() {
        return newBranchButton.isEnabled();
    }

    @Override
    public boolean isDeleteBranchButtonEnabled() {
        return deleteBranchButton.isEnabled();
    }

    @Override
    public void setBranchNames(Collection<String> branchNames) {
        branchList.removeAll();
        for (String name : branchNames) {
            branchList.add(name);
        }
    }

    @Override
    public String[] getBranchNames() {
        return branchList.getItems();
    }

    @Override
    public void setSelected(String name) {
        int i = 0;
        for (String b : branchList.getItems()) {
            if (b.equals(name)) {
                branchList.setSelection(i);
                return;
            }
            i++;
        }
        assert false;
    }

}
