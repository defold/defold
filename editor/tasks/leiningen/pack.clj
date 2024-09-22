;; Copyright 2020-2024 The Defold Foundation
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
           [java.nio.file CopyOption Files FileSystems FileVisitor FileVisitResult LinkOption Path Paths]
           [java.nio.file.attribute FileAttribute]
           [java.util.zip ZipEntry ZipFile]
           [org.apache.commons.io FileUtils]))

(set! *warn-on-reflection* true)

(defn dynamo-home [] (get (System/getenv) "DYNAMO_HOME"))

;; this is universal packing for all the platforms, but we clenup it in bundle.py -> remove_platform_files_from_archive()

;; these can be sourced either from a local build of engine, or downloaded from an archived build on s3
(def engine-artifacts
  {"x86_64-macos" {"bin" ["dmengine"]
                    "lib" ["libparticle_shared.dylib"]}
   "arm64-macos" {"bin" ["dmengine"]
                  "lib" ["libparticle_shared.dylib"]}
   "x86_64-win32"  {"bin" ["dmengine.exe" "dmengine.pdb"]
                    "lib" ["particle_shared.dll"]}
   "x86_64-linux"  {"bin" ["dmengine"]
                    "lib" ["libparticle_shared.so"]}})

(defn- platform->engine-src-dirname [platform]
  (assert (contains? engine-artifacts platform))
  (case platform
    "x86-win32" "win32"
    platform))

