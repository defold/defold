(ns internal.ui.editors
  (:require [service.log :as log]
            [service.registry :refer [registered]])
  (:import [org.eclipse.ui PlatformUI]
           [org.eclipse.ui.internal.registry FileEditorMapping]
           [org.eclipse.swt.widgets Display])
  (:use [clojure.pprint]))

(defn swt-thread-safe*
  [f]
  (.asyncExec (or (Display/getCurrent) (Display/getDefault)) f))

(defmacro swt-safe
  [& body]
  `(swt-thread-safe* (fn [] (do ~@body))))

