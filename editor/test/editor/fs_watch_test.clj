(ns editor.fs-watch-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [editor.fs-watch :as fs-watch])
  (:import [java.io File]
           [java.nio.file Files attribute.FileAttribute]
           [org.apache.commons.io FileUtils]))

(def filter-fn (fn [^File f]
                 (let [name (.getName f)]
                   (not (or (= name "build") (= (subs name 0 1) "."))))))

(defn- setup []
  (let [root (.toFile (Files/createTempDirectory "foo" (into-array FileAttribute [])))
        watcher (fs-watch/make-watcher root filter-fn)]
    [watcher root]))

(defn- add-file [root file content]
  (let [f (File. root file)
        parent (.getParentFile f)]
    (when (not (.exists parent))
      (.mkdirs parent))
    (spit (File. root file) content)))

(defn- append-file [root file content]
  ; Sleep since Java mtime is in second-resolution
  (Thread/sleep 1100)
  (let [f (File. root file)]
    (spit f content :append true)))

(defn- remove-file [root file]
  (.delete (File. root file)))

(defn- add-dir [root name]
  (let [f (File. root name)]
    (.mkdirs f)))

(defn- remove-dir [root name]
  (let [f (File. root name)]
    (FileUtils/deleteDirectory f)))

(defn- fileify [watcher file]
  (when file
    (File. (:root watcher) file)))

(defn- watch [watcher & expected]
  (let [watcher (fs-watch/watch watcher)
        changes (:changes watcher)
        root (:root watcher)
        expected (merge (into {} (map #(do [% []]) (keys changes)))
                        (into {} (map (fn [[type name]] [type [(File. root name)]]) (partition 2 expected))))]
    (is (= expected changes))
    watcher))

(deftest basic
  (let [[watcher root] (setup)
        watcher (watch watcher)
        watcher (do
                  (add-file root "test.txt" "test")
                  (watch watcher :added "test.txt"))
        watcher (do
                  (append-file root "test.txt" "test2")
                  (watch watcher :changed "test.txt"))
        watcher (do
                  (remove-file root "test.txt")
                  (watch watcher :removed "test.txt" :changed ""))
        watcher (do
                  (add-file root "build/test.txt" "test")
                  (watch watcher))
        watcher (do
                  (add-file root ".test.txt" "test")
                  (watch watcher))]))

(deftest directory
  (let [[watcher root] (setup)
        watcher (watch watcher)
        watcher (do
                  (add-dir root "my-dir")
                  (watch watcher :added "my-dir"))
        watcher (do
                  (remove-dir root "my-dir")
                  (watch watcher :removed "my-dir"))]))
