package com.dynamo.cr.editor;

import org.eclipse.jface.action.IMenuManager;
import org.eclipse.jface.action.MenuManager;
import org.eclipse.jface.action.Separator;
import org.eclipse.ui.IWorkbenchWindow;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.actions.ActionFactory;
import org.eclipse.ui.internal.navigator.resources.plugin.WorkbenchNavigatorMessages;
import org.eclipse.ui.navigator.CommonActionProvider;
import org.eclipse.ui.navigator.ICommonActionExtensionSite;
import org.eclipse.ui.navigator.ICommonMenuConstants;
import org.eclipse.ui.navigator.ICommonViewerWorkbenchSite;
import org.eclipse.ui.navigator.WizardActionGroup;


/**
 * Custom class for populating popup menu in navigator
 * Copy-paste from org.eclipse.ui.internal.navigator.resources.actions.NewActionProvider
 * with new project and examples removed
 * @author chmu
 *
 */
public class NewNavigatorActionProvider extends CommonActionProvider {

    private static final String NEW_MENU_NAME = "common.new.menu";//$NON-NLS-1$

    private ActionFactory.IWorkbenchAction showDlgAction;

    private WizardActionGroup newWizardActionGroup;

    private boolean contribute = false;

    public void init(ICommonActionExtensionSite anExtensionSite) {

        if (anExtensionSite.getViewSite() instanceof ICommonViewerWorkbenchSite) {
            IWorkbenchWindow window = ((ICommonViewerWorkbenchSite) anExtensionSite.getViewSite()).getWorkbenchWindow();
            showDlgAction = ActionFactory.NEW.create(window);
            newWizardActionGroup = new WizardActionGroup(window, PlatformUI.getWorkbench().getNewWizardRegistry(), WizardActionGroup.TYPE_NEW, anExtensionSite.getContentService());
            contribute = true;
        }
    }

    /**
     * Adds a submenu to the given menu with the name "group.new" see
     * {@link ICommonMenuConstants#GROUP_NEW}). The submenu contains the following structure:
     *
     * <ul>
     * <li>a new generic project wizard shortcut action, </li>
     * <li>a separator, </li>
     * <li>a set of context senstive wizard shortcuts (as defined by
     * <b>org.eclipse.ui.navigator.commonWizard</b>), </li>
     * <li>another separator, </li>
     * <li>a generic examples wizard shortcut action, and finally </li>
     * <li>a generic "Other" new wizard shortcut action</li>
     * </ul>
     */
    public void fillContextMenu(IMenuManager menu) {
        IMenuManager submenu = new MenuManager(
                WorkbenchNavigatorMessages.NewActionProvider_NewMenu_label,
                NEW_MENU_NAME);
        if(!contribute) {
            return;
        }
        // Add new project wizard shortcut
        submenu.add(new Separator());

        // fill the menu from the commonWizard contributions
        newWizardActionGroup.setContext(getContext());
        newWizardActionGroup.fillContextMenu(submenu);

        submenu.add(new Separator(ICommonMenuConstants.GROUP_ADDITIONS));

        // Add other ..
        submenu.add(new Separator());
        submenu.add(showDlgAction);

        // append the submenu after the GROUP_NEW group.
        menu.insertAfter(ICommonMenuConstants.GROUP_NEW, submenu);
    }

    /* (non-Javadoc)
     * @see org.eclipse.ui.actions.ActionGroup#dispose()
     */
    public void dispose() {
        if (showDlgAction!=null) {
            showDlgAction.dispose();
            showDlgAction = null;
        }
        super.dispose();
    }
}
