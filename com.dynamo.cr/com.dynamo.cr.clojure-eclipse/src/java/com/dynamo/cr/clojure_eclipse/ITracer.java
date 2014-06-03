package com.dynamo.cr.clojure_eclipse;


/**
 * @author laurentpetit
 */
public interface ITracer {
	
    boolean isEnabled(String traceOption);
    
    void trace(String traceOption, Object... message);
    
    void trace(String traceOption, Throwable throwable, Object... message);
    
    void traceDumpStack(String traceOption);
    
    void traceEntry(String traceOption);
    
    void traceEntry(String traceOption, Object... arguments);

    void traceExit(String traceOption);
    
    void traceExit(String traceOption, Object returnValue);

}
