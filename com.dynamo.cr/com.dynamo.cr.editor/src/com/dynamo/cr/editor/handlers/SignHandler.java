package com.dynamo.cr.editor.handlers;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.InvocationTargetException;
import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.core.runtime.SubProgressMonitor;
import org.eclipse.jface.dialogs.Dialog;
import org.eclipse.jface.dialogs.ErrorDialog;
import org.eclipse.jface.dialogs.ProgressMonitorDialog;
import org.eclipse.jface.operation.IRunnableWithProgress;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.client.IProjectClient;
import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.engine.Engine;
import com.dynamo.cr.target.core.TargetPlugin;
import com.dynamo.cr.target.sign.IIdentityLister;
import com.dynamo.cr.target.sign.ISignView;
import com.dynamo.cr.target.sign.IdentityLister;
import com.dynamo.cr.target.sign.SignDialog;
import com.dynamo.cr.target.sign.SignPresenter;
import com.dynamo.cr.target.sign.Signer;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;

/**
 * Sign and upload handler
 * TODO: The SignHandler is located in com.dynamo.cr.editor in order
 * to avoid cyclic dependencies between .editor and .target
 * @author chmu
 *
 */
public class SignHandler extends AbstractHandler {

    class Module extends AbstractModule {
        @Override
        protected void configure() {
            bind(ISignView.class).toInstance(view);
            bind(IIdentityLister.class).toInstance(identityLister);
        }
    }

    private SignDialog view;
    private IdentityLister identityLister;
    private SignPresenter presenter;
    private Shell shell;
    private IProjectClient projectClient;


    static class ProgressInputStream extends InputStream {

        private InputStream stream;
        private IProgressMonitor monitor;

        public ProgressInputStream(IProgressMonitor monitor, InputStream stream) {
            this.monitor = monitor;
            this.stream = stream;
        }

        @Override
        public int read() throws IOException {
            int ret =  stream.read();
            if (ret > 0)
                monitor.worked(1);
            return ret;
        }

        @Override
        public int read(byte[] b) throws IOException {
            int ret = stream.read(b);
            if (ret > 0)
                monitor.worked(ret);
            return ret;
        }
    }

    class SignRunnable implements IRunnableWithProgress {

        @Override
        public void run(IProgressMonitor monitor)
                throws InvocationTargetException, InterruptedException {

            monitor.beginTask("Signing and uploading...", 10);

            try {
                String identity = presenter.getIdentity();
                String profile = presenter.getProfile();

                Signer signer = new Signer();
                Map<String, String> properties = new HashMap<String, String>();
                properties.put("CFBundleDisplayName", "Defold");
                properties.put("CFBundleExecutable", "dmengine");

                String engine = Engine.getDefault().getEnginePath("ios");
                String ipaPath = signer.sign(identity, profile, engine, properties);
                monitor.worked(1);

                File ipaFile = new File(ipaPath);

                SubProgressMonitor sub = new SubProgressMonitor(monitor, 9);
                sub.beginTask("Uploading...", (int) ipaFile.length());
                BufferedInputStream fileStream = new BufferedInputStream(new FileInputStream(ipaPath));
                ProgressInputStream stream = new ProgressInputStream(sub, fileStream);

                projectClient.uploadEngine("ios", stream);
            } catch (Exception e) {
                String msg = e.getMessage();
                Status status = new Status(IStatus.ERROR, "com.dynamo.cr", msg);
                ErrorDialog.openError(shell, "Error signing executable", msg, status);
                TargetPlugin.getDefault().getLog().log(status);
            }
        }

    }

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        projectClient = Activator.getDefault().projectClient;

        if (projectClient == null) {
            return null;
        }

        shell = HandlerUtil.getActiveShell(event);
        view = new SignDialog(shell);
        identityLister = new IdentityLister();
        Module module = new Module();
        Injector injector = Guice.createInjector(module);
        presenter = injector.getInstance(SignPresenter.class);
        view.setPresenter(presenter);
        int ret = view.open();
        if (ret == Dialog.OK) {

            ProgressMonitorDialog dialog = new ProgressMonitorDialog(shell);
            try {
                dialog.run(true, false, new SignRunnable());
            } catch (Exception e) {
                throw new RuntimeException(e);
            }

        }
        return null;
    }

}