(def artifacts
  {"${DYNAMO-HOME}/ext/lib/win32/OpenAL32.dll"        "x86-win32/bin/OpenAL32.dll"
   "${DYNAMO-HOME}/ext/lib/win32/wrap_oal.dll"        "x86-win32/bin/wrap_oal.dll"

   "${DYNAMO-HOME}/ext/bin/x86_64-win32/luajit-64.exe" "x86_64-win32/bin/luajit-64.exe"
   "${DYNAMO-HOME}/ext/lib/x86_64-win32/OpenAL32.dll"  "x86_64-win32/bin/OpenAL32.dll"
   "${DYNAMO-HOME}/ext/lib/x86_64-win32/wrap_oal.dll"  "x86_64-win32/bin/wrap_oal.dll"

   "${DYNAMO-HOME}/ext/bin/x86_64-linux/luajit-64"            "x86_64-linux/bin/luajit-64"

   "${DYNAMO-HOME}/ext/bin/x86_64-macos/luajit-64"           "x86_64-macos/bin/luajit-64"

   "${DYNAMO-HOME}/ext/bin/arm64-macos/luajit-64"            "arm64-macos/bin/luajit-64"

   "$DYNAMO_HOME/ext/bin/x86_64-macos/glslang"                  "x86_64-macos/glslang"
   "$DYNAMO_HOME/ext/bin/arm64-macos/glslang"                   "arm64-macos/glslang"
   "$DYNAMO_HOME/ext/bin/x86_64-linux/glslang"                  "x86_64-linux/glslang"
   "$DYNAMO_HOME/ext/bin/x86_64-win32/glslang.exe"              "x86_64-win32/glslang.exe"

   "$DYNAMO_HOME/ext/bin/x86_64-linux/tint"            "x86_64-linux/tint"
   "$DYNAMO_HOME/ext/bin/x86_64-macos/tint"            "x86_64-macos/tint"
   "$DYNAMO_HOME/ext/bin/arm64-linux/tint"            "arm64-macos/tint"

   "$DYNAMO_HOME/ext/bin/x86_64-macos/spirv-cross"            "x86_64-macos/spirv-cross"
   "$DYNAMO_HOME/ext/bin/arm64-macos/spirv-cross"             "arm64-macos/spirv-cross"
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
  ^File [[artifact version & {:keys [classifier]} :as dependency]]
  (io/file (str (System/getProperty "user.home")
                "/.m2/repository/"
                (str/replace (namespace artifact) "." "/")
                "/" (name artifact)
                "/" version
                "/" (name artifact) "-" version
                (some->> classifier name (str "-")) ".jar")))

(defn jogl-native-dep?
  [[artifact version & {:keys [classifier]}]]
  (and (#{'org.jogamp.gluegen/gluegen-rt
          'org.jogamp.jogl/jogl-all} artifact)
       classifier))

(defn extract-jogl-native-dep
  [[artifact version & {:keys [classifier]} :as dependency] pack-path]
  (let [java-platform (str/replace-first classifier "natives-" "")
        natives-path (str "natives/" java-platform)]
    (with-open [zip-file (ZipFile. (jar-file dependency))]
      (doseq [^ZipEntry entry (enumeration-seq (.entries zip-file))]
        (when (.startsWith (.getName entry) natives-path)
          (let [libname (.getName (io/file (.getName entry)))]
            (doseq [target-platform (jogl-classifier->platforms java-platform)]
              (let [dest (io/file pack-path target-platform "lib" libname)]
                (println (format "extracting '%s'/'%s' to '%s'" (.getName zip-file) (.getName entry) dest))
                (io/make-parents dest)
                (io/copy (.getInputStream zip-file entry) dest)))))))))

(defn pack-jogl-natives
  [pack-path dependencies]
  (doseq [jogl-native-dep (filter jogl-native-dep? dependencies)]
    (extract-jogl-native-dep jogl-native-dep pack-path)))

(defn pack-lua-language-server [pack-path lua-language-server-version]
  (let [release-path (-> (format "https://github.com/defold/lua-language-server/releases/download/%s/release.zip"
                                 lua-language-server-version)
                         http-cache/download
                         .toPath)
        file-attributes (into-array FileAttribute [])
        ^"[Ljava.nio.file.CopyOption;" copy-options (into-array CopyOption [])]
   (with-open [fs (FileSystems/newFileSystem release-path)]
     (doseq [platform (keys engine-artifacts)
             :let [zip-file-name (str platform ".zip")
                   src-zip-path (.getPath fs "lsp-lua-language-server" (into-array String ["plugins" zip-file-name]))
                   dst-root-path (Paths/get pack-path (into-array String [platform "bin" "lsp" "lua"]))]]
       ;; Copy config.json to the pack path
       (let [source-path (.getPath fs "lsp-lua-language-server" (into-array String ["plugins" "share" "config.json"]))
             target-path (.resolve dst-root-path "config.json")]
         (Files/createDirectories (.getParent target-path) file-attributes)
         (Files/copy source-path target-path copy-options))
       ;; Copy contents of bin zips to the pack path
       (with-open [fs (FileSystems/newFileSystem src-zip-path)]
         (doseq [^Path root-path (.getRootDirectories fs)
                 :let [entry-path->dst-path (fn [^Path p]
                                              (let [name-count (.getNameCount p)]
                                                (when (< 2 name-count)
                                                  (.resolve dst-root-path
                                                            (-> root-path
                                                                (.relativize p)
                                                                ;; remove leading "bin/${platform}"
                                                                (.subpath 2 name-count)
                                                                str)))))]]
           (Files/walkFileTree
             root-path
             (reify FileVisitor
               (preVisitDirectory [_ path _]
                 (when-let [^Path target-path (entry-path->dst-path path)]
                   (when-not (Files/exists target-path (into-array LinkOption []))
                     (Files/createDirectories target-path file-attributes)))
                 FileVisitResult/CONTINUE)
               (visitFile [_ path _]
                 (when-let [^Path target-path (entry-path->dst-path path)]
                   (Files/deleteIfExists target-path)
                   (Files/copy ^Path path target-path copy-options))
                 FileVisitResult/CONTINUE)
               (postVisitDirectory [_ _ _] FileVisitResult/CONTINUE)))))))))

(defn copy-artifacts
  [pack-path archive-domain git-sha]
  (let [files (merge (engine-artifact-files archive-domain git-sha)
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
  [{:keys [dependencies packing] :as project} & [git-sha]]
  (let [sha (or git-sha (:engine project))
        archive-domain (get project :archive-domain)
        {:keys [pack-path lua-language-server-version]} packing]
    (FileUtils/deleteQuietly (io/file pack-path))
    (copy-artifacts pack-path archive-domain sha)
    (pack-jogl-natives pack-path dependencies)
    (pack-lua-language-server pack-path lua-language-server-version)))
