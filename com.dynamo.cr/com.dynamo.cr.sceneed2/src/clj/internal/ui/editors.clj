(ns internal.ui.editors
  (:require [dynamo.project :as p]
            [dynamo.file :as f]
            [internal.system :as sys]
            [service.log :as log])
  (:import  [org.eclipse.swt.widgets Display]))

(defn swt-thread-safe*
  [f]
  (.asyncExec (or (Display/getCurrent) (Display/getDefault)) f))

(defmacro swt-safe
  [& body]
  `(swt-thread-safe* (fn [] (do ~@body))))

(defn swt-timed-exec*
  [after f]
  (.timerExec (Display/getCurrent) after f))

(defmacro swt-timed-exec
  [after & body]
  `(swt-timed-exec* ~after (fn [] (do ~@body))))

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
