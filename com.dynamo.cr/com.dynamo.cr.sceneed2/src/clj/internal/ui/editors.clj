(ns internal.ui.editors
  (:require [dynamo.project :as p]
            [dynamo.file :as f]
            [internal.system :as sys]
            [service.log :as log])
  (:import  [org.eclipse.swt.widgets Display Listener]))

(defn display
  []
  (or (Display/getCurrent) (Display/getDefault)))

(defn swt-thread-safe*
  [f]
  (.asyncExec (display) f))

(defmacro swt-safe
  [& body]
  `(swt-thread-safe* (bound-fn [] ~@body)))

(defmacro swt-await
  [& body]
  `(let [res# (promise)]
     (.syncExec (display)
       (bound-fn [] (deliver res# (do ~@body))))
     (deref res#)))

(defn swt-timed-exec*
  [after f]
  (when-let [cur (display)]
    (.timerExec cur after f)))

(defmacro swt-timed-exec
  [after & body]
  `(swt-timed-exec* ~after (bound-fn [] ~@body)))

(defn listen
  [c t f]
  (.addListener c t
    (proxy [Listener] []
      (handleEvent [evt]
        (f evt)))))

(defn implementation-for
  "Factory for values that implement the Editor protocol.
   When called with an editor site and an input file, returns an
   appropriate Editor value."
  [site file]
  (let [proj (sys/project-state)]
    ((p/editor-for
       proj
       (.. file getFullPath getFileExtension))
      proj site file)))
