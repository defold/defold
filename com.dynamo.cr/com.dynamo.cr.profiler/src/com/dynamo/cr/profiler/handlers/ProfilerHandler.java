package com.dynamo.cr.profiler.handlers;

import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.operation.IRunnableWithProgress;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.progress.IProgressService;

import com.dynamo.cr.profiler.core.ProfileData;
import com.dynamo.cr.profiler.core.ProfileData.Frame;
import com.dynamo.cr.profiler.core.ProfileData.Sample;
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

    static class CaptureRunnable implements IRunnableWithProgress {

        private Profiler profiler;
        private String url;
        private int frameCount;

        public CaptureRunnable(Profiler profiler, String url, int frameCount) {
            this.profiler = profiler;
            this.url = url;
            this.frameCount = frameCount;
        }

        @Override
        public void run(IProgressMonitor monitor)
                throws InvocationTargetException, InterruptedException {
            monitor.beginTask("Capturing profile", IProgressMonitor.UNKNOWN);
            try {
                monitor.beginTask("Capturing profile. Press cancel to stop capture.", IProgressMonitor.UNKNOWN);
                for (int i = 0; i < frameCount; ++i) {
                    if (monitor.isCanceled()) {
                        break;
                    }
                    profiler.captureFrame(url);
                }
                profiler.captureStrings(url);
                monitor.done();
            } catch (IOException e) {
                throw new InvocationTargetException(e);
            }
        }
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
        CaptureRunnable runnable = new CaptureRunnable(profiler, url, 5500);
        try {
            service.run(true, true, runnable);
        } catch (Exception e) {
            throw new ExecutionException("Failed to capture profile data", e);
        }

        ProfileData profileData;
        try {
            profileData = profiler.parse();
            Frame[] frames = profileData.getFrames();

            Map<String, Scope> scopesTotal = new HashMap<String, ProfileData.Scope>();
            Map<String, Sample> sampleTotal = new HashMap<String, ProfileData.Sample>();

            for (Frame frame : frames) {
                ProfileData.Sample[] samples = frame.getSamples();
                for (ProfileData.Sample sample : samples) {

                    String key = sample.scope + "." + sample.name;
                    Sample total = sampleTotal.get(key);
                    if (total == null) {
                        total = new Sample(sample.name, sample.scope, 0, 0, -1);
                    }
                    total.elapsed += sample.elapsed;
                    sampleTotal.put(key, total);
                }

                ProfileData.Scope[] scopes = frame.getScopes();
                for (Scope scope : scopes) {
                    Scope total = scopesTotal.get(scope.scope);
                    if (total == null) {
                        total = new Scope(scope.scope, 0, 0);
                    }
                    total.elapsed += scope.elapsed;
                    total.count += scope.count;
                    scopesTotal.put(scope.scope, total);
                }

            }

            ArrayList<Scope> scopesTotalSorted = new ArrayList<Scope>(scopesTotal.values());
            Collections.sort(scopesTotalSorted, new Comparator<Scope>() {
                @Override
                public int compare(Scope o1, Scope o2) {
                    return (int) (o2.elapsed - o1.elapsed);
                }
            });

            ArrayList<Sample> samplesTotalSorted = new ArrayList<Sample>(sampleTotal.values());
            Collections.sort(samplesTotalSorted, new Comparator<Sample>() {
                @Override
                public int compare(Sample o1, Sample o2) {
                    return (int) (o2.elapsed - o1.elapsed);
                }
            });

            System.out.println("\n****** Scopes Total ******");
            for (Scope scope : scopesTotalSorted) {
                System.out.println(scope.scope + ": " + scope.elapsed / 1000.0 + ", " + scope.count);
            }

            System.out.println("****** Samples Total ******");
            for (Sample sample : samplesTotalSorted) {
                System.out.println(sample.scope + "." + sample.name + ": " + sample.elapsed / 1000.0);
            }

            System.out.println(String.format("%d frames captured", frames.length));

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
