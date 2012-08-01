package com.dynamo.cr.editor.ui.tip;

import java.util.HashMap;
import java.util.Map;

import javax.inject.Inject;

import org.eclipse.core.runtime.IConfigurationElement;
import org.eclipse.core.runtime.Platform;
import org.eclipse.jface.preference.IPreferenceStore;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.IPartListener;
import org.eclipse.ui.IPartService;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.IWorkbenchWindow;
import org.eclipse.ui.PlatformUI;

import com.dynamo.cr.editor.ui.EditorUIPlugin;
import com.dynamo.cr.editor.ui.IEditorWindow;

public class TipManager implements IPartListener, ITipManager {
    private TipControl tipControl;
    @Inject
    private IEditorWindow editorWindow;
    private Map<String, Tip> tips = new HashMap<String, Tip>();
    private IEditorPart currentTipPart = null;

    public TipManager() {
        IConfigurationElement[] config = Platform.getExtensionRegistry()
                .getConfigurationElementsFor("com.dynamo.cr.editor.ui.tip");
        for (IConfigurationElement e : config) {
            String editorId = e.getAttribute("editor-id");
            String title = e.getAttribute("title");
            String tipText = e.getAttribute("tip");
            String link = e.getAttribute("link");
            Tip tip = new Tip(editorId, title, tipText, link);
            tips.put(editorId, tip);
        }
    }

    @Override
    public void dispose() {
        IPartService partService = PlatformUI.getWorkbench().getActiveWorkbenchWindow().getPartService();
        partService.removePartListener(this);
    }

    @Override
    public Control createControl(Shell shell) {
        IPartService partService = PlatformUI.getWorkbench().getActiveWorkbenchWindow().getPartService();
        partService.addPartListener(this);

        if (tipControl != null) {
            throw new RuntimeException("createControl can only be called once");
        }
        tipControl = new TipControl(this, shell, SWT.NONE);
        return tipControl;
    }

    @Override
    public void hideTip(Tip tip) {
        currentTipPart = null;
        editorWindow.setMessageAreaVisible(false);
        if (tip != null) {
            IPreferenceStore store = EditorUIPlugin.getDefault().getPreferenceStore();
            String key = tipKey(tip);
            store.setValue(key, true);
        }
    }

    @Override
    public void partActivated(IWorkbenchPart part) {
        showTip(false);
    }

    @Override
    public void showTip(boolean force) {
        IEditorPart activeEditor = PlatformUI.getWorkbench().getActiveWorkbenchWindow().getActivePage().getActiveEditor();
        if (activeEditor != null) {
            String id = activeEditor.getSite().getId();
            if (currentTipPart != activeEditor) {
                // Potentially a new editor with tip
                Tip tip = tips.get(id);
                if (tip != null && (shouldShow(tip) || force)) {
                    this.tipControl.setTip(tip);
                    this.editorWindow.setMessageAreaVisible(true);
                    currentTipPart = activeEditor;
                } else {
                    this.editorWindow.setMessageAreaVisible(false);
                    currentTipPart = null;
                }
            }
        } else {
            this.editorWindow.setMessageAreaVisible(false);
            currentTipPart = null;
        }
    }

    private String tipKey(Tip tip) {
        String key = String.format("%s/%s", "hideTip", tip.getId());
        return key;
    }

    private boolean shouldShow(Tip tip) {
        IPreferenceStore store = EditorUIPlugin.getDefault().getPreferenceStore();
        String key = tipKey(tip);
        store.setDefault(key, false);
        return store.getBoolean(key) == false;
    }

    @Override
    public boolean tipAvailable() {
        // Might be called when workbench is starting/stopping
        // so we are carefull with null-pointers.
        IWorkbenchWindow window = PlatformUI.getWorkbench().getActiveWorkbenchWindow();
        if (window != null && window.getActivePage() != null) {
            IEditorPart activeEditor = window.getActivePage().getActiveEditor();
            return activeEditor != null && tips.containsKey(activeEditor.getEditorSite().getId());
        }
        return false;
    }

    @Override
    public void partBroughtToTop(IWorkbenchPart part) {}

    @Override
    public void partClosed(IWorkbenchPart part) {}

    @Override
    public void partDeactivated(IWorkbenchPart part) {}

    @Override
    public void partOpened(IWorkbenchPart part) {}

}
