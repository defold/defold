package mikera.cljunit;

import java.util.ArrayList;
import java.util.Collection;

import org.junit.runner.Description;
import org.junit.runner.notification.Failure;
import org.junit.runner.notification.RunNotifier;

import clojure.lang.AFn;
import clojure.lang.ExceptionInfo;
import clojure.osgi.ClojureHelper;

class NamespaceTester {
    public Description d;
    public String namespace;
    public ExceptionInfo failOnCompile;

    public ArrayList<VarTester> children = new ArrayList<VarTester>();

    private static final String CLJUNIT_CORE = "mikera.cljunit.core";

    public NamespaceTester(String ns) {
        this.namespace = ns;
        d = Description.createSuiteDescription(namespace);
        try {
            ClojureHelper.require(CLJUNIT_CORE);
            @SuppressWarnings("unchecked")
            Collection<String> testVars = (Collection<String>) ClojureHelper.invoke(CLJUNIT_CORE, "get-test-var-names", namespace);

            for (String v : testVars) {
                VarTester vt = new VarTester(namespace, v);
                d.addChild(vt.getDescription());
                children.add(vt);
            }
        } catch (ExceptionInfo e) {
            failOnCompile = e;
        }
    }

    public Description getDescription() {
        return d;
    }

    public void runTest(final RunNotifier n) {
        if (failOnCompile != null) {
            n.fireTestFailure(new Failure(d, failOnCompile));
        } else {
            ClojureHelper.invoke(CLJUNIT_CORE, "invoke-ns-tests", n, d, namespace, new AFn() {
                @Override
                public Object invoke() {
                    for (VarTester vt : children) {
                        vt.runTest(n);
                    }
                    return null;
                }
            });
        }
    }
}
