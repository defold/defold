(ns leiningen.local-jars
  (:require [clojure.java.shell :as shell]
            [clojure.string :as string]
            [cemerick.pomegranate.aether :as aether]
            [leiningen.core.main :as main])
  (:import (java.io File)
           (java.nio.file Paths)))

(def ^:private jar-decls
  [{:artifact-id "openmali"
    :group-id "com.defold.lib"
    :jar-file "../com.dynamo.cr/com.dynamo.cr.bob/lib/openmali.jar"
    :version "1.0"}])

(defn- working-directory []
  (-> (Paths/get "" (into-array String [])) .toAbsolutePath .toString))

(defn- maven-exe-path [dynamo-home]
  (string/join File/separator [dynamo-home "ext" "share" "maven" "bin" "mvn"]))

(defn- maven-install-command [mvn-exe-path {:keys [jar-file group-id artifact-id version] :as _jar-decl}]
  [mvn-exe-path
   "install:install-file"
   (str "-DartifactId=" artifact-id)
   "-DcreateChecksum=true"
   (str "-Dfile=" jar-file)
   (str "-DgroupId=" group-id)
   "-Dpackaging=jar"
   (str "-Dversion=" version)])

(defn local-jars
  "Install local jar dependencies into the ~/.m2 Maven repository."
  [_project & _arg-list]
  (doseq [{:keys [group-id artifact-id version jar-file]} (sort-by :jar-file jar-decls)]
       (main/info (format "Installing %s" jar-file))
       (aether/install-artifacts
         :files {[(symbol group-id artifact-id) version] jar-file})))
