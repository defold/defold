;; Copyright 2020-2022 The Defold Foundation
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

(ns leiningen.pack
  (:require
   [clojure.java.io :as io]
   [clojure.string :as str]
   [leiningen.util.http-cache :as http-cache])
  (:import
   (java.util.zip ZipFile)
   (org.apache.commons.io FileUtils)))

(defn dynamo-home [] (get (System/getenv) "DYNAMO_HOME"))

;; these can be sourced either from a local build of engine, or downloaded from an archived build on s3
(def engine-artifacts
  {"x86_64-macos" {"bin" ["dmengine"]
                    "lib" ["libparticle_shared.dylib"]}
   "x86-win32"     {"bin" ["dmengine.exe" "dmengine.pdb"]
                    "lib" []}
   "x86_64-win32"  {"bin" ["dmengine.exe" "dmengine.pdb"]
                    "lib" ["particle_shared.dll"]}
   "x86_64-linux"  {"bin" ["dmengine"]
                    "lib" ["libparticle_shared.so"]}
   "arm64-ios"  {"bin" ["dmengine"]
                    "lib" []}})

(defn- platform->engine-src-dirname [platform]
  (assert (contains? engine-artifacts platform))
  (case platform
    "x86-win32" "win32"
    platform))

(def artifacts
  {"${DYNAMO-HOME}/ext/lib/win32/OpenAL32.dll"        "x86-win32/bin/OpenAL32.dll"
   "${DYNAMO-HOME}/ext/lib/win32/wrap_oal.dll"        "x86-win32/bin/wrap_oal.dll"

   "${DYNAMO-HOME}/ext/bin/win32/luajit-32.exe"        "x86_64-win32/bin/luajit-32.exe"
   "${DYNAMO-HOME}/ext/bin/x86_64-win32/luajit-64.exe" "x86_64-win32/bin/luajit-64.exe"
   "${DYNAMO-HOME}/ext/lib/x86_64-win32/OpenAL32.dll"  "x86_64-win32/bin/OpenAL32.dll"
   "${DYNAMO-HOME}/ext/lib/x86_64-win32/wrap_oal.dll"  "x86_64-win32/bin/wrap_oal.dll"

   "${DYNAMO-HOME}/ext/bin/x86_64-linux/luajit-32"            "x86_64-linux/bin/luajit-32"
   "${DYNAMO-HOME}/ext/bin/x86_64-linux/luajit-64"            "x86_64-linux/bin/luajit-64"

   "${DYNAMO-HOME}/ext/bin/x86_64-macos/luajit-32"           "x86_64-macos/bin/luajit-32"
   "${DYNAMO-HOME}/ext/bin/x86_64-macos/luajit-64"           "x86_64-macos/bin/luajit-64"

   "$DYNAMO_HOME/ext/bin/x86_64-macos/glslc"                  "x86_64-macos/glslc"
   "$DYNAMO_HOME/ext/bin/x86_64-linux/glslc"                  "x86_64-linux/glslc"
   "$DYNAMO_HOME/ext/bin/x86_64-win32/glslc.exe"              "x86_64-win32/glslc.exe"

   "$DYNAMO_HOME/ext/bin/x86_64-macos/spirv-cross"            "x86_64-macos/spirv-cross"
   "$DYNAMO_HOME/ext/bin/x86_64-linux/spirv-cross"            "x86_64-linux/spirv-cross"
   "$DYNAMO_HOME/ext/bin/x86_64-win32/spirv-cross.exe"        "x86_64-win32/spirv-cross.exe"

   "${DYNAMO-HOME}/ext/share/luajit"                  "shared/luajit"

   "bundle-resources/_defold"                          "_defold"})

(defn engine-artifact-files
  [archive-domain git-sha]
  (into {} (for [[platform dirs] engine-artifacts
                 [dir files] dirs
                 file files]
             (let [engine-src-dirname (platform->engine-src-dirname platform)
                   src (if (some? git-sha)
                         (http-cache/download (format "https://%s/archive/%s/engine/%s/%s" archive-domain git-sha engine-src-dirname file))
                         (io/file (dynamo-home) dir engine-src-dirname file))
                   dest (io/file platform dir file)]
               [src dest]))))

(defn artifact-files
  []
  (let [subst (fn [s] (.replace s "${DYNAMO-HOME}" (dynamo-home)))]
    (into {} (for [[src dest] artifacts]
               [(io/file (subst src)) (io/file (subst dest))]))))

;; Manually re-pack JOGL natives, so we can avoid JOGLs automatic
;; library loading, see DEFEDIT-494.

(def java-platform->platform
  {"linux-amd64"      "x86_64-linux"
   "macosx-universal" "x86_64-macos"
   "windows-amd64"    "x86_64-win32"
   "windows-i586"     "x86-win32"
   "windows-x64"      "x86_64-win32"})

(defn jar-file
  [[artifact version & {:keys [classifier]} :as dependency]]
  (io/file (str (System/getProperty "user.home")
                "/.m2/repository/"
                (str/replace (namespace artifact) "." "/")
                "/" (name artifact)
                "/" version
                "/" (name artifact) "-" version
                (some->> classifier name (str "-")) ".jar")))

(defn jogl-native-dep?
  [[artifact version & {:keys [classifier]}]]
  (and (#{'com.metsci.ext.org.jogamp.gluegen/gluegen-rt
          'com.metsci.ext.org.jogamp.jogl/jogl-all} artifact)
       classifier))

(defn extract-jogl-native-dep
  [[artifact version & {:keys [classifier]} :as dependency] pack-path]
  (let [java-platform (str/replace-first classifier "natives-" "")
        natives-path (str "natives/" java-platform)]
    (with-open [zip-file (ZipFile. (jar-file dependency))]
      (doseq [entry (enumeration-seq (.entries zip-file))]
        (when (.startsWith (.getName entry) natives-path)
          (let [libname (.getName (io/file (.getName entry)))
                dest (io/file pack-path (java-platform->platform java-platform) "lib" libname)]
            (println (format "extracting '%s'/'%s' to '%s'" (.getName zip-file) (.getName entry) dest))
            (io/make-parents dest)
            (io/copy (.getInputStream zip-file entry) dest)))))))

(defn pack-jogl-natives
  [pack-path dependencies]
  (doseq [jogl-native-dep (filter jogl-native-dep? dependencies)]
    (extract-jogl-native-dep jogl-native-dep pack-path)))

(defn copy-artifacts
  [pack-path archive-domain git-sha]
  (let [files (merge (engine-artifact-files archive-domain git-sha)
                     (artifact-files))]
    (doseq [[src dest] files]
      (let [dest (io/file pack-path dest)]
        (println (format "copying '%s' to '%s'" (str src) (str dest)))
        (if-not (.exists src)
          (println "skipping non-existent" (str src))
          (if (.isDirectory src)
            (FileUtils/copyDirectory src dest)
            (FileUtils/copyFile src dest)))))))

(defn pack
  "Pack all files that need to be unpacked at runtime into `pack-path`."
  [{:keys [dependencies packing] :as project} & [git-sha]]
  (let [sha (or git-sha (:engine project))
        archive-domain (get project :archive-domain)
        {:keys [pack-path]} packing]
    (FileUtils/deleteQuietly (io/file pack-path))
    (copy-artifacts pack-path archive-domain sha)
    (pack-jogl-natives pack-path dependencies)))
