(ns leiningen.local-jars
  (:require [clojure.java.io :as io]
            [clojure.java.shell :as shell]
            [clojure.string :as string]
            [cemerick.pomegranate.aether :as aether]
            [leiningen.core.main :as main]
            [leiningen.util.download :as dl])
  (:import (java.io File)
           (java.nio.file Paths)))

(defn dynamo-home [] (get (System/getenv) "DYNAMO_HOME"))

(def ^:private jar-decls
  [{:artifact-id "openmali"
    :group-id "com.defold.lib"
    :jar-file "../com.dynamo.cr/com.dynamo.cr.bob/lib/openmali.jar"
    :version "1.0"}])

(defn- git-sha1 []
  (-> (shell/sh "git" "rev-parse" "HEAD")
    :out
    string/trim))

(defn bob-artifact-file ^File
  [sha]
  (let [url (format "http://d.defold.com/archive/%s/bob/bob.jar" sha)]
    (try
      (dl/download (io/as-url url))
      (catch java.io.FileNotFoundException e
        ;; Fallback to local bob
        (println (format "- %s could not be found" url))
        (io/file (dynamo-home) "share/java/bob.jar")))))

(defn local-jars
  "Install local jar dependencies into the ~/.m2 Maven repository."
  [_project]
  (let [jar-decls (conj jar-decls {:artifact-id "bob"
                                      :group-id "com.defold.lib"
                                      :jar-file (.getAbsolutePath (bob-artifact-file (git-sha1)))
                                      :version "1.0"})]
    (doseq [{:keys [group-id artifact-id version jar-file]} (sort-by :jar-file jar-decls)]
      (main/info (format "Installing %s" jar-file))
      (aether/install-artifacts
        :files {[(symbol group-id artifact-id) version] jar-file}))))
