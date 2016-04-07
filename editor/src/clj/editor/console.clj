(ns editor.console
  (:import javafx.scene.control.TextArea)
  (:require [editor.ui :as ui]))

(set! *warn-on-reflection* true)

(def ^:private console-node (atom nil))

(defn append-console-message! [message]
  (when-let [^TextArea node @console-node]
    (ui/run-later (.appendText node message))))

(defn setup-console! [^TextArea node]
  (reset! console-node node)
  (append-console-message! "Welcome to Defold!\n"))
