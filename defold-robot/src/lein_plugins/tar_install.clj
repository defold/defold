(ns lein-plugins.tar-install
  (:require [clojure.java.io :as io]
            [clojure.string :as string])
  (:import [java.io ByteArrayOutputStream File FileOutputStream]
           [java.util.zip GZIPOutputStream]
           [org.apache.tools.tar TarEntry TarOutputStream]))

(defn- out-stream ^TarOutputStream [tar-file]
  (let [os (-> tar-file
             (FileOutputStream.)
             (GZIPOutputStream.)
             (TarOutputStream.))]
    (.setLongFileMode os TarOutputStream/LONGFILE_GNU)
    os))

(defn- add-file [os path name]
  (let [f (File. path)
        dir-names (-> name
                    (string/split #"/")
                    butlast
                    vec)]
    (dotimes [i (count dir-names)]
      (let [dir (->> (subvec dir-names 0 (inc i))
                  (string/join "/")
                  (format "%s/"))]
        (.putNextEntry os (TarEntry. dir))
        (.closeEntry os)))
    (.putNextEntry os (TarEntry. f name))
    (io/copy f os)
    (.closeEntry os)))

(defn tar-install [name version target-path]
  (let [tar-file (File. (format "../packages/%s-%s-common.tar.gz" name version))]
    (with-open [os (out-stream tar-file)]
      (add-file os (format "%s/%s-%s-standalone.jar" target-path name version) (format "share/java/%s.jar" name)))
    (println "Wrote" (.getCanonicalPath tar-file))))

(defn -main [name version target-path]
  (tar-install name version target-path))