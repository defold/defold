(ns editor
  (:import [javafx.application Application]))

(defn -main [& args]
  (println "Launching")
  (Application/launch (Class/forName "com.defold.editor.Start") args))
