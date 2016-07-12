package com.dynamo.cr.editor.welcome;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;

import java.net.HttpURLConnection;
import java.net.URL;
import java.net.UnknownHostException;

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

        String url = "http://www.defold.com/webviews/editor-welcome/";
        Boolean pageLoaded = false;
        if (WelcomeEditor.isInternetReachable(url)) {
            try {
                pageLoaded = this.browser.setUrl(url);

            } catch (Exception e) {
                pageLoaded = false;
            }
        }

        if (!pageLoaded) {
            InputStream input = getClass().getResourceAsStream("welcome.html");
            ByteArrayOutputStream output = new ByteArrayOutputStream();

            try {
                IOUtils.copy(input, output);
                this.browser.setText(output.toString("UTF-8"));

            } catch (IOException e) {
                IOUtils.closeQuietly(input);
                throw new RuntimeException(e);
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

    // http://stackoverflow.com/questions/7067844/how-to-detect-internet-connection-with-swt-browser-or-handle-server-not-availab
    private static boolean isInternetReachable(String weburl) {
        HttpURLConnection urlConnect = null;
        try {
            // make a URL to a known source
            URL url = new URL(weburl);
            // open a connection to that source
            urlConnect = (HttpURLConnection) url.openConnection();
            // trying to retrieve data from the source. If there is no connection, this line will fail
            urlConnect.getContent();
        } catch (UnknownHostException e) {
            return false;
        } catch (IOException e) {
            return false;
        } finally {
            // cleanup
            if(urlConnect != null) urlConnect.disconnect();
        }
        return true;
    }
}
