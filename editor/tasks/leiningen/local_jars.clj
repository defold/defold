;; Copyright 2020-2025 The Defold Foundation
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

(ns leiningen.local-jars
  (:require [cemerick.pomegranate.aether :as aether]
            [clojure.java.io :as io]
            [clojure.java.shell :as shell]
            [clojure.string :as string]
            [leiningen.core.main :as main]
            [leiningen.util.http-cache :as http-cache])
  (:import [java.io File]
           [java.nio.file CopyOption FileSystems FileVisitOption Files LinkOption Path StandardCopyOption]
           [java.nio.file.attribute FileAttribute]))

(set! *warn-on-reflection* true)

(defn dynamo-home [] (get (System/getenv) "DYNAMO_HOME"))

(def ^:private jar-decls
  [{:artifact-id "openmali"
    :group-id "com.defold.lib"
    :jar-file "../com.dynamo.cr/com.dynamo.cr.bob/lib/openmali.jar"
    :version "1.0"}])

(defn- clean-icu4j-data
  ;; Useful link:
  ;; https://unicode-org.github.io/icu/userguide/icu_data/buildtool.html#file-slicing-coarse-grained-features
  ^Path [icu4j-config]
  (let [{:keys [version]} icu4j-config
        jar (Files/createTempFile "" "-icu4j.jar" (into-array FileAttribute []))]
    (.deleteOnExit (.toFile jar))
    (with-open [fs (FileSystems/newFileSystem
                     (Files/copy
                       (-> (format "https://repo1.maven.org/maven2/com/ibm/icu/icu4j/%s/icu4j-%s.jar" version version)
                           http-cache/download
                           .toPath)
                       jar
                       ^"[Ljava.nio.file.CopyOption;" (into-array CopyOption [StandardCopyOption/REPLACE_EXISTING])))]
      (run!
        #(Files/delete %)
        (->> fs
             (.getRootDirectories)
             (eduction
               (mapcat #(.toList (Files/walk % (into-array FileVisitOption []))))
               (remove #(Files/isDirectory % (into-array LinkOption [])))
               (filter (fn [^Path p]
                         (let [p (str p)]
                           (or
                             ;; break iter (identifies word boundaries)
                             (.startsWith p "/com/ibm/icu/impl/data/icudata/brkitr")
                             ;; transliteration
                             (.startsWith p "/com/ibm/icu/impl/data/icudata/translit")
                             ;; collation (language-specific string sorting)
                             (.startsWith p "/com/ibm/icu/impl/data/icudata/coll")
                             ;; time units / measure units
                             (.startsWith p "/com/ibm/icu/impl/data/icudata/unit")
                             (= p "/com/ibm/icu/impl/data/icudata/units.res")
                             ;; time zone data
                             (.startsWith p "/com/ibm/icu/impl/data/icudata/zone")
                             (= p "/com/ibm/icu/impl/data/icudata/zoneinfo64.res")
                             (= p "/com/ibm/icu/impl/data/icudata/windowsZones.res")
                             (= p "/com/ibm/icu/impl/data/icudata/metaZones.res")
                             (= p "/com/ibm/icu/impl/data/icudata/timezoneTypes.res")
                             ;; currency data
                             (.startsWith p "/com/ibm/icu/impl/data/icudata/curr")
                             (= p "/com/ibm/icu/impl/ICUCurrencyDisplayInfoProvider.class")
                             ;; number formatting data
                             (.startsWith p "/com/ibm/icu/impl/data/icudata/rbnf")
                             (= p "/com/ibm/icu/text/NumberFormatServiceShim.class")
                             ;; unicode character names
                             (= p "/com/ibm/icu/impl/data/icudata/unames.icu")
                             ;; unicode normalization data
                             (.endsWith p ".nrm")
                             ;; unicode "string preparation"
                             (.endsWith p ".spp")))))))))
    jar))

(defn- git-sha1 []
  (-> (shell/sh "git" "rev-parse" "HEAD")
    :out
    string/trim))

(defn bob-artifact-file ^File
  [archive git-sha]
  (let [f (when git-sha
            (http-cache/download (format "https://%s/archive/%s/bob/bob.jar" archive git-sha)))]
    (or f (io/file (dynamo-home) "share/java/bob.jar"))))

(defn local-jars
  "Install local jar dependencies into the ~/.m2 Maven repository."
  [project & [git-sha]]
  (let [sha (or git-sha (:engine project))
        archive-domain (get project :archive-domain)
        jar-decls (conj jar-decls
                        {:artifact-id "bob"
                         :group-id "com.defold.lib"
                         :jar-file (.getAbsolutePath (bob-artifact-file archive-domain sha))
                         :version "1.0"}
                        {:artifact-id "icu4j"
                         :group-id "com.defold.lib"
                         :version "1.0"
                         :jar-file (str (clean-icu4j-data (:icu4j project)))})]
    (doseq [{:keys [group-id artifact-id version jar-file]} (sort-by :jar-file jar-decls)]
      (main/info (format "Installing %s as [%s \"%s\"]" jar-file (symbol group-id artifact-id) version))
      (aether/install-artifacts
        :files {[(symbol group-id artifact-id) version] jar-file}))))
