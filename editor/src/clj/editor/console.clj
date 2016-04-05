(ns editor.console
  (:import javafx.scene.control.TextArea))

(set! *warn-on-reflection* true)

(def ^:private console-agent (agent nil))

(defn append-console-message! [message]
  (send console-agent (fn [^TextArea node]
                        (when node
                          (.appendText node message)
                          node))))

(defn setup-console! [^TextArea node]
  (send console-agent (constantly node))
  (append-console-message! "Welcome to Defold!\n"))
