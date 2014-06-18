(ns internal.ui.handlers
  "Implementation support for handlers and commands."
  (:require [eclipse.reflect :as r]
            [service.log :as log]
            [service.registry :refer [registered]])
  (:import [org.eclipse.core.expressions IEvaluationContext]
           [org.eclipse.core.commands AbstractHandler ExecutionEvent]
           [org.eclipse.ui.commands ICommandService]
           [org.eclipse.ui.handlers IHandlerService IHandlerActivation]
           [org.eclipse.ui ISources PlatformUI]))


(defn snake-case [s] (.replaceAll (.toLowerCase s) "_" "-"))

(defn context-accessor-sexp
  [fld]
  (let [fld-name (:name (first fld))
        fn-name (symbol (snake-case (subs fld-name 0 (- (count fld-name) 5))))]
    `(defn ~fn-name [^IEvaluationContext ctx#]
       (.getVariable ctx# ~(second fld)))))

(defn context-variable-fields
  [cls]
  (filter #(.endsWith (:name (first %)) "_NAME")
          (r/constants (resolve cls))))

(defmacro context-accessors 
  [cls]
  (let [accessors (clojure.core/map context-accessor-sexp (context-variable-fields cls))]
    (apply list 'do `(def ^:private context-accessor-fns '~(mapv second accessors)) accessors)))

(context-accessors org.eclipse.ui.ISources)

(defn global-handler-service [] (.getAdapter (PlatformUI/getWorkbench) IHandlerService))
(defn global-command-service [] (.getAdapter (PlatformUI/getWorkbench) ICommandService))

(defn- command  [command-id]  (.getCommand  (global-command-service) command-id))
(defn- category [category-id] (.getCategory (global-command-service) category-id))

(defrecord Command [nm category id real-command])

(defn make-command
  [category-id command-id nm]
  (Command. nm category-id command-id
            (doto (command command-id)
              (.define nm nil (category category-id)))))

(defn- deactivate-handler
  [^IHandlerActivation activation]
  (.deactivateHandler (global-handler-service) activation))

(defn- activate-handler
  [command-id handler]
  (.activateHandler (global-handler-service) command-id handler))

(defn- handler-proxy 
  [fn-var args]
  (proxy [AbstractHandler] []
    (execute [^ExecutionEvent execution-event]
      (apply (var-get fn-var) execution-event args))
    
    (setEnabled [evaluation-context]
      (println evaluation-context))))

(defrecord Handler [command-id fn-var args activation])

(def make-handler
  (registered
    (fn [^Command command fn-var & args]
      (Handler. (:id command) fn-var args 
                (activate-handler (:id command) (handler-proxy fn-var args))))
    (fn [^Command command fn-var & args]
      [(:id command) (str fn-var)])
    (fn [^Handler h]
      [(:command-id h) (str (:fn-var h))])
    (fn [^Handler h]
      (deactivate-handler (:activation h)))))

