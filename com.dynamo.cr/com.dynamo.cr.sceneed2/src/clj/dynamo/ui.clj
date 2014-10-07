(ns dynamo.ui
  (:require [clojure.core.async :refer [chan dropping-buffer put!]]
            [internal.ui.handlers :as h]
            [internal.java :refer [bean-mapper]]
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
  (when-let [cur (display)]
    (if (= (Thread/currentThread) (.getThread cur))
      (.timerExec cur after f)
      (swt-thread-safe* #(.timerExec cur after f)))))

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

(def event->map (bean-mapper org.eclipse.swt.widgets.Event))

(deftype EventForwarder [ch]
  Listener
  (handleEvent [this evt]
    (put! ch (update-in (event->map evt) [:type] event-type-map))))

(defn listen
  [^Widget component type callback-fn & args]
  (let [f (bound-fn [evt] (apply callback-fn evt args))]
    (.addListener component (event-map type)
      (proxy [Listener] []
        (handleEvent [evt]
          (f evt))))))

(defn make-event-channel
  []
  (chan (dropping-buffer 100)))

(defn pipe-events-to-channel
  [^Widget control type ch]
  (.addListener control (event-map type)
    (EventForwarder. ch)))

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

(def ^:dynamic *view* nil)

(defprotocol Repaintable
  (request-repaint [this]))

(defn repaint-current-view []
  (when *view*
    (request-repaint *view*)))

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
return value."

         #'Repaintable
         "A type that satisfies this protocol can be repainted on demand."

         #'request-repaint
         "Schedule a repaint to occur at some time in the future."}]
  (alter-meta! v assoc :doc doc))
