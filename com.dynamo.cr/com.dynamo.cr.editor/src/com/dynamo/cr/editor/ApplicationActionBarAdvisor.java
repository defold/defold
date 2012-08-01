package com.dynamo.cr.editor;

import org.eclipse.jface.action.ActionContributionItem;
import org.eclipse.jface.action.GroupMarker;
import org.eclipse.jface.action.IAction;
import org.eclipse.jface.action.IContributionItem;
import org.eclipse.jface.action.IMenuManager;
import org.eclipse.jface.action.MenuManager;
import org.eclipse.jface.action.Separator;
import org.eclipse.ui.ISharedImages;
import org.eclipse.ui.IWorkbenchActionConstants;
import org.eclipse.ui.IWorkbenchWindow;
import org.eclipse.ui.actions.ActionFactory;
import org.eclipse.ui.actions.ActionFactory.IWorkbenchAction;
import org.eclipse.ui.actions.BaseNewWizardMenu;
import org.eclipse.ui.application.ActionBarAdvisor;
import org.eclipse.ui.application.IActionBarConfigurer;
import org.eclipse.ui.ide.IDEActionFactory;
import org.eclipse.ui.internal.ide.IDEWorkbenchMessages;
import org.eclipse.ui.menus.CommandContributionItem;
import org.eclipse.ui.menus.CommandContributionItemParameter;

public class ApplicationActionBarAdvisor extends ActionBarAdvisor {

    /*
     * NOTE: Actions and menus is loosely built on code from WorkbenchActionBuilder
     */
    private IAction undoAction;
    private IAction redoAction;
    private IAction closeAction;
    private IAction closeAllAction;
    private IAction saveAction;
    private IAction saveAsAction;
    private IAction saveAllAction;
    private IAction quitAction;
    private IAction preferencsAction;
    private IWorkbenchWindow window;
    private IWorkbenchAction cutAction;
    private IWorkbenchAction copyAction;
    private IWorkbenchAction pasteAction;
    private IWorkbenchAction newWizardAction;
    private IWorkbenchAction newWizardDropDownAction;
    private BaseNewWizardMenu newWizardMenu;

    public ApplicationActionBarAdvisor(IActionBarConfigurer configurer) {
        super(configurer);
        window = configurer.getWindowConfigurer().getWindow();
    }

    public IWorkbenchWindow getWindow() {
        return window;
    }

    protected void makeActions(IWorkbenchWindow window) {

        preferencsAction = ActionFactory.PREFERENCES.create(window);

        closeAction = ActionFactory.CLOSE.create(window);
        register(closeAction);

        closeAllAction = ActionFactory.CLOSE_ALL.create(window);
        register(closeAllAction);

        saveAction = ActionFactory.SAVE.create(window);
        register(saveAction);

        saveAsAction = ActionFactory.SAVE_AS.create(window);
        register(saveAsAction);

        saveAllAction = ActionFactory.SAVE_ALL.create(window);
        register(saveAllAction);

        quitAction = ActionFactory.QUIT.create(window);
        register(quitAction);

        undoAction = ActionFactory.UNDO.create(window);
        register(undoAction);

        redoAction = ActionFactory.REDO.create(window);
        register(redoAction);

        cutAction = ActionFactory.CUT.create(window);
        register(cutAction);

        copyAction = ActionFactory.COPY.create(window);
        register(copyAction);

        pasteAction = ActionFactory.PASTE.create(window);
        register(pasteAction);

        newWizardAction = ActionFactory.NEW.create(window);
        register(newWizardAction);

        newWizardDropDownAction = IDEActionFactory.NEW_WIZARD_DROP_DOWN
                .create(window);
        register(newWizardDropDownAction);
    }

    protected void fillMenuBar(IMenuManager menuBar) {
        menuBar.add(createFileMenu());
        menuBar.add(createEditMenu());
        menuBar.add(createNavigateMenu());
        menuBar.add(new GroupMarker(IWorkbenchActionConstants.MB_ADDITIONS));
        menuBar.add(createHelpMenu());
    }

    private MenuManager createFileMenu() {
        MenuManager menu = new MenuManager("File", IWorkbenchActionConstants.M_FILE);
        {
            // create the New submenu, using the same id for it as the New action
            String newText = IDEWorkbenchMessages.Workbench_new;
            String newId = ActionFactory.NEW.getId();
            MenuManager newMenu = new MenuManager(newText, newId);
            newMenu.setActionDefinitionId("org.eclipse.ui.file.newQuickMenu"); //$NON-NLS-1$
            newMenu.add(new Separator(newId));
            this.newWizardMenu = new BaseNewWizardMenu(getWindow(), null);
            newMenu.add(this.newWizardMenu);
            newMenu.add(new Separator(IWorkbenchActionConstants.MB_ADDITIONS));
            menu.add(newMenu);
        }

        menu.add(new GroupMarker(IWorkbenchActionConstants.FILE_START));
        menu.add(new GroupMarker(IWorkbenchActionConstants.NEW_EXT));
        menu.add(new Separator());

        menu.add(closeAction);
        menu.add(closeAllAction);
        menu.add(new GroupMarker(IWorkbenchActionConstants.CLOSE_EXT));
        menu.add(new Separator());
        menu.add(saveAction);
        menu.add(saveAsAction);
        menu.add(saveAllAction);
        menu.add(new Separator());

        menu.add(new GroupMarker(IWorkbenchActionConstants.SAVE_EXT));
        menu.add(new Separator());
        menu.add(new GroupMarker(IWorkbenchActionConstants.PRINT_EXT));
        menu.add(new Separator());
        menu.add(new GroupMarker(IWorkbenchActionConstants.OPEN_EXT));
        menu.add(new Separator());
        menu.add(new GroupMarker(IWorkbenchActionConstants.IMPORT_EXT));
        menu.add(new Separator(IWorkbenchActionConstants.MB_ADDITIONS));

        menu.add(new Separator());

        menu.add(new GroupMarker(IWorkbenchActionConstants.MRU));
        menu.add(new Separator());

        menu.add(preferencsAction);
        menu.add(new Separator());

        // If we're on OS X we shouldn't show this command in the File menu. It
        // should be invisible to the user. However, we should not remove it -
        // the carbon UI code will do a search through our menu structure
        // looking for it when Cmd-Q is invoked (or Quit is chosen from the
        // application menu.
        ActionContributionItem quitItem = new ActionContributionItem(quitAction);
        //quitItem.setVisible(!Util.isMac());
        menu.add(quitItem);
        menu.add(new GroupMarker(IWorkbenchActionConstants.FILE_END));
        return menu;
    }

