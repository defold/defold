package mikera.cljunit;

import java.util.Map;

import org.junit.runner.Description;
import org.junit.runner.notification.Failure;
import org.junit.runner.notification.RunNotifier;

import clojure.lang.Keyword;
import clojure.lang.RT;
import clojure.lang.Var;

public class VarTester {
    Var testVar;
    Description desc;

    private static Keyword FILE = Keyword.intern("file");
    private static Keyword LINE = Keyword.intern("line");

    @SuppressWarnings("unchecked")
    public VarTester(String ns, String name) {
        Clojure.require(ns);
        testVar = RT.var(ns, name);
        Map<Keyword, Object> meta = (Map<Keyword, Object>) Clojure.META.invoke(testVar);
        String file = meta.get(FILE).toString();
        String line = meta.get(LINE).toString();

        desc = Description.createSuiteDescription(name + "  <" + file + ":" + line + ">");
    }

    public void runTest(RunNotifier n) {
        n.fireTestStarted(desc);

        try {
            Clojure.invokeTest(n, desc, testVar);
            n.fireTestFinished(desc);
        } catch (Throwable t) {
            n.fireTestFailure(new Failure(desc, t));
        }
    }

    public Description getDescription() {
        return desc;
    }
}
