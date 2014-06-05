package clojure.osgi;

import java.util.concurrent.Callable;

import org.osgi.framework.Bundle;

public interface IClojureOSGi {
    void unload(Bundle aBundle);

    void require(Bundle aBundle, final String aName);

    void loadAOTClass(final Bundle aContext, final String fullyQualifiedAOTClassName) throws Exception;

    <T> T withBundle(Bundle aBundle, final Callable<T> aCode);
}
