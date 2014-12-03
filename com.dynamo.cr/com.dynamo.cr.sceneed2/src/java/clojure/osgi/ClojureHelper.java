package clojure.osgi;

import java.util.concurrent.Callable;

import org.osgi.framework.Bundle;
import org.osgi.framework.FrameworkUtil;

import clojure.java.api.Clojure;
import clojure.lang.IFn;
import clojure.lang.ISeq;
import clojure.lang.Keyword;
import clojure.lang.RT;
import clojure.lang.Var;
import clojure.osgi.internal.ClojureOSGi;

/**
 * Proxy to the Clojure Java API.
 *
 * This class should be loaded dynamically inside a classloader that has the
 * bundle's .clj files available.
 *
 * @author mtnygard
 */
public class ClojureHelper {
    public static final Object EMPTY_MAP = Clojure.read("{}");

    public static final String NODE_NS = "dynamo.node";

    // Keywords that are event types
    public static final Keyword INIT = RT.keyword(null, "init");
    public static final Keyword SAVE = RT.keyword(null, "save");
    public static final Keyword CREATE = RT.keyword(null, "create");
    public static final Keyword FOCUS = RT.keyword(null, "focus");
    public static final Keyword DESTROY = RT.keyword(null, "destroy");

    // Keywords that go into the event maps
    public static final Keyword PARENT = RT.keyword(null, "parent");
    public static final Keyword SITE = RT.keyword(null, "site");
    public static final Keyword INPUT = RT.keyword(null, "input");
    public static final Keyword FILE = RT.keyword(null, "file");
    public static final Keyword MONITOR = RT.keyword(null, "monitor");

    private static final IFn SEQ = var("clojure.core", "seq");

    static {
        require(NODE_NS);
    }

    public static Var var(String pkg, String var) {
        return RT.var(pkg, var);
    }

    public static void require(String packageName) {
        ClojureOSGi.require(ClojureOSGi.clojureBundle(), packageName);
    }

    public static Object invoke(final String namespace, final String function, final Object... args) {
        try {
            return ClojureOSGi.withBundle(ClojureOSGi.clojureBundle(), new Callable<Object>() {
                @Override
                public Object call() {
                    return Clojure.var(namespace, function).applyTo((ISeq) SEQ.invoke(args));
                }
            });
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    public static <T> T inBundle(Bundle bundle, Callable<T> thunk) throws Exception {
        return ClojureOSGi.withBundle(bundle, thunk);
    }

    public static <T> T inBundle(Object caller, Callable<T> thunk) throws Exception {
        return ClojureOSGi.withBundle(FrameworkUtil.getBundle(caller.getClass()), thunk);
    }

    public static void dispatchMessage(Object behavior, Keyword messageType, Object... args) {
        Object[] messageArgs = new Object[args.length + 2];
        messageArgs[0] = behavior;
        messageArgs[1] = messageType;
        System.arraycopy(args, 0, messageArgs, 2, args.length);
        invoke(NODE_NS, "dispatch-message", messageArgs);
    }
}
