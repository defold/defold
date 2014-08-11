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

(defn implementation-for
  "Factory for values that implement the Editor protocol.
   When called with an editor site and an input file, returns an
   appropriate Editor value."
  [site file]
  ((p/editor-for
     (sys/project-state)
     (.. file getFullPath getFileExtension))
    site file))
