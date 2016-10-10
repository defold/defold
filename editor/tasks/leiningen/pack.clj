(ns leiningen.pack
  (:require
   [clojure.java.io :as io]
   [clojure.java.shell :as shell]
   [clojure.string :as str])
  (:import
   (java.io File)
   (org.apache.commons.io FileUtils)))

(defn sha1
  [s]
  (.toString (BigInteger. 1 (-> (java.security.MessageDigest/getInstance "SHA1")
                                (.digest (.getBytes s)))) 16))

(defn download
  [url]
  (let [cache-file (io/file ".cache" (sha1 (str url)))]
    (if (.exists cache-file)
      (do
        (println (format "using cached file '%s' for '%s'" cache-file url))
        cache-file)
      (do
        (println (format "downloading file '%s' to '%s'" url cache-file))
        (let [tmp-file (File/createTempFile "defold-download-cache" nil)]
          (FileUtils/copyURLToFile url tmp-file (* 10 1000) (* 30 1000))
          (FileUtils/moveFile tmp-file cache-file)
          cache-file)))))


(defn dynamo-home [] (get (System/getenv) "DYNAMO_HOME"))

;; these can be sourced either from a local build of engine, or downloaded from an archived build on s3
(def engine-artifacts
  {"x86_64-darwin" {"bin" ["dmengine" "dmengine_release"]
                    "lib" ["libparticle_shared.dylib" "libtexc_shared.dylib"]}
   "x86-win32"     {"bin" ["dmengine.exe" "dmengine_release.exe"]
                    "lib" ["particle_shared.dll" "texc_shared.dll"]}
   "x86-linux"     {"bin" ["dmengine" "dmengine_release"]
                    "lib" ["libparticle_shared.so" "libtexc_shared.so"]}
   "x86_64-linux"  {"bin" ["dmengine" "dmengine_release"]
                    "lib" ["libparticle_shared.so" "libtexc_shared.so"]}})

(def engine-platform
  {"x86_64-linux"  "x86_64-linux"
   "x86-linux"     "linux"
   "x86_64-darwin" "x86_64-darwin"
   "x86-win32"     "win32"
   "x86_64-win32"  "x86_64-win32"})

;; these are only sourced from local DYNAMO_HOME
(def dynamo-home-artifacts
  {"ext/bin/win32/luajit.exe"     "x86-win32/bin/luajit.exe"
   "ext/lib/win32/OpenAL32.dll"   "x86-win32/bin/OpenAL32.dll"
   "ext/bin/x86-linux/luajit"     "x86-linux/bin/luajit"
   "ext/bin/x86_64-darwin/luajit" "x86_64-darwin/bin/luajit"
   "ext/bin/x86_64-linux/luajit"  "x86_64-linux/bin/luajit"
   "ext/share/luajit"             "shared/luajit"})

(defn engine-archive-url
  [sha platform file]
  (io/as-url (format "http://d.defold.com/archive/%s/engine/%s/%s" sha
                     (engine-platform platform) file)))

(defn engine-artifact-files
  [git-sha]
  (into {} (for [[platform dirs] engine-artifacts
                 [dir files] dirs
                 file files]
             (let [src (if git-sha
                         (download (engine-archive-url git-sha platform file))
                         (io/file (dynamo-home) dir platform file))]
               [src (io/file platform dir file)]))))

(defn dynamo-home-artifact-files
  []
  (into {} (for [[src dest] dynamo-home-artifacts]
             [(io/file (dynamo-home) src) (io/file dest)])))

(defn jogl-native-files
  []
  {(io/file "jogl-natives/") ""})

(defn copy-artifacts
  [pack-path git-sha]
  (let [files (merge (engine-artifact-files git-sha)
                     (dynamo-home-artifact-files)
                     (jogl-native-files))]
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
  [{:keys [packing] :as project} & [git-sha]]
  (let [{:keys [pack-path]} packing]
    (FileUtils/deleteQuietly (io/file pack-path))
    (copy-artifacts pack-path git-sha)))
