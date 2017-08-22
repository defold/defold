(ns editor.handler
  (:require [plumbing.core :refer [fnk]]
            [dynamo.graph :as g]
            [editor.core :as core]
            [editor.analytics :as analytics]
            [editor.error-reporting :as error-reporting]))

(set! *warn-on-reflection* true)

(defonce ^:dynamic *handlers* (atom {}))
(defonce ^:dynamic *adapters* nil)

(defprotocol SelectionProvider
  (selection [this])
  (succeeding-selection [this])
  (alt-selection [this]))

(defrecord Context [name env selection-provider dynamics adapters])

(defn ->context
  ([name env]
    (->context name env nil))
  ([name env selection-provider]
    (->context name env selection-provider {}))
  ([name env selection-provider dynamics]
    (->context name env selection-provider dynamics {}))
  ([name env selection-provider dynamics adapters]
    (->Context name env selection-provider dynamics adapters)))

; TODO: Validate arguments for all functions and log appropriate message

(defmacro defhandler [command context & body]
  (let [qname (keyword (str *ns*) (name command))
        fns (->> body
              (mapcat (fn [[fname fargs & fbody]]
                        [(keyword fname) `(fnk ~fargs ~@fbody)]))
              (apply hash-map))]
    `(swap! *handlers* assoc-in [[~command ~context] ~qname] {:command ~command
                                                              :context ~context
                                                              :fns ~fns})))

(defn available-commands
  []
  (map first (keys @*handlers*)))

(defn- get-fnk [handler fsym]
  (get-in handler [:fns fsym]))

(defonce ^:private throwing-handlers (atom #{}))

(defn enable-disabled-handlers!
  "Re-enables any handlers that were disabled because they threw an exception."
  []
  (reset! throwing-handlers #{})
  nil)

(defn- invoke-fnk [handler fsym command-context default]
  (let [env (:env command-context)
        throwing-id [(:command handler) (:context handler) fsym (:active-resource env)]]
    (if (contains? @throwing-handlers throwing-id)
      nil
      (if-let [f (get-fnk handler fsym)]
        (binding [*adapters* (:adapters command-context)]
          (try
            (f env)
            (catch Exception e
              (when (not= :run fsym)
                (swap! throwing-handlers conj throwing-id))
              (error-reporting/report-exception!
                (ex-info (format "handler '%s' in context '%s' failed at '%s' with message '%s'"
                                 (:command handler) (:context handler) fsym (.getMessage e))
                         {:handler handler
                          :command-context command-context}
                         e))
              nil)))
        default))))

(defn- get-active-handler [command command-context]
  (let [ctx-name (:name command-context)]
    (some (fn [handler]
            (when (invoke-fnk handler :active? command-context true)
              handler))
          (vals (get @*handlers* [command ctx-name])))))

(defn- get-active [command command-contexts user-data]
  (let [command-contexts (mapv (fn [ctx] (assoc-in ctx [:env :user-data] user-data)) command-contexts)]
    (some (fn [ctx] (when-let [handler (get-active-handler command ctx)]
                      [handler ctx])) command-contexts)))

(defn- ctx->screen-name [ctx]
  ;; TODO distinguish between scene/form etc when workbench is the context
  (name (:name ctx)))

(defn run [[handler command-context]]
  (analytics/track-screen (ctx->screen-name command-context))
  (invoke-fnk handler :run command-context nil))

(defn state [[handler command-context]]
  (invoke-fnk handler :state command-context nil))

(defn enabled? [[handler command-context]]
  (boolean (invoke-fnk handler :enabled? command-context true)))

(defn label [[handler command-context]]
  (invoke-fnk handler :label command-context nil))

(defn options [[handler command-context]]
  (invoke-fnk handler :options command-context nil))

(defn- eval-dynamics [context]
  (cond-> context
    (contains? context :dynamics)
    (update :env merge (into {} (map (fn [[k [node v]]] [k (g/node-value (get-in context [:env node]) v)]) (:dynamics context))))))

(defn active [command command-contexts user-data]
  (get-active command command-contexts user-data))

(defn- context-selections [context]
  (if-let [s (get-in context [:env :selection])]
    [s]
    (if-let [sp (:selection-provider context)]
      (let [s (selection sp)
            alt-s (alt-selection sp)]
        (if (and (seq alt-s) (not= s alt-s))
          [s alt-s]
          [s]))
      [nil])))

(defn- eval-selection-context [context]
  (let [selections (context-selections context)]
    (mapv (fn [selection]
            (update context :env assoc :selection selection :selection-context (:name context) :selection-provider (:selection-provider context)))
          selections)))

(defn eval-contexts [contexts all-selections?]
  (let [contexts (mapv eval-dynamics contexts)]
    (loop [selection-contexts (mapcat eval-selection-context contexts)
           result []]
      (if-let [ctx (and (or all-selections?
                            (= (:name (first selection-contexts)) (:name (first contexts))))
                        (first selection-contexts))]
        (let [result (if-let [selection (get-in ctx [:env :selection])]
                       (let [adapters (:adapters ctx)
                             name (:name ctx)
                             selection-provider (:selection-provider ctx)]
                         (into result (map (fn [ctx] (-> ctx
                                                       (update :env assoc :selection selection :selection-context name :selection-provider selection-provider)
                                                       (assoc :adapters adapters)))
                                           selection-contexts)))
                       (conj result ctx))]
          (recur (rest selection-contexts) result))
        result))))

(defn adapt [selection t]
  (if (empty? selection)
    selection
    (let [selection (if (g/isa-node-type? t)
                      (adapt selection Long)
                      selection)
          adapters *adapters*
          v (first selection)
          f (cond
              (isa? (type v) t) identity
              ;; this is somewhat of a hack, copied from clojure internal source
              ;; there does not seem to be a way to test if a type is a protocol
              (and (:on-interface t) (instance? (:on-interface t) v)) identity
              ;; test for node types specifically by checking for longs
              ;; we can't use g/NodeID because that is actually a wrapped ValueTypeRef
              (and (g/isa-node-type? t) (= (type v) java.lang.Long)) (fn [v] (when (g/node-instance? t v) v))
              (satisfies? core/Adaptable v) (fn [v] (core/adapt v t))
              true (get adapters t (constantly nil)))]
      (mapv f selection))))

(defn adapt-every [selection t]
  (if (empty? selection)
    nil
    (let [s' (adapt selection t)]
      (if (every? some? s')
        s'
        nil))))

(defn adapt-single [selection t]
  (when (and (nil? (next selection)) (first selection))
    (first (adapt selection t))))

(defn selection->node-ids [selection]
  (adapt-every selection Long))

(defn selection->node-id [selection]
  (adapt-single selection Long))
