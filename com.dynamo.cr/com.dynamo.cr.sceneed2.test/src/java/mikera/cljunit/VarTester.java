package mikera.cljunit;

import java.util.Map;

import org.junit.runner.Description;
import org.junit.runner.notification.Failure;
import org.junit.runner.notification.RunNotifier;

import clojure.lang.Keyword;
import clojure.lang.RT;
import clojure.lang.Var;
import clojure.osgi.ClojureHelper;

public class VarTester {
    Var testVar;
    Description desc;

    private static final String CLJUNIT_CORE = "mikera.cljunit.core";
    private static final Var META = RT.var("clojure.core", "meta");
    private static final Keyword FILE = Keyword.intern("file");
    private static final Keyword LINE = Keyword.intern("line");

    @SuppressWarnings("unchecked")
    public VarTester(String ns, String name) {
        ClojureHelper.require(ns);
        testVar = RT.var(ns, name);
        Map<Keyword, Object> meta = (Map<Keyword, Object>) META.invoke(testVar);
        String file = meta.get(FILE).toString();
        String line = meta.get(LINE).toString();

        desc = Description.createSuiteDescription(name + "  <" + file + ":" + line + ">");
    }

    public void runTest(RunNotifier n) {
        n.fireTestStarted(desc);

        try {
            ClojureHelper.invoke(CLJUNIT_CORE, "invoke-test", n, desc, testVar);
            n.fireTestFinished(desc);
        } catch (Throwable t) {
            n.fireTestFailure(new Failure(desc, t));
        }
    }

    public Description getDescription() {
        return desc;
    }
}
