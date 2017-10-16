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
  {"x86_64-darwin" {"bin" ["dmengine" "dmengine_release"]
                    "lib" ["libparticle_shared.dylib" "libtexc_shared.dylib"]}
   "x86-win32"     {"bin" ["dmengine.exe" "dmengine_release.exe"]
                    "lib" ["particle_shared.dll" "texc_shared.dll"]}
   "x86_64-win32"  {"bin" ["dmengine.exe" "dmengine_release.exe"]
                    "lib" ["particle_shared.dll" "texc_shared.dll"]}
   "x86-linux"     {"bin" ["dmengine" "dmengine_release"]
                    "lib" ["libparticle_shared.so" "libtexc_shared.so"]}
   "x86_64-linux"  {"bin" ["dmengine" "dmengine_release"]
                    "lib" ["libparticle_shared.so" "libtexc_shared.so"]}
   "armv7-darwin"  {"bin" ["dmengine" "dmengine_release"]
                    "lib" []}
   "arm64-darwin"  {"bin" ["dmengine" "dmengine_release"]
                    "lib" []}})

(defn- platform->engine-src-dirname [platform]
  (assert (contains? engine-artifacts platform))
  (case platform
    "x86-darwin" "darwin"
    "x86-linux" "linux"
    "x86-win32" "win32"
    platform))

(def artifacts
  {"${DYNAMO-HOME}/ext/bin/win32/luajit.exe"          "x86-win32/bin/luajit.exe"
   "${DYNAMO-HOME}/ext/lib/win32/OpenAL32.dll"        "x86-win32/bin/OpenAL32.dll"
   "${DYNAMO-HOME}/ext/lib/win32/wrap_oal.dll"        "x86-win32/bin/wrap_oal.dll"
   "${DYNAMO-HOME}/ext/lib/win32/PVRTexLib.dll"       "x86-win32/lib/PVRTexLib.dll"
   "${DYNAMO-HOME}/ext/lib/win32/msvcr120.dll"        "x86-win32/lib/msvcr120.dll"

   "${DYNAMO-HOME}/ext/bin/x86_64-win32/luajit.exe"    "x86_64-win32/bin/luajit.exe"
   "${DYNAMO-HOME}/ext/lib/x86_64-win32/OpenAL32.dll"  "x86_64-win32/bin/OpenAL32.dll"
   "${DYNAMO-HOME}/ext/lib/x86_64-win32/wrap_oal.dll"  "x86_64-win32/bin/wrap_oal.dll"
   "${DYNAMO-HOME}/ext/lib/x86_64-win32/PVRTexLib.dll" "x86_64-win32/lib/PVRTexLib.dll"
   "${DYNAMO-HOME}/ext/lib/x86_64-win32/msvcr120.dll"  "x86_64-win32/lib/msvcr120.dll"

   "${DYNAMO-HOME}/ext/bin/linux/luajit"                      "x86-linux/bin/luajit"
   "${DYNAMO-HOME}/ext/lib/linux/libPVRTexLib.so"             "x86-linux/lib/libPVRTexLib.so"

   "${DYNAMO-HOME}/ext/bin/x86_64-linux/luajit"               "x86_64-linux/bin/luajit"
   "${DYNAMO-HOME}/ext/lib/x86_64-linux/libPVRTexLib.so"      "x86_64-linux/lib/libPVRTexLib.so"

   "${DYNAMO-HOME}/ext/lib/x86_64-darwin/libPVRTexLib.dylib"  "x86_64-darwin/lib/libPVRTexLib.dylib"
   "${DYNAMO-HOME}/ext/bin/x86_64-darwin/luajit"              "x86_64-darwin/bin/luajit"

   "${DYNAMO-HOME}/ext/share/luajit"                  "shared/luajit"

   "bundle-resources/x86_64-darwin/lipo"              "x86_64-darwin/bin/lipo"
   "bundle-resources/x86_64-darwin/codesign_allocate" "x86_64-darwin/bin/codesign_allocate"})

(defn engine-artifact-files
  [git-sha]
  (into {} (for [[platform dirs] engine-artifacts
                 [dir files] dirs
                 file files]
             (let [engine-src-dirname (platform->engine-src-dirname platform)
                   src (if (some? git-sha)
                         (http-cache/download (format "https://d.defold.com/archive/%s/engine/%s/%s" git-sha engine-src-dirname file))
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
   "linux-i586"       "x86-linux"
   "macosx-universal" "x86_64-darwin"
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
  (and (#{'org.jogamp.gluegen/gluegen-rt
          'org.jogamp.jogl/jogl-all} artifact)
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
  [pack-path git-sha]
  (let [files (merge (engine-artifact-files git-sha)
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
  "Pack all files that need to be unpacked at runtime into `pack-path`.

  Arguments:

    - git-sha [optional]: If supplied, download and use archived engine artifacts
                          for the given sha. Otherwise use local engine artifacts
                          when they exist."
  [{:keys [dependencies packing] :as project} & [git-sha]]
  (let [sha (or git-sha (:engine project))
        {:keys [pack-path]} packing]
    (FileUtils/deleteQuietly (io/file pack-path))
    (copy-artifacts pack-path sha)
    (pack-jogl-natives pack-path dependencies)))
