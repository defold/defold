package com.dynamo.cr.editor.wizards;

import java.util.Collection;

import org.eclipse.jface.dialogs.IDialogPage;
import org.eclipse.jface.viewers.ArrayContentProvider;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.ISelectionChangedListener;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.LabelProvider;
import org.eclipse.jface.viewers.SelectionChangedEvent;
import org.eclipse.jface.viewers.TableViewer;
import org.eclipse.jface.wizard.WizardPage;
import org.eclipse.swt.SWT;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Composite;

import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfo;

/**
 * Wizard page shown when the user has chosen car as means of
 * transport
 */
public class ConnectionWizardProjectsPageView extends WizardPage implements ConnectionWizardProjectsPagePresenter.IDisplay, ISelectionChangedListener
{
    private TableViewer projectList;
    private ConnectionWizardProjectsPagePresenter presenter;

    /**
     * Constructor for HolidayDocumentPage.
     * @param pageName
     */
    public ConnectionWizardProjectsPageView(String pageName) {
        super(pageName);
        setTitle("Open Project");
        setDescription("Select project");
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
        int ncol = 1;
        gl.numColumns = ncol;
        composite.setLayout(gl);
        projectList = new TableViewer(composite, SWT.SINGLE | SWT.V_SCROLL | SWT.BORDER);
        projectList.getTable().setLayoutData(new GridData(GridData.FILL_BOTH));
        projectList.addSelectionChangedListener(this);
        projectList.setContentProvider(new ArrayContentProvider());
        projectList.setLabelProvider(new LabelProvider() {
            @Override
            public String getText(Object element) {
                ProjectInfo p = (ProjectInfo) element;
                return p.getName();
            }

            @Override
            public Image getImage(Object element) {
                return super.getImage(element);
            }
        });

        setControl(composite);
    }

    /*
     * Process the events: when the user has entered all information
     * the wizard can be finished
     */
    /*
    public void handleEvent(Event e)
    {
        if (e.widget == projectList) {
            String[] selection = projectList.getSelection();
            if (selection.length == 0)
                presenter.onSelectProject(null);
            else
                presenter.onSelectProject(selection[0]);
        }
    }
*/
    @Override
    public boolean canFlipToNextPage()
    {
        return super.canFlipToNextPage();
    }

    @Override
    public ProjectInfo getSelectedProject() {
        ISelection selection = projectList.getSelection();
        if (selection.isEmpty())
            return null;
        else {
            IStructuredSelection structured = (IStructuredSelection) selection;
            return (ProjectInfo) structured.getFirstElement();
        }
    }

    public void setPresenter(ConnectionWizardProjectsPagePresenter presenter) {
        this.presenter = presenter;

    }

    @Override
    public void setProjectsNames(Collection<ProjectInfo> projects) {
        ProjectInfo[] a = new ProjectInfo[projects.size()];
        a = projects.toArray(a);
        projectList.setInput(a);
        projectList.refresh(true);
    }

    /*
    @Override
    public String[] getProjectNames() {
        return projectList.getItems();
    }*/

    /*
    @Override
    public void setSelected(String name) {
        projectList.setSelection(new Stru)
        int i = 0;
        for (String b : projectList.getItems()) {
            if (b.equals(name)) {
                projectList.setSelection(i);
                return;
            }
            i++;
        }
        assert false;
    }*/

    @Override
    public void selectionChanged(SelectionChangedEvent event) {
        ISelection selection = event.getSelection();
        if (selection.isEmpty())
            presenter.onSelectProject(null);
        else {
            IStructuredSelection structured = (IStructuredSelection) selection;
            presenter.onSelectProject((ProjectInfo) structured.getFirstElement());
        }

    }

}
