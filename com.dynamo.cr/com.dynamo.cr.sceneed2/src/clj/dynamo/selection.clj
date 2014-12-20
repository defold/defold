(ns dynamo.selection
  (:require [schema.core :as s]
            [plumbing.core :refer [defnk]]
            [dynamo.ui :as ui]
            [dynamo.node :as n :refer [defnode]]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [service.log :as log])
  (:import [org.eclipse.ui ISelectionListener]
           [org.eclipse.jface.viewers ISelection ISelectionProvider ISelectionChangedListener SelectionChangedEvent]
           [dynamo.ui EventBroadcaster]))

(set! *warn-on-reflection* true)

(defnk produce-selection [this selected-nodes]
  (let [node-ids (mapv :_id selected-nodes)]
    (reify
      ISelection
      (isEmpty [this] (empty? node-ids))
      clojure.lang.IDeref
      (deref [this] node-ids))))

(defn fire-selection-changed
  [graph self transaction]
  (when (ds/is-modified? transaction self :selection)
    (let [before  (n/get-node-value self :selection)
          release (promise)]
      (ds/send-after self {:type :release :latch release})
      (ui/swt-thread-safe*
        (fn []
          (if (deref release 50 false)
            (let [after (n/get-node-value self :selection)]
              (when (not= @before @after)
               (prn "fire-selection-changed: selection *has* changed from " @before " to " @after)
               (.setSelection ^ISelectionProvider self after)))
            (log/warn "Timed out waiting for transaction to finish.")))))))

(defnode Selection
  (input selected-nodes [t/Node])

  (property selection-listeners EventBroadcaster (default #(ui/make-event-broadcaster)))
  (property triggers n/Triggers (default [#'fire-selection-changed]))

  (output selection s/Any produce-selection)

  (on :release
    (deliver (:latch event) true))

  ISelectionProvider
  (^ISelection getSelection [this]
    (n/get-node-value this :selection))
  (setSelection [this selection]
    (ui/send-event this (SelectionChangedEvent. this selection)))
  (^void addSelectionChangedListener [this ^ISelectionChangedListener listener]
    (ui/add-listener (:selection-listeners this) listener #(.selectionChanged listener %)))
  (removeSelectionChangedListener [this listener]
    (ui/remove-listener (:selection-listeners this) listener))

  ui/EventSource
  (send-event [this event]
    (ui/send-event (:selection-listeners this) event)))
