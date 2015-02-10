(ns editor.debug
  (:require [clojure.tools.nrepl.server :as nrepl]))

(let [server (nrepl/start-server)]
 (println (format "Started REPL at nrepl://127.0.0.1:%s\n" (:port server))))
