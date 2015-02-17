(ns editor.debug
  (:require [clojure.tools.nrepl.server :as nrepl]))

(defonce repl-server (atom nil))

(defn start-server
  []
  (let [server (reset! repl-server (nrepl/start-server))]
    (println (format "Started REPL at nrepl://127.0.0.1:%s\n" (:port server)))))

(defn stop-server
  []
  (when-let [server @repl-server]
    (nrepl/stop-server server)
    (reset! repl-server nil)))
