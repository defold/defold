(ns editor.handler
  (:require [plumbing.core :refer [fnk]]
            [dynamo.graph :as g]
            [editor.core :as core]
            [service.log :as log]))

(set! *warn-on-reflection* true)

(defonce ^:dynamic *handlers* (atom {}))
(defonce ^:dynamic *adapters* nil)

(defprotocol SelectionProvider
  (selection [this]))

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
    `(swap! *handlers* assoc ~qname {:command ~command
                                     :context ~context
                                     :fns ~fns})))

(defn- get-fnk [handler fsym]
  (get-in handler [:fns fsym]))

(defn- invoke-fnk [handler fsym command-context default]
  (if-let [f (get-in handler [:fns fsym])]
    (with-bindings {#'*adapters* (:adapters command-context)}
      (try (f (:env command-context))
        (catch Exception e
          (log/error :exception e :msg (format "handler %s in context %s failed at %s with message [%s]"
                                               (:command handler) (:context handler) fsym (.getMessage e))))))
    default))

(defn- get-active-handler [command command-context]
  (let [ctx-name (:name command-context)]
    (some (fn [handler]
            (when (and (= command (:command handler))
                       (= ctx-name (:context handler)))
              (when (invoke-fnk handler :active? command-context true)
                handler)))
          (vals @*handlers*))))

(defn- get-active [command command-contexts user-data]
  (let [command-contexts (mapv (fn [ctx] (assoc-in ctx [:env :user-data] user-data)) command-contexts)]
    (some (fn [ctx] (when-let [handler (get-active-handler command ctx)]
                      [handler ctx])) command-contexts)))

(defn run [[handler command-context]]
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

(defn- context-selection [context]
  (get-in context [:env :selection] (some-> (:selection-provider context) selection)))

(defn- eval-context [context]
  (let [selection (context-selection context)]
    (-> context
      eval-dynamics
      (assoc-in [:env :selection] selection))))

(defn eval-contexts [contexts all-selections?]
  (loop [command-contexts (mapv eval-context contexts)
         result []]
    (if-let [ctx (first command-contexts)]
      (let [result (if-let [selection (get-in ctx [:env :selection])]
                     (into result (map (fn [ctx] (assoc-in ctx [:env :selection] selection)) command-contexts))
                     (conj result ctx))]
        (if all-selections?
          (recur (rest command-contexts) result)
          result))
      result)))

(defn adapt [selection t]
  (if (empty? selection)
    selection
    (let [adapters *adapters*
          v (first selection)
          adapter (get adapters t)
          f (cond
              adapter adapter
              ;; this is somewhat of a hack, copied from clojure internal source
              ;; there does not seem to be a way to test if a type is a protocol
              (and (:on-interface t) (instance? (:on-interface t) v)) identity
              ;; test for node types specifically by checking for longs
              ;; we can't use g/NodeID because that is actually a wrapped ValueTypeRef
              (and (g/isa-node-type? t) (= (type v) java.lang.Long)) (fn [v] (when (g/node-instance? t v) v))
              (isa? (type v) t) identity
              (satisfies? core/Adaptable v) (fn [v] (core/adapt v t))
              true (constantly nil))]
      (mapv f selection))))

(defn adapt-every [selection t]
  (if (empty? selection)
    false
    (let [s' (adapt selection t)]
      (if (every? some? s')
        s'
        nil))))

(defn adapt-single [selection t]
  (when (and (nil? (next selection)) (first selection))
    (first (adapt selection t))))
