(ns editor.debug
  (:require [cider.nrepl :as cider]
            [clojure.tools.nrepl.server :as nrepl]
            [refactor-nrepl.middleware :as refactor]))

(defonce ^:private repl-server (agent nil))

(defn- start-and-print
  [config]
  (let [srv (apply nrepl/start-server (mapcat identity config))]
    (println (format "Started REPL at nrepl://127.0.0.1:%s\n" (:port srv)))
    srv))

(defn start-server
  [port]
  (let [repl-config (cond-> {:handler (refactor/wrap-refactor cider/cider-nrepl-handler)}
                      port
                      (assoc :port port))]
      (send repl-server (constantly repl-config))
      (send repl-server start-and-print)))

(defn stop-server
  []
  (send repl-server nrepl/stop-server))
