package com.dynamo.cr.editor.ui.tip;

import java.util.HashMap;
import java.util.Map;

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

import com.dynamo.cr.editor.ui.EditorUIPlugin;
import com.dynamo.cr.editor.ui.EditorWindow;

public class TipManager implements IPartListener {
    private TipControl tipControl;
    private EditorWindow editorWindow;
    private IPartService partService;
    private Map<String, Tip> tips = new HashMap<String, Tip>();
    private IEditorPart currentTipPart = null;

    public TipManager(EditorWindow editorWindow, IPartService partService) {
        this.editorWindow = editorWindow;
        this.partService = partService;
        partService.addPartListener(this);

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

    public void dispose() {
        partService.removePartListener(this);
    }

    public Control createControl(Shell shell) {
        if (tipControl != null) {
            throw new RuntimeException("createControl can only be called once");
        }
        tipControl = new TipControl(this, shell, SWT.NONE);
        return tipControl;
    }

    public void hideTip(Tip tip) {
        editorWindow.setMessageAreaVisible(false);
        if (tip != null) {
            IPreferenceStore store = EditorUIPlugin.getDefault().getPreferenceStore();
            String key = tipKey(tip);
            store.setValue(key, true);
        }
    }

    @Override
    public void partActivated(IWorkbenchPart part) {
        String id = part.getSite().getId();
        IEditorPart activeEditor = part.getSite().getPage().getActiveEditor();
        if (activeEditor != null) {
            if (currentTipPart != activeEditor) {
                // Potentially a new editor with tip
                Tip tip = tips.get(id);
                if (tip != null && shouldShow(tip)) {
                    this.tipControl.setTip(tip);
                    this.editorWindow.setMessageAreaVisible(true);
                    currentTipPart = (IEditorPart) part;
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
    public void partBroughtToTop(IWorkbenchPart part) {}

    @Override
    public void partClosed(IWorkbenchPart part) {}

    @Override
    public void partDeactivated(IWorkbenchPart part) {}

    @Override
    public void partOpened(IWorkbenchPart part) {}

}
