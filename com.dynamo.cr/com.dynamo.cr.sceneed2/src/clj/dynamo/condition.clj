(ns dynamo.condition)

(set! *warn-on-reflection* true)

(def ^:dynamic *handlers* {})

(defn- throw-unhandled
  [condition]
  (throw (ex-info "Unhandled signal: " condition)))

(defn- supertramp
  "Like trampoline, but passes the args to every call"
  [f & args]
  (loop [f f]
    (let [ret (apply f args)]
      (if (fn? ret)
        (recur ret)
        ret))))

(defn signal
  [tp & {:as more}]
  (let [handler  (get-in *handlers* [tp :handler] throw-unhandled)
        restarts (get-in *handlers* [tp :restarts])
        response (handler (merge more {:condition tp}))]
    (supertramp response *handlers* tp)))

(defn bounce [v] (throw (ex-info "Bounce" {::bounce v})))

(defn- fn-wrap
  [rhs]
  `(fn [c#]
     (let [r# (~rhs c#)]
       (if (fn? r#)
         r#
         (bounce r#)))))

(defmacro handler-bind
  [& forms]
  (let [bindings (butlast forms)
        body     (last forms)
        hmerge   (reduce
                   (fn [h [which how]]
                     (assoc-in h [which :handler] (fn-wrap how)))
                   {}
                   bindings)]
    `(binding [*handlers*  (merge *handlers* ~hmerge)]
       (try
         ~body
         (catch clojure.lang.ExceptionInfo ei#
           (if (::bounce (ex-data ei#))
             (::bounce (ex-data ei#))
             (throw ei#)))))))

(defmacro restart-case
  [& forms]
  (let [conds    (butlast forms)
        body     (last forms)
        ls       (for [c conds
                       r (rest c)
                       :let [rst  (first r)
                             doit (list* `fn (rest r))]]
                   [[(first c) :restarts rst] doit])
        as       (for [[w v] ls] `(assoc-in ~w ~v))]
    `(binding [*handlers* (-> *handlers* ~@as)]
       ~body)))

(defn invoke-restart
  [which & args]
  (fn [handlers condition]
    (if-let [f (get-in handlers [condition :restarts which])]
      (apply f args)
      (throw (ex-info (str "No such restart: " which) {})))))
