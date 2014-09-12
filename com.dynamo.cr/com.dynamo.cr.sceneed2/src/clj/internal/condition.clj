(ns internal.condition)

(def ^:dynamic *handlers* {})

(defn- throw-unhandled
  [condition]
  (throw (ex-info "Unhandled signal" condition)))

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


(comment

  ;; inside an image node
  (def load-image
    (caching
     (fn [src]
       (if-let [img (ImageIO/read (io/input-stream src))]
         (make-image src img)
         (signal :missing-image :path src)))))

  (defnk image-from-resource :- Image
    [this project]
    (load-image (project-path project (:image this))))


  ;; inside a node, when accessing its own values
  (restart-case
   (:missing-image
    (:use-value [v] v)
    (:retry     [f] (image-from-file f)))
   (p/get-resource-value project-state this :textureset))

  ;; upper level code, when asking for the textureset to render to screen
  (defn use-placeholder-image
    [condition]
    (invoke-restart :use-value placeholder))

  (defn produce-renderable
    (handler-bind
     (:missing-image  #'use-placeholder-image)
     (:corrupted-file (fn [condition] (service.log/warn :corrupted-file (:filename condition))))
     (p/get-resource-value project-state :renderable)))

  )
