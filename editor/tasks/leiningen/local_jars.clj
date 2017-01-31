(ns leiningen.local-jars
  (:require [clojure.java.shell :as shell]
            [clojure.string :as string])
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
  (if-let [dynamo-home (System/getenv "DYNAMO_HOME")]
    (let [mvn-exe-path (maven-exe-path dynamo-home)]
      (doseq [jar-decl (sort-by :jar-file jar-decls)]
        (let [mvn-command (maven-install-command mvn-exe-path jar-decl)
              {:keys [exit out err] :as result} (apply shell/sh mvn-command)]
          (print out)
          (print err)
          (when (not= 0 exit)
            (let [work-dir (working-directory)
                  command (string/join " " mvn-command)]
              (println (str "From \"" work-dir "\""))
              (println (str "Command \"" command "\""))
              (println "Failed with exit code" exit)
              (println "When processing" (pr-str jar-decl))
              (throw (ex-info (str "Failed to install required jar \"" (:jar-file jar-decl) "\" into local maven repository.")
                              {:command command
                               :exit-code exit
                               :jar-decl jar-decl
                               :stderr err
                               :stdout out
                               :work-dir work-dir}))))
          exit)))
    (throw (ex-info "The DYNAMO_HOME environment variable must be set." {}))))
