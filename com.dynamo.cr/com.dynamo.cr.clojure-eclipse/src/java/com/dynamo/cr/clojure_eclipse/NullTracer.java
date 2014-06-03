package com.dynamo.cr.clojure_eclipse;

/**
 * @author laurentpetit
 */
public final class NullTracer implements ITracer {
    public static final NullTracer INSTANCE = new NullTracer();
    
    private NullTracer() {}
    
    public boolean isEnabled(String traceOption) { return true; }

    public void trace(String traceOption, Object... message) {
    	System.out.println("trace:" + traceOption + ", message:"
    			+ Tracer.buildMessage(message));
    }

    public void trace(String traceOption, Throwable throwable, Object... message) {
    	System.out.println("trace:" + traceOption + ", message:"
    			+ Tracer.buildMessage(message));
    	throwable.printStackTrace();
    }

    public void traceDumpStack(String traceOption) {
    	try {
    		throw new RuntimeException("traceDumpStack:" + traceOption);
    	} catch (RuntimeException e) {
    		e.printStackTrace();
    	}
    }

    public void traceEntry(String traceOption) {
    	try {
    		throw new RuntimeException("traceEntry:" + traceOption);
    	} catch (RuntimeException e) {
    		e.printStackTrace();
    	}
    }

    public void traceEntry(String traceOption, Object... arguments) {
    	try {
    		throw new RuntimeException("traceEntry:" + traceOption
    				+ ", arguments:" + Tracer.buildMessage(arguments));
    	} catch (RuntimeException e) {
    		e.printStackTrace();
    	}
    }

    public void traceExit(String traceOption) {
    	try {
    		throw new RuntimeException("traceExit:" + traceOption);
    	} catch (RuntimeException e) {
    		e.printStackTrace();
    	}
    }

    public void traceExit(String traceOption, Object returnValue) {
    	try {
    		throw new RuntimeException("traceExit:" + traceOption 
    				+ ", returnValue:" + returnValue);
    	} catch (RuntimeException e) {
    		e.printStackTrace();
    	}
    }
    
}
