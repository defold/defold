(ns leiningen.builtins
  (:require [clojure.java.io :as io])
  (:import  [java.io File]
            [java.util.zip ZipOutputStream ZipEntry]))

(defn- relative-path [^File f1 ^File f2]
  (.toString (.relativize (.toPath f1) (.toPath f2))))

(defn- read-file [^File file]
  (let [buffer (byte-array (.length file))]
    (with-open [is (io/input-stream file)]
      (.read is buffer)
      buffer)))

(defn zip-tree [^File root path zip-path]
  (with-open [zip (ZipOutputStream. (io/output-stream zip-path))]
    (doseq [f (filter #(.isFile %) (file-seq (io/file path)))]
      (let [entry (ZipEntry. (relative-path root f))]
        (.putNextEntry zip entry)
        (.write zip (read-file f) 0 (.length f))
        (.closeEntry zip)))))

(defn builtins [project & args]
  (let [dynamo-home (get (System/getenv) "DYNAMO_HOME")]
    (zip-tree (io/file (str dynamo-home "/content")) (str dynamo-home "/content/builtins") "generated-resources/builtins.zip")))
