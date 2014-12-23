(ns eclipse.markers
  (:import [org.eclipse.core.resources IMarker IResource]
           [org.eclipse.core.runtime IStatus]))

(set! *warn-on-reflection* true)

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

(defrecord Status [severity children message code exception]
  IStatus
  (matches [this severity-mask]
    (not= 0 (bit-and severity-mask severity)))
  (getMessage [this]
    message)
  (getException [this]
    exception)
  (isMultiStatus [this]
    (not (empty? children)))
  (getPlugin [this]
    "com.dynamo.cr.sceneed2")
  (getCode [this]
    code)
  (isOK [this]
    (= severity IStatus/OK))
  (getChildren [this]
    children)
  (getSeverity [this]
    severity))

(defn make-status
  ([severity message]
    (make-status severity message IStatus/OK))
  ([severity message code]
    (make-status severity nil message code nil))
  ([severity message code exception]
    (make-status severity nil message code exception))
  ([severity children message code exception]
    (->Status severity children message code exception)))

(def ok-status (make-status IStatus/OK "ok"))
(defn warning-status
  ([message]
    (make-status IStatus/WARNING message IStatus/WARNING))
  ([message code]
    (make-status IStatus/WARNING message code)))
(defn error-status
  ([message]
    (make-status IStatus/ERROR message IStatus/ERROR))
  ([message code]
    (make-status IStatus/ERROR message code))
  ([message code exception]
    (make-status IStatus/ERROR message code exception)))
