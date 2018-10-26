(ns editor.disk-availability
  (:require [editor.ui :as ui])
  (:import (javafx.beans.property SimpleBooleanProperty)))

(set! *warn-on-reflection* true)

(def ^:private busy-refcount-atom (atom 0))

(defn available? []
  (zero? @busy-refcount-atom))

(defn push-busy! []
  (swap! busy-refcount-atom inc))

(defn pop-busy! []
  (swap! busy-refcount-atom (fn [busy-refcount]
                              (assert (pos? busy-refcount))
                              (dec busy-refcount))))

;; WARNING:
;; Observing or binding to an observable that lives longer than the observer will
;; cause a memory leak. You must manually unhook them or use weak listeners.
;; Source: https://community.oracle.com/message/10360893#10360893
(defonce ^SimpleBooleanProperty available-property (SimpleBooleanProperty. true))

(add-watch busy-refcount-atom ::busy-refcount-watch
           (fn [_ _ _ _]
             (ui/run-later
               (.set available-property (available?)))))
