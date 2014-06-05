(ns dynamo.ui
  "Interaction with the development environment itself.

   This namespace has the functions and macros you use to
   write tools in the editor."
  (:require [internal.ui.handlers :as h]))

(defmacro defcommand
  "Create a command with the given category and id. Binds 
   the resulting command to the named variable.

   Label should be a human-readable string. It will appear 
   directly in the UI (unless there is a translation for it.)

   If you use the same category-id and command-id more than
   once, this will create independent entities that refer to
   the same underlying command."
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
      (assert (var? (resolve fn-var)) "fn-var must be a var.")
      `(def ~name (h/make-handler ~command ~fn-var ~@body))))



(comment
  
(defn lookup [event]
  (println "default variable" (.getDefaultVariable (.getApplicationContext event)))
  (println (active-editor (.getApplicationContext event)))
  println)

(defn say-hello [^ExecutionEvent event] (println "Output here."))

(defcommand a-command "com.dynamo.cr.clojure-eclipse" "com.dynamo.cr.clojure-eclipse.commands.hello" "Speak!")
(defhandler hello a-command #'say-hello)

)


