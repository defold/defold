(ns editor.ui
  (:import [javafx.event ActionEvent EventHandler]))

(defmacro event-handler [event & body]
  `(reify EventHandler
     (handle [this ~event]
       ~@body)))
