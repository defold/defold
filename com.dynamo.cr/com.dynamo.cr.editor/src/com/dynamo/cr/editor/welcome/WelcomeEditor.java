package com.dynamo.cr.editor.welcome;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;

import org.apache.commons.io.IOUtils;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.resource.ImageDescriptor;
import org.eclipse.swt.SWT;
import org.eclipse.swt.browser.Browser;
import org.eclipse.swt.browser.LocationAdapter;
import org.eclipse.swt.browser.LocationEvent;
import org.eclipse.swt.program.Program;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.ui.IEditorInput;
import org.eclipse.ui.IEditorSite;
import org.eclipse.ui.IPersistableElement;
import org.eclipse.ui.PartInitException;
import org.eclipse.ui.part.EditorPart;

public class WelcomeEditor extends EditorPart {

    public static final String ID = "com.dynamo.cr.editor.welcomeEditor";
    private Browser browser;

    public WelcomeEditor() {
    }

    @Override
    public void doSave(IProgressMonitor monitor) {
    }

    @Override
    public void doSaveAs() {
    }

    @Override
    public void init(IEditorSite site, IEditorInput input)
            throws PartInitException {
        setSite(site);
        setInput(input);
        setTitleToolTip(input.getToolTipText());
    }

    @Override
    public boolean isDirty() {
        return false;
    }

    @Override
    public boolean isSaveAsAllowed() {
        return false;
    }

    @Override
    public void createPartControl(Composite parent) {
        this.browser = new Browser(parent, SWT.NONE);

        try {
            this.browser.setUrl("http://www.defold.com/webviews/editor-welcome/");

        } catch (Exception e) {

            InputStream input = getClass().getResourceAsStream("welcome.html");
            ByteArrayOutputStream output = new ByteArrayOutputStream();
            try {
                IOUtils.copy(input, output);
                this.browser.setText(output.toString("UTF-8"));

            } catch (IOException e2) {
                IOUtils.closeQuietly(input);
                throw new RuntimeException(e2);
            }
        }

        this.browser.addLocationListener(new LocationAdapter() {
            @Override
            public void changing(LocationEvent event) {
                System.out.println(event.location);
                event.doit = false;
                Program.launch(event.location);
            }
        });
    }

    @Override
    public void setFocus() {
        this.browser.setFocus();
    }

    public static IEditorInput getInput() {
        return new IEditorInput() {

            @Override
            public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
                return null;
            }

            @Override
            public String getToolTipText() {
                return "Welcome";
            }

            @Override
            public IPersistableElement getPersistable() {
                return null;
            }

            @Override
            public String getName() {
                return "Welcome";
            }

            @Override
            public ImageDescriptor getImageDescriptor() {
                return null;
            }

            @Override
            public boolean exists() {
                return false;
            }
        };
    }

}
