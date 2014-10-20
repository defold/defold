(ns dynamo.condition
  "Provides a functional structure for signaling errors
and conditionally handling them. This approach works as follows:

A low-level function signals a named error condition in place of throwing
an exception using `signal`. Example, from `dynamo.image`:

    (defn image-from-resource [this project]
      (let [src (project-path project (:image this))]
        (try
          (load-image src)
          (catch Throwable e
            (signal :unreadable-resource :exception e :path src)))))

When `load-image` throws an exception, the resource on the src path is
unavailable. This signal can be handled by a restart.

A production function in a node offers a collection of “restarts”
that handle the signaled condition. Some of these restarts will allow the computation
to continue normally. Others will produce an Error value.  Example, from
`atlas.core`:

		(defn produce-textureset
		  [this images animations margin extrude-borders]
		  (handler-bind
		    (:unreadable-resource use-placeholder)
		    (-> (pack-textures margin extrude-borders (consolidate images animations))
		      (assoc :animations animations))))

In this case, `handler-bind` indicates that a restart provider for `:unreadable-resource`
is the function `use-placeholder`. That function defines the `:use-value` restarts available for that
handler:

    (defn use-placeholder [_] (invoke-restart :use-value placeholder-image))

When the error condition :unreadable-resource is signaled, `use-placeholder` will
use the value of `placeholder-image` in the location where the signal originated.
The particular restart case is selected when the value is produced:

		(defn- get-value-with-restarts
		  [node g label]
		  (restart-case
		    (:unreadable-resource
		      (:use-value [v] v))
		    (t/get-value node g label)))

`signal` signals an error condition which may be restarted at the site of the error.
`handler-bind` provides a function that, when evaluated, defines the restart to be invoked.
(using `invoke-restart`). `restart-case` chooses a restart case to be used within its
closure for a particular signal. For additional detail, consult the API document or
`dynamo.condition-test`."
  )

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
  "Signals an error condition (tp) and a map of additional information.
If the signal is handled, returns value. Otherwise, throws exception."
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
  "Binds a list of condition handlers by condition type to be used within the
closure. Handlers define a function to be called when the condition is `signal`ed.
Example:

    (defn image-from-resource [this path]
      (try
        (load-image path)
        (catch Throwable e
          (signal :unreadable-resource :exception e
                                       :path path)))))

		(defn produce-image
				  [this path]
				  (handler-bind
				    (:unreadable-resource use-placeholder-image)
				    (image-from-resource path))))

See `dynamo.condition` docs for details."
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
  "Provides a list of conditions and the restart cases to use within this closure. example:

		(restart-case
	    (:unreadable-resource
			  (:use-value [v] v))
        ,,, )

See `dynamo.condition` docs for details."
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
  "Invokes a restart case with the args provided. Example:

    (invoke-restart :unreadable-resource :use-value placeholder-image)

When the `:use-value` restart is called on an `:unreadable-resource` condition, the value of
`placeholder-image` will be used. See `dynamo.condition` docs for details."
  [which & args]
  (fn [handlers condition]
    (if-let [f (get-in handlers [condition :restarts which])]
      (apply f args)
      (throw (ex-info (str "No such restart: " which) {})))))
