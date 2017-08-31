(ns leiningen.builtins
  (:require [clojure.java.io :as io]
            [leiningen.util.http-cache :as http-cache]))

(defn builtins [project & [git-sha]]
  (let [f (when git-sha
            (http-cache/download (format "https://d.defold.com/archive/%s/engine/share/builtins.zip" git-sha)))]
    (io/copy (or f (io/file (format "%s/share/builtins.zip" (get (System/getenv) "DYNAMO_HOME"))))
      (io/file "generated-resources/builtins.zip"))))
