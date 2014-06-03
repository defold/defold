package com.dynamo.cr.clojure_eclipse;

import java.util.Dictionary;
import java.util.HashMap;
import java.util.Hashtable;
import java.util.Map;

import org.eclipse.osgi.service.debug.DebugOptions;
import org.eclipse.osgi.service.debug.DebugOptionsListener;
import org.eclipse.osgi.service.debug.DebugTrace;
import org.osgi.framework.BundleContext;


/**
 * @author laurentpetit
 */
public class Tracer implements ITracer {
    
    private final String bundleSymbolicName;

    private final PluginDebugOptionsListener traceOptionsListener = new PluginDebugOptionsListener();
    
    private class PluginDebugOptionsListener implements DebugOptionsListener {
        public void optionsChanged(DebugOptions options) {
            if (options.isDebugEnabled()) {
                debugTrace = options.newDebugTrace(bundleSymbolicName, Tracer.class);
            } else {
                debugTrace = null;
            }
            updateOptions(options);
        }
    }
    
    private DebugTrace debugTrace = null;

    private final Map<String, Boolean> options = new HashMap<String, Boolean>();
    
    public Tracer(final BundleContext bundleContext, Map<String, Boolean> options) {
        this.bundleSymbolicName = bundleContext.getBundle().getSymbolicName();
        this.options.putAll(options);
        
        enableTracing(bundleContext);
    }
    
    private void enableTracing(BundleContext context) {
        Dictionary<String, String> props = new Hashtable<String, String>();
        props.put(DebugOptions.LISTENER_SYMBOLICNAME, context.getBundle().getSymbolicName());
        context.registerService(
                DebugOptionsListener.class.getName(),
                traceOptionsListener,
                props);
    }
    
    private void updateOptions(DebugOptions options) {
        for (String option: this.options.keySet()) {
            this.options.put(option, options.getBooleanOption(bundleSymbolicName + "/" + option, false));
        }
    }
    
    public boolean isEnabled(String traceOption) {
        if (debugTrace == null) {
            return false;
        }
        
        Boolean res = options.get(traceOption);
        if (res != null) {
            return res;
        } else {
            return false;
        }
    }

    public static String buildMessage(Object[] message) {
        StringBuilder sb = new StringBuilder();
        for (Object m: message) {
            sb.append(m);
        }
        return sb.toString();
    }
    
    public void trace(String traceOption, Object... message) {
    	if (debugTrace != null)
        debugTrace.trace("/" + traceOption, buildMessage(message));
    	System.out.println(traceOption + ": " + buildMessage(message));
        if (isEnabled(traceOption)) {
            debugTrace.trace("/" + traceOption, buildMessage(message));
        }
    }

    public void trace(String traceOption, Throwable throwable, Object... message) {
    	if (debugTrace != null)
        debugTrace.trace("/" + traceOption, buildMessage(message), throwable);
    	System.out.println(traceOption + ": " + buildMessage(message) + ". Exception: " + throwable.getMessage());
    	throwable.printStackTrace();
        if (isEnabled(traceOption)) {
            debugTrace.trace("/" + traceOption, buildMessage(message), throwable);
        }
    }

    public void traceDumpStack(String traceOption) {
    	if (debugTrace != null)
        debugTrace.traceDumpStack("/" + traceOption);
        System.out.println("trace dump stack: " + traceOption);
        if (isEnabled(traceOption)) {
            debugTrace.traceDumpStack("/" + traceOption);
        }
    }

    public void traceEntry(String traceOption) {
    	if (debugTrace != null)
    	debugTrace.traceEntry("/" + traceOption);
    	System.out.println("trace entry: " + traceOption);
        if (isEnabled(traceOption)) {
            debugTrace.traceEntry("/" + traceOption);
        }
    }

    public void traceEntry(String traceOption, Object... arguments) {
    	if (debugTrace != null)
    	debugTrace.traceEntry("/" + traceOption, arguments);
    	System.out.print("trace entry: " + traceOption);
    	if (arguments != null) {
    		System.out.print("[");
    		boolean isFirst = true;
    		for (Object o: arguments) {
    			System.out.println(o);
    			if (isFirst) {
    				isFirst = false;
    			} else {
    				System.out.println(", ");
    			}
    		}
    		System.out.print("]");
    	}
    	System.out.println();
        if (isEnabled(traceOption)) {
            debugTrace.traceEntry("/" + traceOption, arguments);
        }
    }

    public void traceExit(String traceOption) {
    	if (debugTrace != null)
    	debugTrace.traceExit("/" + traceOption);
    	System.out.println("trace exit: " + traceOption);
        if (isEnabled(traceOption)) {
            debugTrace.traceExit("/" + traceOption);
        }
    }

    public void traceExit(String traceOption, Object returnValue) {
    	if (debugTrace != null)
        debugTrace.traceExit("/" + traceOption, returnValue);
    	System.out.print("trace entry: " + traceOption);
    	if (returnValue != null) {
    		System.out.print("[" + returnValue + "]");
    	}
    	System.out.println();
        if (isEnabled(traceOption)) {
            debugTrace.traceExit("/" + traceOption, returnValue);
        }
    }
    
}
