package mikera.cljunit;

import java.util.List;

import org.junit.runner.RunWith;

@RunWith(ClojureRunner.class)
public abstract class ClojureTest {

    public abstract List<String> namespaces();

    /**
     * Specifies a prefix for namespaces to test, e.g. "my.organisation"
     * @return
     */
    public String filter() {
        return null;
    }
}
