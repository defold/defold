(ns leiningen.init
  (:require
   [clojure.java.io :as io]
   [clojure.java.shell :as sh]
   [clojure.string :as string]
   [leiningen.core.main :as main])
  (:import [clojure.lang ExceptionInfo]
           [java.io File]
           [java.nio.file FileSystems]
           [java.util.zip ZipEntry ZipOutputStream]))

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
    (catch ExceptionInfo _
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

(defn- copy-single-dep!
  [^File from ^File to]
  (if-not (.exists from)
    (println (str "Cannot copy " (.getPath from) ", it does not exist."))
    (do
      (.mkdirs (.getParentFile to))
      (println (.getPath from) "=>" (.getPath to))
      (io/copy from to))))

(defn- glob-filter
  [f-seq ^String glob]
  (let [path-matcher (.getPathMatcher (FileSystems/getDefault) (str "glob:" glob))]
    (filter #(and (.isFile ^File %) (.matches path-matcher (.toPath ^File %))) f-seq)))

(defn- copy-bundle-deps-from-path!
  [^File source ^File dest dep-globs]
  (let [path-len (count (.getPath source))
        f-seq (file-seq source)
        files (into [] (mapcat #(glob-filter f-seq %)) dep-globs)]
    (doseq [from files]
      (let [to (io/file (str (.getPath dest) (subs (.getPath from) path-len)))]
        (copy-single-dep! from to)))))

(defn- zip-android-resources!
  "Used to make a zip archive of all the stuff in
  DYNAMO_HOME/ext/share/java/res (from) and put it in the editor resources
  folder (to) so that we can easily access and extract it when bundling for
  Android. Sadly we have to extract the files because the Android command line
  tools require that the input be files."
  [from to]
  (with-open [zip (ZipOutputStream. (io/output-stream to))]
    (let [prefix-len (inc (count (.getCanonicalPath from)))]
      (doseq [^File f (glob-filter (file-seq from) "**")]
        (let [entry-name (subs (.getCanonicalPath f) prefix-len)
              entry (ZipEntry. entry-name)]
          (.putNextEntry zip entry)
          (io/copy (io/input-stream f) zip)
          (.closeEntry zip))))
    (.finish zip)))

;; TODO(#4201): This needs to be cleaned up once the editor does all the
;; bundling. There is no sense in picking up tools and configuration from a
;; bunch of seemingly random locations when we remove Bob. We should just put
;; the stuff in the editor resources folder to begin with.
(defn- copy-bundle-deps!
  []
  (let [dynamo-home (io/file (string/join File/separator [".." "tmp" "dynamo_home"]))
        dynamo-lib (io/file dynamo-home "lib")
        bob-libexec (io/file ".." "com.dynamo.cr" "com.dynamo.cr.bob" "libexec")
        dynamo-ext-bin (io/file dynamo-home "ext" "bin")
        dynamo-ext-share-java-res (io/file dynamo-home "ext" "share" "java" "res")
        resources (io/file "resources")
        resources-lib (io/file resources "lib")
        resources-libexec (io/file resources "libexec")
        resources-android-res (io/file resources "android-res.zip")]
    (copy-bundle-deps-from-path! dynamo-lib
                                 resources-lib
                                 ["**/*libc++*"])
    (zip-android-resources! dynamo-ext-share-java-res resources-android-res)
    (doseq [[from to]
            [["bin/armv7-android/libdmengine.so" "libexec/armv7-android/libdmengine.so"]
             ["bin/armv7-android/libdmengine_release.so", "libexec/armv7-android/libdmengine_release.so"]
             ["bin/arm64-android/libdmengine_release.so", "libexec/arm64-android/libdmengine_release.so"]
             ["bin/arm64-android/libdmengine.so", "libexec/arm64-android/libdmengine.so"]
             ["ext/share/java/android.jar", "lib/android.jar"]
             ["share/java/classes.dex", "lib/classes.dex"]]]
      (copy-single-dep! (io/file dynamo-home from) (io/file resources to)))))

(def init-tasks
  [["clean"]
   ["local-jars"]
   ["builtins"]
   ["ref-doc"]
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
      (main/resolve-and-apply project task+args)))
  (println "Copying Android bundling dependencies to resources folder.")
  (copy-bundle-deps!))
