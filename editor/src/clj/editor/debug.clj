;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.debug
  (:require [editor.fs :as fs]
            [editor.ns-batch-builder :as bb]
            [editor.system :as system]))

(set! *warn-on-reflection* true)

(defonce ^:private repl-server (agent nil))
(defonce should-write-nrepl-port-file (atom (system/defold-dev?)))

(defn ^:private write-nrepl-port-file
  [port]
  (when @should-write-nrepl-port-file
    (reset! should-write-nrepl-port-file false)
    (let [nrepl-port-file (java.io.File. ".nrepl-port")]
      (try
        (spit nrepl-port-file (str port))
        (fs/delete-on-exit! nrepl-port-file)
        (catch Exception e
          (println (format "Failed to write NREPL port file to `%s`" (.getAbsolutePath nrepl-port-file)))
          (prn e))))))

;; Try/catching the requires here because the dependencies might be missing

(defn- try-resolve
  [sym]
  (try
    (require (symbol (namespace sym)))
    (resolve sym)
    (catch Exception _ false)))

(defn- start-and-print
  [config]
  (if-let [start-server (or (try-resolve 'clojure.tools.nrepl.server/start-server)
                            (try-resolve 'nrepl.server/start-server))]
    (try
      (let [srv (if config
                  (apply start-server (mapcat identity config))
                  (start-server))]
        (println (format "Started REPL at nrepl://127.0.0.1:%s\n" (:port srv)))
        (write-nrepl-port-file (:port srv))
        srv)
      (catch Exception e
        (println "Failed to start NREPL server")
        (prn e)))
    (println "NREPL library not found, not starting")))

(defn- maybe-load-refactor-nrepl
  [repl-config]
  (if-let [wrap-refactor (try-resolve 'refactor-nrepl.middleware/wrap-refactor)]
    (do (println "Adding refactor-nrepl middleware")
        (update repl-config :handler wrap-refactor))
    repl-config))

(defn- maybe-load-sayid
  [repl-config]
  (if-let [wrap-sayid (try-resolve 'com.billpiel.sayid.nrepl-middleware/wrap-sayid)]
    (do (println "Adding sayid middleware")
      (update repl-config :handler wrap-sayid))
    repl-config))

(defn- maybe-load-cider
  [repl-config]
  (if-let [cider-handler (try-resolve 'cider.nrepl/cider-nrepl-handler)]
    (do (println "Using CIDER nrepl handler")
        (assoc repl-config :handler cider-handler))
    repl-config))

(defn start-server
  [port]
  (let [repl-config (cond-> {:bind "localhost"}
                      true (maybe-load-cider)
                      true (maybe-load-refactor-nrepl)
                      true (maybe-load-sayid)
                      port (assoc :port port))]
    (send repl-server (constantly repl-config))
    (send repl-server start-and-print)))

(defn stop-server
  []
  (try
    (require 'clojure.tools.nrepl.server)
    (let [stop-server (resolve 'clojure.tools.nrepl.server/stop-server)]
      (send repl-server stop-server))
    (catch Exception _)))

(defn init-debug
  []
  (bb/spit-batches "resources/sorted_clojure_ns_list.edn")
  (when (= "true" (System/getProperty "defold.nrepl"))
    (start-server nil)))

