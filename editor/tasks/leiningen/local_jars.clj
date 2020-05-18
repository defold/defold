;; Copyright 2020 The Defold Foundation
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

(ns leiningen.local-jars
  (:require [clojure.java.io :as io]
            [clojure.java.shell :as shell]
            [clojure.string :as string]
            [cemerick.pomegranate.aether :as aether]
            [leiningen.core.main :as main]
            [leiningen.util.http-cache :as http-cache])
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
  [git-sha]
  (let [f (when git-sha
            (http-cache/download (format "https://d.defold.com/archive/%s/bob/bob.jar" git-sha)))]
    (or f (io/file (dynamo-home) "share/java/bob.jar"))))

(defn local-jars
  "Install local jar dependencies into the ~/.m2 Maven repository."
  [project & [git-sha]]
  (let [sha (or git-sha (:engine project))
        jar-decls (conj jar-decls {:artifact-id "bob"
                                   :group-id "com.defold.lib"
                                   :jar-file (.getAbsolutePath (bob-artifact-file sha))
                                   :version "1.0"})]
    (doseq [{:keys [group-id artifact-id version jar-file]} (sort-by :jar-file jar-decls)]
      (main/info (format "Installing %s" jar-file))
      (aether/install-artifacts
        :files {[(symbol group-id artifact-id) version] jar-file}))))
