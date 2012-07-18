package com.dynamo.cr.profiler.handlers;

import java.io.IOException;
import java.lang.reflect.InvocationTargetException;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.operation.IRunnableWithProgress;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.progress.IProgressService;

import com.dynamo.cr.profiler.core.ProfileData;
import com.dynamo.cr.profiler.core.ProfileData.Counter;
import com.dynamo.cr.profiler.core.ProfileData.Frame;
import com.dynamo.cr.profiler.core.ProfileData.Scope;
import com.dynamo.cr.profiler.core.Profiler;
import com.dynamo.cr.profiler.core.ProfilerException;
import com.dynamo.cr.target.core.ITarget;
import com.dynamo.cr.target.core.TargetPlugin;

/**
 * Our sample handler extends AbstractHandler, an IHandler base class.
 *
 * @see org.eclipse.core.commands.IHandler
 * @see org.eclipse.core.commands.AbstractHandler
 */
public class ProfilerHandler extends AbstractHandler {
    /**
     * The constructor.
     */
    public ProfilerHandler() {
    }

    /**
     * the command has been executed, so extract extract the needed information
     * from the application context.
     */
    public Object execute(ExecutionEvent event) throws ExecutionException {

        final Profiler profiler = new Profiler();

        ITarget target = TargetPlugin.getDefault().getTargetsService().getSelectedTarget();
        final String url = target.getUrl();
        if (url == null) {
            return null;
        }

        IProgressService service = PlatformUI.getWorkbench()
                .getProgressService();
        try {
            service.run(true, true, new IRunnableWithProgress() {

                @Override
                public void run(IProgressMonitor monitor)
                        throws InvocationTargetException, InterruptedException {
                    monitor.beginTask("Capturing profile", IProgressMonitor.UNKNOWN);
                    try {
                        profiler.captureFrame(url);
                        profiler.captureStrings(url);
                    } catch (IOException e) {
                        throw new InvocationTargetException(e);
                    }
                }
            });
        } catch (Exception e) {
            throw new ExecutionException("Failed to capture profile data", e);
        }

        ProfileData profileData;
        try {
            profileData = profiler.parse();
            Frame[] frames = profileData.getFrames();
            for (Frame frame : frames) {

                System.out.println("****** Samples ******");
                ProfileData.Sample[] samples = frame.getSamples();
                for (ProfileData.Sample sample : samples) {
                    System.out.println(sample.scope + "." + sample.name + ": " + sample.elapsed / 1000.0);
                }

                System.out.println("\n****** Scopes ******");
                ProfileData.Scope[] scopes = frame.getScopes();
                for (Scope scope : scopes) {
                    System.out.println(scope.scope + ": " + scope.elapsed + ", " + scope.count);
                }

                System.out.println("\n****** Counters ******");
                ProfileData.Counter[] counters = frame.getCounters();
                for (Counter scope : counters) {
                    System.out.println(scope.counter + ": " + scope.value);
                }

            }
        } catch (ProfilerException e) {
            throw new ExecutionException("Failed to parse profile data", e);
        }

        /*
         * IWorkbenchWindow window =
         * HandlerUtil.getActiveWorkbenchWindowChecked(event);
         * MessageDialog.openInformation( window.getShell(), "Profiler",
         * "Hello, Eclipse world");
         */
        return null;
    }
}
