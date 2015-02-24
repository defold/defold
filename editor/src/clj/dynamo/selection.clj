(ns dynamo.selection
  (:require [schema.core :as s]
            [plumbing.core :refer [defnk fnk]]
            [dynamo.ui :as ui]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [service.log :as log]
            [internal.system :as is])
  (:import [dynamo.ui EventBroadcaster]))

(defnk produce-selection [selected-nodes]
  (mapv :_id selected-nodes))

(n/defnode Selection
  (input selected-nodes ['t/Node])

  (property selection-listeners EventBroadcaster (default #(ui/make-event-broadcaster)))

  (output selection s/Any produce-selection)
  (output selection-node Selection (fnk [self] self))

  ui/EventSource
  (send-event [this event]
    (ui/send-event (:selection-listeners this) event)))

(defmulti selected-nodes (fn [x] (class x)))
