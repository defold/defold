package com.defold.editor;

import java.util.ArrayList;
import org.apache.commons.lang.NullArgumentException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class Shutdown {
    private static Logger logger = LoggerFactory.getLogger(Start.class);
    private static ArrayList<Runnable> shutdownActions = new ArrayList<>(8);

    public static synchronized void addShutdownAction(Runnable runnable) {
        // Called from Clojure code to register shutdown callbacks.
        if (runnable == null) {
            throw new NullArgumentException("runnable");
        }

        shutdownActions.add(runnable);
    }

    static synchronized void runShutdownActions() {
        // Execute shutdown actions in reverse add order.
        for (int i = shutdownActions.size() - 1; i >= 0; --i) {
            Runnable action = shutdownActions.get(i);

            try {
                action.run();
            } catch (Throwable error) {
                error.printStackTrace();
                logger.error("failed run shutdown action", error);
            }
        }

        shutdownActions.clear();
    }
}
