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

(ns leiningen.init
  (:require
   [clojure.java.shell :as sh]
   [clojure.string :as string]
   [leiningen.core.main :as main]))

(defn- sha-from-version-file
  []
  (let [stable-version (first (string/split-lines (slurp "../VERSION")))
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

(defn- resolve-archive-domain
  [user_domain]
  (let [default_domain "d.defold.com"
        env_domain (System/getenv "DM_ARCHIVE_DOMAIN")]
    (or user_domain env_domain default_domain)))

(def init-tasks
  [["clean"]
   ["local-jars"]
   ; ["builtins"] builtins is in bob which we install in `local-jars` step
   ["ref-doc"]
   ["project-templates"]
   ["protobuf"]
   ["sass" "once"]
   ["pack"]])

(defn init
  "Initialise project with required engine artifacts based on `version`, or
  $DYNAMO_HOME if none given.

  Arguments:
    - version: dynamo-home, archived, archived-stable or a sha.
    - archive-domain: domain where engine artifacts are stored"
  [project & [version archive-domain]]
  (let [git-sha (resolve-version version)
        archive-domain (resolve-archive-domain archive-domain)
        project (assoc project :archive-domain archive-domain :engine git-sha)]
    (println (format "Initializing editor with version '%s', resolved to '%s'. Using archive domain '%s'." version (get project :engine) (get project :archive-domain)))
    (doseq [task+args init-tasks]
      (main/resolve-and-apply project task+args))))
