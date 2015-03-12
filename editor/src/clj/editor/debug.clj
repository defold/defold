(ns editor.debug
  (:require [clojure.tools.nrepl.server :as nrepl]
            [cider.nrepl :as cider]
            [dynamo.util :refer [applym]]))

(defonce ^:private repl-server (agent {:handler cider/cider-nrepl-handler}))

(defn- start-and-print
  [config]
  (let [srv (applym nrepl/start-server config)]
    (println (format "Started REPL at nrepl://127.0.0.1:%s\n" (:port srv)))
    srv))

(defn start-server
  [port]
  (let [repl-config (cond-> {:handler cider/cider-nrepl-handler}
                      port
                      (assoc :port port))]
      (send repl-server (constantly repl-config))
      (send repl-server start-and-print)))

(defn stop-server
  []
  (send repl-server nrepl/stop-server))
