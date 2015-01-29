(ns dynamo.selection
  (:require [schema.core :as s]
            [plumbing.core :refer [defnk fnk]]
            [dynamo.ui :as ui]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [service.log :as log]
            [internal.system :as is]
            [internal.ui.handlers :as handlers])
  (:import [org.eclipse.ui ISelectionListener]
           [org.eclipse.jface.viewers ISelection ISelectionProvider ISelectionChangedListener SelectionChangedEvent]
           [dynamo.ui EventBroadcaster]
           [org.eclipse.core.commands ExecutionEvent]))

(set! *warn-on-reflection* true)

(defnk produce-selection [this selected-nodes]
  (let [node-ids (mapv :_id selected-nodes)]
    (reify
      ISelection
      (isEmpty [this] (empty? node-ids))
      clojure.lang.IDeref
      (deref [this] node-ids))))

(defn fire-selection-changed
  [transaction graph self label kind]
  (when (ds/is-modified? transaction self :selection)
    (let [before  (n/get-node-value self :selection)
          release (promise)]
      (ds/send-after self {:type :release :latch release})
      (ui/swt-thread-safe*
        (fn []
          (if (deref release 50 false)
            (let [after (n/get-node-value (ds/refresh self) :selection)]
              (when (not= @before @after)
               (prn "fire-selection-changed: selection *has* changed from " @before " to " @after)
               (.setSelection ^ISelectionProvider self after)))
            (log/warn "Timed out waiting for transaction to finish.")))))))

(n/defnode Selection
  (input selected-nodes ['t/Node])

  (property selection-listeners EventBroadcaster (default #(ui/make-event-broadcaster)))

  (trigger notify-listeners :modified fire-selection-changed)

  (output selection s/Any produce-selection)
  (output selection-node Selection (fnk [self] self))

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

(defmulti selected-nodes (fn [x] (class x)))

(defmethod selected-nodes ExecutionEvent
  [^ExecutionEvent evt]
  (let [workbench-selection (handlers/active-current-selection (.getApplicationContext evt))]
    (when (instance? clojure.lang.IDeref workbench-selection)
      (map (partial ds/node (is/world-ref)) (deref workbench-selection)))))
