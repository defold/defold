(ns editor
  (:import [javafx.application Application]
           [javafx.stage Stage])
  (:gen-class :name editor.Main
              :extends javafx.application.Application))

(defn -start
  [this ^Stage primary-stage]
  (eval
   '(do
      (require 'editor.debug)
      (editor.debug/start-server)
      (require 'dynamo.messages)
      (require 'editor.boot))))

(defn -main [& args]
  (println "Launching")
  (Application/launch (Class/forName "editor.Main") args))
