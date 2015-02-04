(ns dynamo.ui
  "This namespace has the functions and macros you use to interact with the GUI.
  - defcommand and defhandler let you define menu items and actions"
  (:require [dynamo.types :as t]
            [internal.ui.handlers :as h]
            [internal.java :refer [bean-mapper]]
            [camel-snake-kebab :refer :all]
            [service.log :as log])
  (:import [com.dynamo.cr.sceneed.core SceneUtil SceneUtil$MouseType]
           [org.eclipse.core.runtime SafeRunner]
           [org.eclipse.jface.util SafeRunnable]
           [org.eclipse.swt.widgets Display Event Listener Shell Widget]
           [org.eclipse.swt.layout FillLayout]
           [org.eclipse.swt SWT]))

(set! *warn-on-reflection* true)

(defn mouse-type
  []
  (if (= SceneUtil$MouseType/ONE_BUTTON (SceneUtil/getMouseType))
    :one-button
    :three-button))

(defn ^Display display
  []
  (or (Display/getCurrent) (Display/getDefault)))

(defn- is-display-thread?
  [^Display display]
  (= (.getThread display) (Thread/currentThread)))

(defn swt-thread-safe*
  [f]
  (.asyncExec (display) f))

(defmacro swt-safe
  [& body]
  `(swt-thread-safe* (bound-fn [] ~@body)))

(defn swt-await*
  [f]
  (let [res (promise)]
    (.syncExec (display)
      #(deliver res (f)))
    (deref res)))

(defmacro swt-await
  [& body]
  `(swt-await* (bound-fn [] ~@body)))

(defn swt-timed-exec*
  [after f]
  (when-let [d (display)]
    (if (is-display-thread? d)
      (.timerExec d after f)
      (swt-thread-safe* #(.timerExec d after f)))))

(defmacro after
  [wait-time & body]
  `(swt-timed-exec* ~wait-time (bound-fn [] ~@body)))

(defn tick
  [interval f]
  (if-let [d (display)]
    (letfn [(tickf [] (f) (.timerExec d interval tickf))]
      (.asyncExec d tickf)
      tickf)
    (log/error :message "Display is not available.")))

(defn untick
  [tickf]
  (swt-thread-safe* #(.timerExec (display) -1 tickf)))

(defmacro swt-events [& evts]
  (let [keywords (map (comp keyword ->kebab-case str) evts)
        constants (map symbol (map #(str "SWT/" %) evts))]
    (zipmap keywords constants)))

(defmacro run-safe
  [& body]
  `(SafeRunner/run
     (proxy [SafeRunnable] []
       (run []
         ~@body))))

(def event-map (swt-events Dispose Resize Paint MouseDown MouseUp MouseDoubleClick
                           MouseEnter MouseExit MouseHover MouseMove MouseWheel DragDetect
                           FocusIn FocusOut Gesture KeyUp KeyDown MenuDetect Traverse
                           Selection DefaultSelection))

(def event-type-map (clojure.set/map-invert event-map))

(defn event-type [^Event evt]
  (event-type-map (.type evt)))

(def ^:private e->m (bean-mapper Event))

(defn event->map
  [evt]
  (update-in (e->m evt) [:type] event-type-map))

(defn make-listener [callback-fn args]
  (proxy [Listener] []
    (handleEvent [evt]
      (apply callback-fn (event->map evt) args))))

(defn listen
  [^Widget component type callback-fn & args]
  (.addListener component (event-map type)
    (make-listener callback-fn args)))

(defprotocol EventRegistration
  (add-listener [this key listener])
  (remove-listener [this key]))

(defprotocol EventSource
  (send-event [this event]))

(defrecord EventBroadcaster [listeners]
  EventRegistration
  (add-listener [this key listener] (swap! listeners assoc key listener))
  (remove-listener [this key] (swap! listeners dissoc key))

  EventSource
  (send-event [this event]
    (swt-await
      (doseq [l (vals @listeners)]
       (run-safe
         (l event))))))

(defn make-event-broadcaster [] (EventBroadcaster. (atom {})))

(defn shell [] (doto (Shell.) (.setLayout (FillLayout.))))

(defn now [] (System/currentTimeMillis))

(deftype DisplayDebouncer [delay action ^:volatile-mutable signaled ^:volatile-mutable last-signaled]
  t/Condition
  (signal [this]
    (set! last-signaled (now))
    (set! signaled true)
    (.asyncExec (display) this))

  t/Cancelable
  (cancel [this]
    (set! signaled false))

  Runnable
  (run [this]
    (when signaled
      (if (< delay (- (now) last-signaled))
        (do
          (action)
          (set! signaled false))
        (.asyncExec (display) this)))))

(defn display-debouncer
  [when f]
  (->DisplayDebouncer when f nil nil))

(defmacro defcommand
  "Create a command with the given category and id. Binds
the resulting command to the named variable.

Label should be a human-readable string. It will appear
directly in the UI (unless there is a translation for it.)

If you use the same category-id and command-id more than once,
this will create independent entities that refer to the same underlying
command."
  [name category-id command-id label]
  `(def ^:command ~name (h/make-command ~label ~category-id ~command-id)))

(defmacro defhandler
  "Creates a handler and binds it to the given command.

In the first form, the handler will always be enabled. Upon invocation, it
will call the function bound to fn-var with the
org.eclipse.core.commands.ExecutionEvent and the additional args.

In the second form, enablement-fn will be checked. When it returns a truthy
value, the handler will be enabled. Enablement-fn must have metadata to
identify the evaluation context variables and properties that affect its
return value."
  [name command & body]
  (let [enablement (if (= :enabled-when (first body)) (second body) nil)
        body       (if (= :enabled-when (first body)) (drop 2 body) body)
        fn-var     (first body)
        body       (rest body)]
    `(def ~name (h/make-handler ~command ~fn-var ~@body))))

