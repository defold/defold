(ns editor.debug)

(set! *warn-on-reflection* true)

(defonce ^:private repl-server (agent nil))

;; Try/catching the requires here because the dependencies might be missing

(defn- start-and-print
  [config]
  (try
    (require 'clojure.tools.nrepl.server)
    (let [start-server (resolve 'clojure.tools.nrepl.server/start-server)
          srv          (if config
                         (apply start-server (mapcat identity config))
                         (start-server))]
      (println (format "Started REPL at nrepl://127.0.0.1:%s\n" (:port srv)))
      srv)
    (catch Exception _
      (println "Failed to start NREPL server"))))

(defn start-server
  [port]
  (try
    (require 'cider.nrepl)
    (require 'refactor-nrepl.middleware)
    (let [wrap-refactor (resolve 'refactor-nrepl.middleware/wrap-refactor)
          cider-handler (resolve 'cider.nrepl/cider-nrepl-handler)
          repl-config   (cond-> {:handler (wrap-refactor cider-handler)}
                          port (merge {:port port}))]
      (send repl-server (constantly repl-config)))
    (catch Exception _
      (println "Failed to load NREPL middlewares")))
  (send repl-server start-and-print))

(defn stop-server
  []
  (try
    (require 'clojure.tools.nrepl.server)
    (let [stop-server (resolve 'clojure.tools.nrepl.server/stop-server)]
      (send repl-server stop-server))
    (catch Exception _)))
