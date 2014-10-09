package com.dynamo.cr.menu;

import java.util.Vector;

import org.eclipse.jface.action.IContributionItem;
import org.eclipse.jface.action.MenuManager;
import org.eclipse.ui.IWorkbenchActionConstants;
import org.eclipse.ui.IWorkbenchWindow;
import org.eclipse.ui.actions.CompoundContributionItem;

import clojure.lang.RT;
import clojure.osgi.ClojureHelper;

public class FlexibleMenuContribution extends CompoundContributionItem {
    private boolean requiredNamespace = false;

    private final String extensionPoint;
    private final IWorkbenchWindow window;
    private final String[] menuIds;

    protected String getExtensionPoint() {
        return this.extensionPoint;
    }

    public FlexibleMenuContribution(IWorkbenchWindow window, String extensionPoint) {
        this.window = window;
        this.extensionPoint = extensionPoint;
        this.menuIds = new String[0];
    }

    public FlexibleMenuContribution(IWorkbenchWindow window, String extensionPoint, String[] menuIds) {
        this.window = window;
        this.extensionPoint = extensionPoint;
        this.menuIds = menuIds;
    }

    public static CompoundContributionItem addAllMenus(IWorkbenchWindow window, String extensionPoint) {
        return new FlexibleMenuContribution(window, extensionPoint, getAvailableMenuIds());
    }

    public static CompoundContributionItem addMenu(IWorkbenchWindow window, String extensionPoint, String menuId) {
        return new FlexibleMenuContribution(window, extensionPoint, new String[] { menuId });
    }

    public static CompoundContributionItem addMenus(IWorkbenchWindow window, String extensionPoint, String[] menuIds) {
        return new FlexibleMenuContribution(window, extensionPoint, menuIds);
    }

    @Override
    public boolean isDynamic() {
        return true;
    }

    protected IContributionItem getContributedMenu(String menuId) {
        ensureNamespace("internal.ui.menus");
        try {
            Object clojureData = ClojureHelper.invoke("internal.ui.menus", "get-menu-config", menuId);
            // '(label category-id)
            String label = (String) RT.first(clojureData);
            String categoryId = (String) RT.second(clojureData);
            MenuManager menu = new MenuManager(label, IWorkbenchActionConstants.MB_ADDITIONS);
            menu.add(new FlexibleMenuItemContribution(this.window, this.extensionPoint, categoryId));
            return menu;
        } catch (IllegalStateException e) {
            return new MenuManager("BUSTED", IWorkbenchActionConstants.MB_ADDITIONS);
        }

    }

    protected IContributionItem[] getContributedMenus(String[] menuIds) {
        Vector<IContributionItem> menus = new Vector<IContributionItem>();
        for (String menuId : menuIds) {
            menus.add(getContributedMenu(menuId));
        }
        return menus.toArray(new IContributionItem[menus.size()]);
    }

    @Override
    protected IContributionItem[] getContributionItems() {
        return getContributedMenus(this.menuIds);
    }

    private static String[] getAvailableMenuIds() {
        ClojureHelper.require("internal.ui.menus");
        Object clojureData = ClojureHelper.invoke("internal.ui.menus", "get-menu-ids");

        int count = RT.count(clojureData);
        String[] results = new String[count];
        for (int i = 0; i < count; i++) {
            results[i] = (String) RT.nth(clojureData, i);
        }
        return results;
    }

    // clojure helper
    private synchronized void ensureNamespace(String ns) {
        if (!requiredNamespace) {
            ClojureHelper.require(ns);
            requiredNamespace = true;
        }
    }
}
