(ns internal.id-gen
  (:require [internal.graph.types :as gt]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

;; Thread-local flag to prevent id-generation inside property setters.
(def ^:dynamic *allow-generated-ids* true)

(defrecord ^:private State [^long next-id claimed-ids])

(defn make-id-generator
  "Returns a new id generator that starts at zero."
  []
  (atom (->State 0 {})))

(defn peek-id
  "Peek at the next id that will be generated."
  ^long [id-generator]
  (.next-id ^State (deref id-generator)))

(defn next-id!
  "Generate and return a new id."
  ^long [id-generator]
  (assert *allow-generated-ids*)
  (dec (.next-id ^State (swap! id-generator
                               (fn [^State state]
                                 (->State (inc (.next-id state))
                                          (.claimed-ids state)))))))

(defn claim-id!
  "Generate and return a new id. The id becomes associated with the supplied
  key, and subsequent calls with the same key will return the same id."
  ^long [id-generator key]
  (let [id-volatile (volatile! nil)]
    (swap! id-generator
           (fn [^State state]
             (if-some [existing-id (get (.claimed-ids state) key)]
               (do (vreset! id-volatile existing-id)
                   state)
               (let [id (.next-id state)]
                 (vreset! id-volatile id)
                 (->State (inc id)
                          (assoc (.claimed-ids state) key id))))))
    @id-volatile))

(defn next-node-id!
  ^long [node-id-generators-by-graph-id ^long graph-id]
  (gt/make-node-id graph-id (next-id! (node-id-generators-by-graph-id graph-id))))

(defn claim-node-id!
  ^long [node-id-generators-by-graph-id ^long graph-id node-key]
  (gt/make-node-id graph-id (claim-id! (node-id-generators-by-graph-id graph-id) node-key)))

(defn next-override-id!
  ^long [override-id-generator ^long graph-id]
  (gt/make-override-id graph-id (next-id! override-id-generator)))

(defn claim-override-id!
  ^long [override-id-generator ^long graph-id override-key]
  (gt/make-override-id graph-id (claim-id! override-id-generator override-key)))
