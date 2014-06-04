package clojure.osgi.internal;

import java.util.HashSet;
import java.util.Set;
import java.util.StringTokenizer;
import java.util.concurrent.Callable;

import org.osgi.framework.Bundle;
import org.osgi.framework.BundleContext;
import org.osgi.framework.BundleEvent;
import org.osgi.framework.ServiceReference;
import org.osgi.service.log.LogService;
import org.osgi.util.tracker.BundleTracker;
import org.osgi.util.tracker.ServiceTracker;

import clojure.lang.RT;
import clojure.lang.Var;

public class ExtenderTracker extends BundleTracker<Long> {
    private final Set<Long> requireProcessed = new HashSet<Long>();
    private final Set<Long> active = new HashSet<Long>();
    private ServiceTracker<Bundle, LogService> logTracker;
    private LogService log = new StreamLog(System.out);

    private enum CallbackType {
        START, STOP
    }

    public ExtenderTracker(BundleContext context) {
        super(context, Bundle.STARTING | Bundle.ACTIVE | Bundle.STOPPING, null);

        logTracker = new ServiceTracker<Bundle, LogService>(context, LogService.class.getName(), null) {
            @Override
            public LogService addingService(ServiceReference<Bundle> reference) {
                return log = super.addingService(reference);
            }

            @Override
            public void removedService(ServiceReference<Bundle> reference, LogService service) {
                super.removedService(reference, service);

                log = new StreamLog(System.out);
            }
        };

        logTracker.open();
    }

    @Override
    public Long addingBundle(Bundle bundle, BundleEvent event) {
        if (!requireProcessed.contains(bundle.getBundleId())) {
            processRequire(bundle);
            requireProcessed.add(bundle.getBundleId());
        }

        if ((bundle.getState() == Bundle.STARTING || bundle.getState() == Bundle.ACTIVE) && !active.contains(bundle.getBundleId())) {

            try {
                invokeActivatorCallback(CallbackType.START, bundle);
            } finally {
                active.add(bundle.getBundleId());
            }

        } else if (bundle.getState() == Bundle.STOPPING && active.contains(bundle.getBundleId())) {
            try {
                invokeActivatorCallback(CallbackType.STOP, bundle);
            } finally {
                active.remove(bundle.getBundleId());
            }
        }

        return bundle.getBundleId();
    }

    private void processRequire(Bundle bundle) {
        String header = bundle.getHeaders().get("Clojure-Require");

        if (header != null) {
            StringTokenizer lib = new StringTokenizer(header, ",");
            while (lib.hasMoreTokens()) {
                final String ns = lib.nextToken().trim();
                if (log != null)
                    log.log(LogService.LOG_DEBUG, String.format("requiring %s from bundle %s", ns, bundle));
                ClojureOSGi.require(bundle, ns);
            }
        }
    }

    private String callbackFunctionName(CallbackType callback, String header) {
        // TODO support callback function name customization. i.e.:
        // Clojure-ActivatorNamespace:
        // a.b.c.d;start-function="myStart";stop-function="myStop"
        switch (callback) {
        case START:
            return "bundle-start";

        case STOP:
            return "bundle-stop";

        default:
            throw new IllegalStateException();
        }
    }

    private void invokeActivatorCallback(CallbackType callback, final Bundle bundle) {
        final String ns = bundle.getHeaders().get("Clojure-ActivatorNamespace");
        if (ns != null) {
            final String callbackFunction = callbackFunctionName(callback, ns);
            final Var var = RT.var(ns, callbackFunction);
            if (var.isBound()) {
                try {
                    ClojureOSGi.withBundle(bundle, new Callable<Object>() {
                        @Override
                        public Object call() throws Exception {
                            if (log != null) {
                                log.log(LogService.LOG_DEBUG, String.format("invoking function %s/%s for bundle: %s", ns, callbackFunction, bundle));
                            }

                            var.invoke(bundle.getBundleContext());

                            return null;
                        }
                    });

                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            } else {
                throw new RuntimeException(String.format("'%s' is not bound in '%s'", callbackFunction, ns));

            }
        }
    }

    @Override
    public void modifiedBundle(Bundle bundle, BundleEvent event, Long object) {

    }

    @Override
    public void removedBundle(Bundle bundle, BundleEvent event, Long object) {

    }

    @Override
    public void close() {
        logTracker.close();
        super.close();
    }
}
