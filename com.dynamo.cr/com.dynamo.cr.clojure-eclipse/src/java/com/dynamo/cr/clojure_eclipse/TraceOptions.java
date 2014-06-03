package com.dynamo.cr.clojure_eclipse;

import java.util.HashMap;
import java.util.Map;

/**
 * @author laurentpetit
 */
public class TraceOptions {

  public static final String LOG_INFO = "log/info";
  public static final String LOG_WARNING = "log/warning";
  public static final String LOG_ERROR = "log/error";

  public static final String BUNDLE = "bundle";

  @SuppressWarnings("serial")
  public static final Map<String, Boolean> getTraceOptions() {
    return new HashMap<String, Boolean>() {
      {
        put(LOG_INFO, false);
        put(LOG_WARNING, false);
        put(LOG_ERROR, false);
        put(BUNDLE, true);
      }
    };
  }
}
