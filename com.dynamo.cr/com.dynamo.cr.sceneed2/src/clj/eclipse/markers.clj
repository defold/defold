(ns eclipse.markers
  (:import [org.eclipse.core.resources IMarker IResource]))

(defn remove-markers
  [^IResource resource]
  (.deleteMarkers resource IMarker/PROBLEM true IResource/DEPTH_INFINITE))

(defn attach-marker
  [^IResource resource type severity message location line]
  (doto (.createMarker resource type)
    (.setAttribute IMarker/SEVERITY    severity)
    (.setAttribute IMarker/MESSAGE     message)
    (.setAttribute IMarker/LOCATION    location)
    (.setAttribute IMarker/LINE_NUMBER line)))

(defn compile-error
  ([^IResource resource ^clojure.lang.Compiler$CompilerException error]
    (compile-error resource (.getMessage (.getCause error)) (.line error)))
  ([^IResource resource message line]
    (attach-marker resource IMarker/PROBLEM IMarker/SEVERITY_ERROR message (str "Line " line) line)))