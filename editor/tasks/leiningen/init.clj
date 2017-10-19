(ns leiningen.init
  (:require
   [clojure.java.shell :as sh]
   [clojure.string :as string]
   [leiningen.core.main :as main]))

(defn- sha-from-version-file
  []
  (let [stable-version (slurp "../VERSION")
        {:keys [exit out err]} (sh/sh "git" "rev-list" "-n" "1" stable-version)]
    (if (zero? exit)
      (string/trim out)
      (throw (ex-info (format "Unable to determine engine version sha from version file, was '%s'" stable-version)
                      {:exit exit
                       :out out
                       :err err})))))

(defn- sha-from-ref
  [ref]
  (let [{:keys [exit out err]} (sh/sh "git" "rev-parse" ref)]
    (if (zero? exit)
      (string/trim out)
      (throw (ex-info (format "Unable to determine engine version sha for ref '%s'" ref)
                      {:ref ref
                       :exit exit
                       :out out
                       :err err})))))

(defn- autodetect-sha
  []
  (try
    (sha-from-version-file)
    (catch clojure.lang.ExceptionInfo e
      (sha-from-ref "HEAD"))))

(defn- resolve-version
  [version]
  (when version
    (case version
      "auto"            (autodetect-sha)
      "dynamo-home"     nil ; for symmetry
      "archived-stable" (sha-from-version-file)
      "archived"        (sha-from-ref "HEAD")
      version)))

(def init-tasks
  [["clean"]
   ["local-jars"]
   ["builtins"]
   ["protobuf"]
   ["sass" "once"]
   ["pack"]])

(defn init
  "Initialise project with required engine artifacts based on `version`, or
  $DYNAMO_HOME if none given.

  Arguments:
    - version: dynamo-home, archived, archived-stable or a sha."
  [project & [version]]
  (let [git-sha (resolve-version version)
        project (assoc project :engine git-sha)]
    (println (format "Initializing editor with version '%s', resolved to '%s'" version git-sha))
    (doseq [task+args init-tasks]
      (main/resolve-and-apply project task+args))))
