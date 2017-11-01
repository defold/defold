(ns leiningen.builtins
  (:require [clojure.java.io :as io]
            [leiningen.util.http-cache :as http-cache]))

(defn- builtins-zip
  [sha]
  (if sha
    (http-cache/download (format "https://d.defold.com/archive/%s/engine/share/builtins.zip" sha))
    (io/file (format "%s/share/builtins.zip" (get (System/getenv) "DYNAMO_HOME")))))

(defn builtins [project & [git-sha]]
  (let [sha (or git-sha (:engine project))]
    (io/copy (builtins-zip sha)
             (io/file "generated-resources/builtins.zip"))))
