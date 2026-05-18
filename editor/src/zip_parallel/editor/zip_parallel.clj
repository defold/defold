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

(ns editor.zip-parallel
  "Create and filter jar files with parallel deflate."
  (:gen-class)
  (:require [clojure.edn :as edn]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.xml :as xml]
            [clojure.zip :as zip])
  (:import [java.io ByteArrayInputStream OutputStream PrintWriter]
           [java.util LinkedHashSet]
           [java.util.concurrent Executors ExecutorService TimeUnit]
           [java.util.regex Pattern]
           [org.apache.commons.compress.archivers.zip ParallelScatterZipCreator Zip64Mode ZipArchiveEntry ZipArchiveOutputStream ZipFile]
           [org.apache.commons.compress.parallel InputStreamSupplier]
           [org.apache.commons.io.output CloseShieldOutputStream]))

(set! *warn-on-reflection* true)

(defn- escape-xml
  [s]
  (string/escape s {\& "&amp;"
                    \< "&lt;"
                    \> "&gt;"
                    \" "&quot;"
                    \' "&apos;"}))

(defn- excluded?
  [exclusion-patterns entry-name]
  (boolean (some #(re-find % entry-name) exclusion-patterns)))

(defn- merge-entry-kind
  [entry-name]
  (cond
    (= "META-INF/plexus/components.xml" entry-name) :components
    (= "data_readers.clj" entry-name) :clj-map
    (re-find #"META-INF/services/.*" entry-name) :service
    :else nil))

(defn- tree-edit
  [zipper editor]
  (loop [loc zipper]
    (if (zip/end? loc)
      (zip/root loc)
      (if (= :description (:tag (zip/node loc)))
        (recur (zip/next (zip/edit loc editor)))
        (recur (zip/next loc))))))

(defn- html-escape-editor
  [node]
  (let [content (get (:content node) 0)]
    (if-not (nil? content)
      (assoc-in node [:content 0] (escape-xml content))
      node)))

(defn- components-read [ins]
  (let [zipper (->> ins xml/parse zip/xml-zip)]
    (some #(when (= (:tag %) :components)
             (:content %))
          (-> (tree-edit zipper html-escape-editor)
              zip/xml-zip
              zip/children))))

(defn- components-write [^OutputStream out components]
  (binding [*out* (PrintWriter. out)]
    (xml/emit {:tag :component-set
               :content [{:tag :components
                          :content components}]})
    (.flush *out*)))

(defn- read-merge-entry
  [kind ^ZipFile zip-file ^ZipArchiveEntry entry prev]
  (with-open [ins (.getInputStream zip-file entry)]
    (case kind
      :components
      (let [new (components-read ins)]
        (if-not prev
          new
          (into new prev)))

      :clj-map
      (let [new (read-string (slurp ins))]
        (if-not prev
          new
          (merge new prev)))

      :service
      (let [new (slurp ins)]
        (if-not prev
          new
          (str new "\n" prev))))))

(defn- write-merge-entry!
  [^ZipArchiveOutputStream out ^String entry-name result]
  (let [kind (merge-entry-kind entry-name)]
    (.putArchiveEntry out (ZipArchiveEntry. entry-name))
    (case kind
      :components
      (components-write (CloseShieldOutputStream. out) result)

      :clj-map
      (io/copy (ByteArrayInputStream. (.getBytes (pr-str result) "UTF-8")) out)

      :service
      (io/copy (ByteArrayInputStream. (.getBytes ^String result "UTF-8")) out))
    (.closeArchiveEntry out)))

(defn- zip-entry
  ^ZipArchiveEntry [^ZipArchiveEntry source-entry]
  (let [entry (ZipArchiveEntry. (.getName source-entry))
        size (.getSize source-entry)
        extra (.getExtra source-entry)]
    (.setTime entry (.getTime source-entry))
    (.setMethod entry ZipArchiveOutputStream/DEFLATED)
    (when (not= -1 size)
      (.setSize entry size))
    (.setExternalAttributes entry (.getExternalAttributes source-entry))
    (.setInternalAttributes entry (.getInternalAttributes source-entry))
    (.setComment entry (.getComment source-entry))
    (when (some? extra)
      (.setExtra entry extra))
    entry))

(defn- input-stream-supplier
  ^InputStreamSupplier [^ZipFile zip-file ^ZipArchiveEntry source-entry]
  (reify InputStreamSupplier
    (get [_]
      (.getInputStream zip-file source-entry))))

(defn- add-archive-entry!
  [^ParallelScatterZipCreator creator ^ZipFile zip-file ^ZipArchiveEntry source-entry]
  (.addArchiveEntry creator (zip-entry source-entry) (input-stream-supplier zip-file source-entry)))

(defn- add-jar-entries!
  [^ParallelScatterZipCreator creator ^LinkedHashSet seen-entry-names merged-entries exclusion-patterns jar-file]
  (let [zip-file (ZipFile. (io/file jar-file))]
    [zip-file
     (reduce
       (fn [merged-entries ^ZipArchiveEntry entry]
         (let [entry-name (.getName entry)
               merge-kind (merge-entry-kind entry-name)]
           (cond
             (excluded? exclusion-patterns entry-name)
             (do
               (.add seen-entry-names entry-name)
               merged-entries)

             merge-kind
             (update merged-entries entry-name #(read-merge-entry merge-kind zip-file entry %))

             (.add seen-entry-names entry-name)
             (do
               (add-archive-entry! creator zip-file entry)
               merged-entries)

             :else
             merged-entries)))
       merged-entries
       (enumeration-seq (.getEntries zip-file)))]))

(defn- thread-count []
  (max 2 (.availableProcessors (Runtime/getRuntime))))

(defn- create-output-stream
  ^ZipArchiveOutputStream [filename]
  (doto (ZipArchiveOutputStream. (io/file filename))
    (.setUseZip64 Zip64Mode/AsNeeded)))

(defn create-standalone-jar!
  [standalone-filename exclusions-file jars]
  (let [exclusion-patterns (mapv #(Pattern/compile %) (edn/read-string (slurp exclusions-file)))
        thread-count (thread-count)
        ^ExecutorService executor (Executors/newFixedThreadPool thread-count)
        creator (ParallelScatterZipCreator. executor)
        seen-entry-names (LinkedHashSet.)
        open-zip-files (atom [])]
    (try
      (io/make-parents standalone-filename)
      (let [merged-entries (reduce
                             (fn [merged-entries jar-file]
                               (let [[zip-file merged-entries] (add-jar-entries! creator seen-entry-names merged-entries exclusion-patterns jar-file)]
                                 (swap! open-zip-files conj zip-file)
                                 merged-entries))
                             (array-map)
                             jars)]
        (with-open [out (create-output-stream standalone-filename)]
          (.writeTo creator out)
          (doseq [[entry-name result] merged-entries]
            (write-merge-entry! out entry-name result))))
      (println (format "Created standalone jar entries with ParallelScatterZipCreator (%d threads)" thread-count))
      (finally
        (doseq [^ZipFile zip-file @open-zip-files]
          (.close zip-file))
        (.shutdown executor)
        (.awaitTermination executor 1 TimeUnit/MINUTES)))))

(defn filter-jar!
  [input-jar output-jar removals-file]
  (let [removed-entry-names (with-open [reader (io/reader removals-file)]
                              (into #{} (line-seq reader)))
        thread-count (thread-count)
        ^ExecutorService executor (Executors/newFixedThreadPool thread-count)
        creator (ParallelScatterZipCreator. executor)]
    (try
      (io/make-parents output-jar)
      (with-open [zip-file (ZipFile. (io/file input-jar))]
        (doseq [^ZipArchiveEntry entry (enumeration-seq (.getEntries zip-file))]
          (when-not (contains? removed-entry-names (.getName entry))
            (add-archive-entry! creator zip-file entry)))
        (with-open [out (create-output-stream output-jar)]
          (.writeTo creator out)))
      (println (format "Filtered jar entries with ParallelScatterZipCreator (%d threads)" thread-count))
      (finally
        (.shutdown executor)
        (.awaitTermination executor 1 TimeUnit/MINUTES)))))

(defn- usage []
  (println "Usage:")
  (println "  create-standalone-jar <output-jar> <exclusions-edn> <jar>...")
  (println "  filter-jar <input-jar> <output-jar> <removals-file>"))

(defn -main
  [& [command & args]]
  (if (nil? command)
    (do
      (usage)
      (System/exit 1))
    (case command
      ("create-standalone-jar" "--create-standalone-jar")
      (let [[standalone-filename exclusions-file & jars] args]
        (when (or (nil? standalone-filename) (nil? exclusions-file) (empty? jars))
          (usage)
          (System/exit 1))
        (create-standalone-jar! standalone-filename exclusions-file jars))

      ("filter-jar" "--filter-jar")
      (let [[input-jar output-jar removals-file & extra] args]
        (when (or (nil? input-jar) (nil? output-jar) (nil? removals-file) (seq extra))
          (usage)
          (System/exit 1))
        (filter-jar! input-jar output-jar removals-file))

      (do
        (usage)
        (throw (IllegalArgumentException. (str "Unknown command: " command)))))))
