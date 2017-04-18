(ns leiningen.util.download
  (:require
   [clojure.java.io :as io]
   [clojure.java.shell :as shell]
   [clojure.string :as str]
   [cemerick.pomegranate.aether :as aether]
   [leiningen.core.main :as main])
  (:import
   (java.io File)
   (java.util.zip ZipFile ZipEntry)
   (org.apache.commons.io FileUtils)))

(defn sha1
  [s]
  (.toString (BigInteger. 1 (-> (java.security.MessageDigest/getInstance "SHA1")
                                (.digest (.getBytes s)))) 16))

(defn download
  [url]
  (let [cache-file (io/file ".cache" (sha1 url))]
    (if (.exists cache-file)
      (do
        (println (format "using cached file '%s' for '%s'" cache-file url))
        cache-file)
      (try
        (println (format "downloading file '%s' to '%s'" url cache-file))
        (let [tmp-file (File/createTempFile "defold-download-cache" nil)]
          (FileUtils/copyURLToFile (io/as-url url) tmp-file (* 10 1000) (* 30 1000))
          (FileUtils/moveFile tmp-file cache-file)
          cache-file)
        (catch java.io.FileNotFoundException e
          ;; Fallback to local bob
          (println (format "- %s could not be found" url))
          nil)))))
