package mikera.cljunit;

import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.junit.runner.Description;
import org.junit.runner.notification.RunNotifier;

import clojure.lang.IFn;
import clojure.lang.Keyword;
import clojure.lang.RT;
import clojure.lang.Symbol;
import clojure.lang.Var;

public class Clojure {
    public static final Var REQUIRE = RT.var("clojure.core", "require");
    public static final Var META = RT.var("clojure.core", "meta");
    public static final Keyword TEST_KEY = Keyword.intern("test");
    public static final Keyword PREFIX = Keyword.intern("prefix");

    static {
        require("clojure.test");
        require("mikera.cljunit.core");
    }

    public static final Var GET_TEST_VAR_NAMES = RT.var("mikera.cljunit.core", "get-test-var-names");
    public static final Var GET_TEST_NAMESPACE_NAMES = RT.var("mikera.cljunit.core", "get-test-namespace-names");
    public static final Var INVOKE_TEST = RT.var("mikera.cljunit.core", "invoke-test");
    public static final Var INVOKE_NS_TEST = RT.var("mikera.cljunit.core", "invoke-ns-tests");

    @SuppressWarnings("unchecked")
    public static Collection<String> getTestVars(String namespace) {
        return (Collection<String>) GET_TEST_VAR_NAMES.invoke(namespace);
    }

    public static void require(String ns) {
        REQUIRE.invoke(Symbol.intern(ns));
    }

    @SuppressWarnings("unchecked")
    public static List<String> getNamespaces() {
        return (List<String>) GET_TEST_NAMESPACE_NAMES.invoke();
    }

    @SuppressWarnings("unchecked")
    public static List<String> getNamespaces(String filter) {
        @SuppressWarnings({ "rawtypes" })
        Map<Object, Object> hm = new HashMap();
        hm.put(PREFIX, filter);

        return (List<String>) GET_TEST_NAMESPACE_NAMES.invoke(hm);
    }

    public static Object invokeTest(RunNotifier n, Description desc, Var testVar) {
        return INVOKE_TEST.invoke(n, desc, testVar);
    }

    public static Object invokeNamespaceTests(RunNotifier n, Description d, String namespace, IFn inner) {
        return INVOKE_NS_TEST.invoke(n, d, namespace, inner);
    }
}
