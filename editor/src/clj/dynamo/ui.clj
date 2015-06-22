(ns dynamo.ui
  "This namespace has the functions and macros you use to interact with the GUI.
  - defcommand and defhandler let you define menu items and actions"
  (:require [dynamo.types :as t]
            [internal.java :refer [bean-mapper]]
            [service.log :as log])
  (:import [javafx.animation AnimationTimer]
           [javafx.application Platform]
           [javafx.event EventHandler WeakEventHandler]))

(defn mouse-type
  []
  :one-button)

(defn- on-ui-thread?
  []
  (Platform/isFxApplicationThread))

(defmacro on-app-thread
  [& body]
  `(if (on-ui-thread?)
     (do ~@body)
     (Platform/runLater
      (bound-fn [] (do ~@body)))))

(defn make-animation-timer
  [f & args]
  (proxy [AnimationTimer] []
    (handle [now] (apply f now args))))

(defn run-wait
  [f]
  (let [result (promise)]
    (on-app-thread
     (deliver result (f)))
    @result))

(defmacro run-safe
  [& body]
  `(Platform/runLater
    (fn [] ~@body)))

(defn handle
  [f]
  (reify EventHandler
    (handle [this event] (f event))))

(defn weak [^EventHandler h]
  (WeakEventHandler. h))

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
    #_(swt-await
      (doseq [l (vals @listeners)]
       (run-safe
         (l event))))))

(defn make-event-broadcaster [] (EventBroadcaster. (atom {})))

(defmacro defcommand
  "Create a command with the given category and id. Binds
the resulting command to the named variable.

Label should be a human-readable string. It will appear
directly in the UI (unless there is a translation for it.)

If you use the same category-id and command-id more than once,
this will create independent entities that refer to the same underlying
command."
  [name category-id command-id label]
  `(def ^:command ~name [~label ~category-id ~command-id]))

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
    `(def ^:handler ~name [~command ~fn-var ~@body])))
