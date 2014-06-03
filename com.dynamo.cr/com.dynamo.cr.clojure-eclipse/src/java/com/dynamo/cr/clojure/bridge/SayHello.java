package com.dynamo.cr.clojure.bridge;

import java.util.concurrent.Callable;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;

import com.dynamo.cr.clojure_eclipse.ClojureEclipse;
import com.dynamo.cr.clojure_eclipse.ClojureHelper;
import com.dynamo.cr.clojure_eclipse.ClojureOSGi;
import com.dynamo.cr.clojure_eclipse.TraceOptions;

import clojure.java.api.Clojure;
import clojure.lang.IFn;

public class SayHello extends AbstractHandler {
	public SayHello() {
    ClojureEclipse.getTracer().trace(TraceOptions.LOG_INFO, "TCCL when instantiating SayHello is " + Thread.currentThread().getContextClassLoader());
	}

	private IFn lookup(ExecutionEvent event) {
    ClojureHelper.require("eclipse.handlers");
    return (IFn) Clojure.var("eclipse.handlers", "lookup").invoke(event);
	}
	
	@Override
	public Object execute(final ExecutionEvent event) throws ExecutionException {
	  return ClojureOSGi.inBundle(new Callable<Object>() {
      @Override
      public Object call() {
        ClojureEclipse.getTracer().trace(TraceOptions.LOG_INFO, "TCCL when executing handler is " + Thread.currentThread().getContextClassLoader());
        return lookup(event).invoke(event.getParameters());
      }
    });
	}
}