    private IContributionItem getItem(String actionId, String commandId,
            String image, String disabledImage, String label, String tooltip, String helpContextId) {
        ISharedImages sharedImages = window.getWorkbench()
                .getSharedImages();

        CommandContributionItemParameter commandParm = new CommandContributionItemParameter(
                window, actionId, commandId, null, sharedImages
                        .getImageDescriptor(image), sharedImages
                        .getImageDescriptor(disabledImage), null, label, null,
                tooltip, CommandContributionItem.STYLE_PUSH, null, false);
        return new CommandContributionItem(commandParm);
    }

    private IContributionItem getFindItem() {
        return getItem(
                ActionFactory.FIND.getId(),
                ActionFactory.FIND.getCommandId(),
                null, null, "&Find/Replace...",
                "Find/Replace...", null);
    }

    private IContributionItem getCutItem() {
        return getItem(
                ActionFactory.CUT.getId(),
                ActionFactory.CUT.getCommandId(),
                ISharedImages.IMG_TOOL_CUT,
                ISharedImages.IMG_TOOL_CUT_DISABLED,
                "Cu&t",
                "Cut", null);
    }

    private IContributionItem getCopyItem() {
        return getItem(
                ActionFactory.COPY.getId(),
                ActionFactory.COPY.getCommandId(),
                ISharedImages.IMG_TOOL_COPY,
                ISharedImages.IMG_TOOL_COPY_DISABLED,
                "&Copy",
                "", null);
    }

    private IContributionItem getPasteItem() {
        return getItem(
                ActionFactory.PASTE.getId(),
                ActionFactory.PASTE.getCommandId(),
                ISharedImages.IMG_TOOL_PASTE,
                ISharedImages.IMG_TOOL_PASTE_DISABLED,
                "&Paste",
                "", null);
    }

    private IContributionItem getDeleteItem() {
        return getItem(ActionFactory.DELETE.getId(),
                ActionFactory.DELETE.getCommandId(),
                ISharedImages.IMG_TOOL_DELETE,
                ISharedImages.IMG_TOOL_DELETE_DISABLED,
                "&Delete",
                "",
                null);
    }

    private IContributionItem getSelectAllItem() {
        return getItem(
                ActionFactory.SELECT_ALL.getId(),
                ActionFactory.SELECT_ALL.getCommandId(),
                null, null, "Select &All",
                "", null);
    }

    private MenuManager createEditMenu() {
        MenuManager menu = new MenuManager("Edit", IWorkbenchActionConstants.M_EDIT);
        menu.add(new GroupMarker(IWorkbenchActionConstants.EDIT_START));

        menu.add(undoAction);
        menu.add(redoAction);
        menu.add(new GroupMarker(IWorkbenchActionConstants.UNDO_EXT));
        menu.add(new Separator());

        menu.add(getCutItem());
        menu.add(getCopyItem());
        menu.add(getPasteItem());
        menu.add(new GroupMarker(IWorkbenchActionConstants.CUT_EXT));
        menu.add(new Separator());

        menu.add(getDeleteItem());
        menu.add(getSelectAllItem());
        menu.add(new Separator());

        menu.add(getFindItem());
        menu.add(new GroupMarker(IWorkbenchActionConstants.FIND_EXT));
        menu.add(new Separator());

        menu.add(new GroupMarker(IWorkbenchActionConstants.EDIT_END));
        menu.add(new Separator(IWorkbenchActionConstants.MB_ADDITIONS));
        return menu;
    }

    private MenuManager createNavigateMenu() {
        MenuManager menu = new MenuManager("Navigate", IWorkbenchActionConstants.M_NAVIGATE);
        menu.add(new GroupMarker(IWorkbenchActionConstants.NAV_START));
        menu.add(new Separator(IWorkbenchActionConstants.MB_ADDITIONS));
        menu.add(new GroupMarker(IWorkbenchActionConstants.NAV_END));
        return menu;
    }

    private MenuManager createHelpMenu() {
        MenuManager menu = new MenuManager("&Help", IWorkbenchActionConstants.M_HELP);
        menu.add(new GroupMarker(IWorkbenchActionConstants.MB_ADDITIONS));
        return menu;
    }

}
