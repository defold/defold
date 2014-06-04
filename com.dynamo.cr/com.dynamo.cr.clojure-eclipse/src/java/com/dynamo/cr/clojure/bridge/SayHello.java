package com.dynamo.cr.clojure.bridge;

import java.util.concurrent.Callable;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;

import clojure.java.api.Clojure;
import clojure.lang.IFn;
import clojure.osgi.ClojureHelper;

public class SayHello extends AbstractHandler {
    public SayHello() {
    }

    private IFn lookup(ExecutionEvent event) {
        ClojureHelper.require("eclipse.handlers");
        return (IFn) Clojure.var("eclipse.handlers", "lookup").invoke(event);
    }

    @Override
    public Object execute(final ExecutionEvent event) throws ExecutionException {
        try {
            return ClojureHelper.inBundle(this, new Callable<Object>() {
                @Override
                public Object call() {
                    return lookup(event).invoke(event.getParameters());
                }
            });
        } catch (Exception e) {
            throw new ExecutionException("Execution in event handler", e);
        }
    }
}