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

(ns leiningen.pack
  (:require [clojure.java.io :as io]
            [clojure.string :as str]
            [leiningen.util.http-cache :as http-cache])
  (:import [java.io File]
           [java.util.zip ZipEntry ZipFile]
           [org.apache.commons.compress.archivers ArchiveEntry ArchiveInputStream]
           [org.apache.commons.compress.archivers.tar TarArchiveEntry TarArchiveInputStream]
           [org.apache.commons.compress.archivers.zip ZipArchiveEntry ZipArchiveInputStream]
           [org.apache.commons.compress.compressors.gzip GzipCompressorInputStream]
           [org.apache.commons.io FileUtils]))

(set! *warn-on-reflection* true)

(defn dynamo-home [] (get (System/getenv) "DYNAMO_HOME"))

;; these can be sourced either from a local build of engine, or downloaded from an archived build on s3
(def engine-artifacts
  {"x86_64-macos" {"bin" ["dmengine"]
                   "lib" ["libparticle_shared.dylib"
                          "libmouse_capture_shared.dylib"]}
   "arm64-macos" {"bin" ["dmengine"]
                  "lib" ["libparticle_shared.dylib"
                         "libmouse_capture_shared.dylib"]}
   "x86_64-win32"  {"bin" ["dmengine.pdb"]
                    "lib" ["particle_shared.dll"
                           "mouse_capture_shared.dll"]}
   "x86_64-linux"  {"bin" ["dmengine"]
                    "lib" ["libparticle_shared.so"
                           "libmouse_capture_shared.so"]}})

(def known-platforms (vec (keys engine-artifacts)))

(defn- selected-platforms
  [target-platform]
  (if (some? target-platform)
    (if (contains? engine-artifacts target-platform)
      [target-platform]
      (throw (ex-info (format "Unknown target platform '%s'. Expected one of: %s"
                              target-platform
                              (str/join ", " (sort known-platforms)))
                      {:target-platform target-platform
                       :known-platforms known-platforms})))
    known-platforms))

(defn- platform->engine-src-dirname [platform]
  (assert (contains? engine-artifacts platform))
  (case platform
    "x86-win32" "win32"
    platform))

(def artifacts
  {
   ; These artifacts are equal to the artifacts from bob
   ; see ResourceUnpacker.java for more info
   ; "${DYNAMO-HOME}/ext/lib/win32/OpenAL32.dll"          "x86-win32/bin/OpenAL32.dll"
   ; "${DYNAMO-HOME}/ext/lib/win32/wrap_oal.dll"          "x86-win32/bin/wrap_oal.dll"

   ; "${DYNAMO-HOME}/ext/bin/x86_64-win32/luajit-64.exe"  "x86_64-win32/bin/luajit-64.exe"
   ; "${DYNAMO-HOME}/ext/lib/x86_64-win32/OpenAL32.dll"   "x86_64-win32/bin/OpenAL32.dll"
   ; "${DYNAMO-HOME}/ext/lib/x86_64-win32/wrap_oal.dll"   "x86_64-win32/bin/wrap_oal.dll"

   ;"${DYNAMO-HOME}/ext/bin/x86_64-linux/luajit-64"       "x86_64-linux/bin/luajit-64"
   ;"${DYNAMO-HOME}/ext/bin/x86_64-macos/luajit-64"       "x86_64-macos/bin/luajit-64"
   ;"${DYNAMO-HOME}/ext/bin/arm64-macos/luajit-64"        "arm64-macos/bin/luajit-64"

   ;"${DYNAMO-HOME}/ext/bin/x86_64-macos/glslang"        "x86_64-macos/glslang"
   ;"${DYNAMO-HOME}/ext/bin/arm64-macos/glslang"         "arm64-macos/glslang"
   ;"${DYNAMO-HOME}/ext/bin/x86_64-linux/glslang"        "x86_64-linux/glslang"
   ;"${DYNAMO-HOME}/ext/bin/x86_64-win32/glslang.exe"    "x86_64-win32/glslang.exe"

   ;"${DYNAMO-HOME}/ext/bin/x86_64-linux/tint"           "x86_64-linux/tint"
   ;"${DYNAMO-HOME}/ext/bin/x86_64-macos/tint"           "x86_64-macos/tint"
   ;"${DYNAMO-HOME}/ext/bin/arm64-macos/tint"            "arm64-macos/tint"
   ;"${DYNAMO-HOME}/ext/bin/x86_64-win32/tint.exe"       "x86_64-win32/tint.exe"

   "resources/lua-annotations"                         "shared/lua-annotations"
   "resources/lua-language-server"                     "shared/lua-language-server"
   "${DYNAMO-HOME}/ext/share/luajit"                    "shared/luajit"

   "bundle-resources/_defold"                           "_defold"})

