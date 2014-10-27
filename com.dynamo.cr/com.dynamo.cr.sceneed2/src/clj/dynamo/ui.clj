(ns dynamo.ui
  (:require [internal.ui.handlers :as h]
            [internal.java :refer [bean-mapper]]
            [dynamo.types :as t]
            [camel-snake-kebab :refer :all])
  (:import  [com.dynamo.cr.sceneed.core SceneUtil SceneUtil$MouseType]
            [org.eclipse.swt.widgets Display Listener Widget]
            [org.eclipse.swt SWT]))

(set! *warn-on-reflection* true)

(defn mouse-type
  []
  (if (= SceneUtil$MouseType/ONE_BUTTON (SceneUtil/getMouseType))
    :one-button
    :three-button))

(defn display ^Display
  []
  (or (Display/getCurrent) (Display/getDefault)))

(defn- is-display-thread?
  [display]
  (= (.getThread display) (Thread/currentThread)))

(defn swt-thread-safe*
  [f]
  (.asyncExec (display) f))

(defmacro swt-safe
  [& body]
  `(swt-thread-safe* (bound-fn [] ~@body)))

(defmacro swt-await
  [& body]
  `(let [res# (promise)]
     (.syncExec (display)
       (bound-fn [] (deliver res# (do ~@body))))
     (deref res#)))

(defn swt-timed-exec*
  [after f]
  (when-let [d (display)]
    (if (is-display-thread? d)
      (.timerExec d after f)
      (swt-thread-safe* #(.timerExec d after f)))))

(defmacro swt-timed-exec
  [after & body]
  `(swt-timed-exec* ~after (bound-fn [] ~@body)))

(defmacro after
  [wait-time & body]
  `(swt-timed-exec ~wait-time ~@body))

(defmacro swt-events [& evts]
  (let [keywords (map (comp keyword ->kebab-case str) evts)
        constants (map symbol (map #(str "SWT/" %) evts))]
    (zipmap keywords constants)))

(def event-map (swt-events Dispose Resize Paint MouseDown MouseUp MouseDoubleClick
                           MouseEnter MouseExit MouseHover MouseMove MouseWheel DragDetect
                           FocusIn FocusOut Gesture KeyUp KeyDown MenuDetect Traverse))

(def event-type-map (clojure.set/map-invert event-map))

(defn event-type [^org.eclipse.swt.widgets.Event evt]
  (event-type-map (.type evt)))

(def ^:private e->m (bean-mapper org.eclipse.swt.widgets.Event))

(defn event->map
  [evt]
  (update-in (e->m evt) [:type] event-type-map))

(defn listen
  [^Widget component type callback-fn & args]
  (.addListener component (event-map type)
    (proxy [Listener] []
      (handleEvent [evt]
        (apply callback-fn (event->map evt) args)))))

(defmacro defcommand
  [name category-id command-id label]
  `(def ^:command ~name (h/make-command ~label ~category-id ~command-id)))

(defmacro defhandler
  [name command & body]
  (let [enablement (if (= :enabled-when (first body)) (second body) nil)
        body       (if (= :enabled-when (first body)) (drop 2 body) body)
        fn-var     (first body)
        body       (rest body)]
    `(def ~name (h/make-handler ~command ~fn-var ~@body))))

(doseq [[v doc]
        {*ns*
         "Interaction with the development environment itself.

This namespace has the functions and macros you use to write
tools in the editor."

         #'defcommand
         "Create a command with the given category and id. Binds
the resulting command to the named variable.

Label should be a human-readable string. It will appear
directly in the UI (unless there is a translation for it.)

If you use the same category-id and command-id more than once,
this will create independent entities that refer to the same underlying
command."

         #'defhandler
         "Creates a handler and binds it to the given command.

In the first form, the handler will always be enabled. Upon invocation, it
will call the function bound to fn-var with the
org.eclipse.core.commands.ExecutionEvent and the additional args.

In the second form, enablement-fn will be checked. When it returns a truthy
value, the handler will be enabled. Enablement-fn must have metadata to
identify the evaluation context variables and properties that affect its
return value."}]
  (alter-meta! v assoc :doc doc))
