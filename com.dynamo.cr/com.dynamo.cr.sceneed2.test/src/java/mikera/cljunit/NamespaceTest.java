package mikera.cljunit;

import org.junit.runner.RunWith;

@RunWith(NamespaceRunner.class)
public abstract class NamespaceTest {
	
	public NamespaceTest() {
		// nothing, used for default instance
	}
	
	public abstract String namespace();
}