(defn engine-artifact-files
  [archive-domain git-sha selected-platforms]
  (into {} (for [platform selected-platforms
                 :let [dirs (engine-artifacts platform)]
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
  (let [subst (fn [s] (str/replace s "${DYNAMO-HOME}" (dynamo-home)))]
    (into {} (for [[src dest] artifacts]
               [(io/file (subst src)) (io/file (subst dest))]))))

;; Manually re-pack JOGL natives, so we can avoid JOGLs automatic
;; library loading, see DEFEDIT-494.

(def jogl-classifier->platforms
  {"linux-amd64"      ["x86_64-linux"]
   "macosx-universal" ["arm64-macos" "x86_64-macos"]
   "windows-amd64"    ["x86_64-win32"]
   "windows-x64"      ["x86_64-win32"]})

(defn jar-file
  ^File [local-repo [artifact version & {:keys [classifier]}]]
  (io/file local-repo
           (str/replace (namespace artifact) "." "/")
           (name artifact)
           version
           (str (name artifact) "-" version
                (some->> classifier name (str "-")) ".jar")))

(defn jogl-native-dep?
  [[artifact version & {:keys [classifier]}]]
  (and (#{'org.jogamp.gluegen/gluegen-rt
          'org.jogamp.jogl/jogl-all} artifact)
       classifier))

(defn extract-jogl-native-dep
  [local-repo [_ _ & {:keys [classifier]} :as dependency] pack-path selected-platforms]
  (let [java-platform (str/replace-first classifier "natives-" "")
        natives-path (str "natives/" java-platform)]
    (with-open [zip-file (ZipFile. (jar-file local-repo dependency))]
      (doseq [^ZipEntry entry (enumeration-seq (.entries zip-file))]
        (when (.startsWith (.getName entry) natives-path)
          (let [libname (.getName (io/file (.getName entry)))]
            (doseq [target-platform (filter (set selected-platforms)
                                            (jogl-classifier->platforms java-platform))]
              (let [dest (io/file pack-path target-platform "lib" libname)]
                (println (format "extracting '%s'/'%s' to '%s'" (.getName zip-file) (.getName entry) dest))
                (io/make-parents dest)
                (io/copy (.getInputStream zip-file entry) dest)))))))))

(defn pack-jogl-natives
  [pack-path local-repo dependencies selected-platforms]
  (doseq [jogl-native-dep (filter jogl-native-dep? dependencies)]
    (extract-jogl-native-dep local-repo jogl-native-dep pack-path selected-platforms)))

(defn pack-lua-language-server [pack-path lua-language-server-version selected-platforms]
  (doseq [platform selected-platforms
          :let [[release-platform extension] (case platform
                                               "x86_64-macos" ["darwin-x64" "tar.gz"]
                                               "arm64-macos" ["darwin-arm64" "tar.gz"]
                                               "x86_64-linux" ["linux-x64" "tar.gz"]
                                               "x86_64-win32" ["win32-x64" "zip"])
                archive-file (-> (format "https://github.com/LuaLS/lua-language-server/releases/download/%s/lua-language-server-%s-%s.%s"
                                         lua-language-server-version
                                         lua-language-server-version
                                         release-platform
                                         extension)
                                 http-cache/download)
                output-dir (.getCanonicalFile (io/file pack-path platform "bin" "lsp" "lua"))]]
    (with-open [^ArchiveInputStream input
                (case extension
                  "tar.gz" (-> archive-file io/input-stream GzipCompressorInputStream. TarArchiveInputStream.)
                  "zip" (-> archive-file io/input-stream ZipArchiveInputStream.))]
      (loop []
        (when-let [^ArchiveEntry entry (.getNextEntry input)]
          (when-not (.isDirectory entry)
            (let [output (.getCanonicalFile (io/file output-dir (.getName entry)))]
              (when-not (.startsWith (.toPath output) (.toPath output-dir))
                (throw (ex-info "Archive entry is outside the destination directory"
                                {:entry (.getName entry)})))
              (io/make-parents output)
              (io/copy input output)
              (when (pos? (bit-and (case extension
                                     "tar.gz" (.getMode ^TarArchiveEntry entry)
                                     "zip" (.getUnixMode ^ZipArchiveEntry entry))
                                   2r001000000))
                (.setExecutable output true))))
          (recur))))))

(defn copy-artifacts
  [pack-path archive-domain git-sha selected-platforms]
  (let [files (merge (engine-artifact-files archive-domain git-sha selected-platforms)
                     (artifact-files))]
    (doseq [[^File src dest] files]
      (let [dest (io/file pack-path dest)]
        (println (format "copying '%s' to '%s'" (str src) (str dest)))
        (if-not (.exists src)
          (println "skipping non-existent" (str src))
          (if (.isDirectory src)
            (FileUtils/copyDirectory src dest)
            (FileUtils/copyFile src dest)))))))

(defn pack
  "Pack all files that need to be unpacked at runtime into `pack-path`."
  [{:keys [dependencies local-repo packing] :as project} & [git-sha]]
  (let [sha (or git-sha (:engine project))
        archive-domain (get project :archive-domain)
        {:keys [pack-path lua-language-server-version target-platform]} packing
        platforms-to-pack (selected-platforms target-platform)]
    (when-not local-repo
      (throw (ex-info "Missing project :local-repo" {:task "pack"})))
    (FileUtils/deleteQuietly (io/file pack-path))
    (copy-artifacts pack-path archive-domain sha platforms-to-pack)
    (pack-jogl-natives pack-path local-repo dependencies platforms-to-pack)
    (pack-lua-language-server pack-path lua-language-server-version platforms-to-pack)))
