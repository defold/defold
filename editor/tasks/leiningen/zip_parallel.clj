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

(ns leiningen.zip-parallel
  "Create and filter jar files with parallel deflate."
  (:require [clojure.edn :as edn]
            [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.string :as string]
            [clojure.xml :as xml]
            [clojure.zip :as zip]
            [leiningen.core.classpath :as classpath]
            [leiningen.core.main :as main]
            [leiningen.core.project :as project]
            [leiningen.jar :as jar]
            [leiningen.pom :as pom])
  (:import [clojure.lang Reflector]
           [java.io ByteArrayInputStream File OutputStream PrintWriter]
           [java.lang.reflect InvocationHandler Method Proxy]
           [java.util LinkedHashSet List]
           [java.util.concurrent Executors ExecutorService TimeUnit]
           [java.util.regex Pattern]))

(set! *warn-on-reflection* true)

(defn- commons-compress-jar
  [deps]
  (or (some #(when (re-find #"commons-compress-[^/\\]+\.jar$" (.getPath ^File %)) %) deps)
      (main/abort "Unable to find commons-compress jar for parallel zip task.")))

(defn- standalone-jar-classpath
  ^String [project deps]
  (let [tasks-path (io/file (:root project) "tasks")]
    (string/join File/pathSeparator
                 [(str (commons-compress-jar deps))
                  (str tasks-path)
                  (System/getProperty "java.class.path")])))

(defn- invoke-writer!
  [project deps args abort-message]
  (let [java-executable (io/file (System/getProperty "java.home") "bin" "java")
        command (into [(str java-executable)
                       "-Dfile.encoding=UTF-8"
                       "-cp"
                       (standalone-jar-classpath project deps)
                       "clojure.main"
                       "-m"
                       "leiningen.zip-parallel"]
                      (map str)
                      args)
        exit-code (-> (ProcessBuilder. ^List command)
                      (doto (.directory (io/file (:root project)))
                            (.inheritIO))
                      (.start)
                      (.waitFor))]
    (when-not (zero? exit-code)
      (main/abort abort-message "failed with exit code" exit-code))))

(def ^:private parallel-scatter-zip-creator-class-name
  "org.apache.commons.compress.archivers.zip.ParallelScatterZipCreator")

(def ^:private zip64-mode-class-name
  "org.apache.commons.compress.archivers.zip.Zip64Mode")

(def ^:private zip-archive-entry-class-name
  "org.apache.commons.compress.archivers.zip.ZipArchiveEntry")

(def ^:private zip-archive-output-stream-class-name
  "org.apache.commons.compress.archivers.zip.ZipArchiveOutputStream")

(def ^:private zip-file-class-name
  "org.apache.commons.compress.archivers.zip.ZipFile")

(def ^:private input-stream-supplier-class-name
  "org.apache.commons.compress.parallel.InputStreamSupplier")

(def ^:private close-shield-output-stream-class-name
  "org.apache.commons.io.output.CloseShieldOutputStream")

(defn- class-named
  ^Class [class-name]
  (Class/forName class-name))

(defn- construct
  [class-name & args]
  (Reflector/invokeConstructor (class-named class-name) (object-array args)))

(defn- invoke
  [target method-name & args]
  (Reflector/invokeInstanceMethod target method-name (object-array args)))

(defmacro ^:private with-closed
  [[binding init] & body]
  `(let [~binding ~init]
     (try
       ~@body
       (finally
         (when ~binding
           (invoke ~binding "close"))))))

(defn- static-field
  [class-name field-name]
  (-> (class-named class-name)
      (.getField field-name)
      (.get nil)))

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
  [kind zip-file entry prev]
  (with-closed [ins (invoke zip-file "getInputStream" entry)]
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
  [out ^String entry-name result]
  (let [kind (merge-entry-kind entry-name)]
    (invoke out "putArchiveEntry" (construct zip-archive-entry-class-name entry-name))
    (case kind
      :components
      (components-write (construct close-shield-output-stream-class-name out) result)

      :clj-map
      (io/copy (ByteArrayInputStream. (.getBytes (pr-str result) "UTF-8")) out)

      :service
      (io/copy (ByteArrayInputStream. (.getBytes ^String result "UTF-8")) out))
    (invoke out "closeArchiveEntry")))

(defn- zip-entry
  [source-entry]
  (let [entry (construct zip-archive-entry-class-name (invoke source-entry "getName"))
        size (invoke source-entry "getSize")
        extra (invoke source-entry "getExtra")]
    (invoke entry "setTime" (invoke source-entry "getTime"))
    (invoke entry "setMethod" (static-field zip-archive-output-stream-class-name "DEFLATED"))
    (when (not= -1 size)
      (invoke entry "setSize" size))
    (invoke entry "setExternalAttributes" (invoke source-entry "getExternalAttributes"))
    (invoke entry "setInternalAttributes" (invoke source-entry "getInternalAttributes"))
    (invoke entry "setComment" (invoke source-entry "getComment"))
    (when (some? extra)
      (invoke entry "setExtra" extra))
    entry))

(defn- input-stream-supplier
  [zip-file source-entry]
  (let [supplier-class (class-named input-stream-supplier-class-name)]
    (Proxy/newProxyInstance
      (.getClassLoader supplier-class)
      (into-array Class [supplier-class])
      (reify InvocationHandler
        (invoke [_ proxy method args]
          (case (.getName ^Method method)
            "get"
            (invoke zip-file "getInputStream" source-entry)

            "toString"
            "InputStreamSupplier"

            "hashCode"
            (System/identityHashCode proxy)

            "equals"
            (identical? proxy (aget ^objects args 0))

            (throw (UnsupportedOperationException.
                     (str "Unsupported InputStreamSupplier method: " (.getName ^Method method))))))))))

(defn- add-archive-entry!
  [creator zip-file source-entry]
  (invoke creator "addArchiveEntry" (zip-entry source-entry) (input-stream-supplier zip-file source-entry)))

(defn- add-jar-entries!
  [creator ^LinkedHashSet seen-entry-names merged-entries exclusion-patterns jar-file]
  (let [zip-file (construct zip-file-class-name (io/file jar-file))]
    [zip-file
     (reduce
       (fn [merged-entries entry]
         (let [entry-name (invoke entry "getName")
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
       (enumeration-seq (invoke zip-file "getEntries")))]))

(defn- thread-count []
  (max 2 (.availableProcessors (Runtime/getRuntime))))

(defn- create-output-stream
  [filename]
  (doto (construct zip-archive-output-stream-class-name (io/file filename))
    (invoke "setUseZip64" (static-field zip64-mode-class-name "AsNeeded"))))

(defn- create-standalone-jar!
  [standalone-filename exclusions-file jars]
  (let [exclusion-patterns (mapv #(Pattern/compile %) (edn/read-string (slurp exclusions-file)))
        thread-count (thread-count)
        ^ExecutorService executor (Executors/newFixedThreadPool thread-count)
        creator (construct parallel-scatter-zip-creator-class-name executor)
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
        (with-closed [out (create-output-stream standalone-filename)]
          (invoke creator "writeTo" out)
          (doseq [[entry-name result] merged-entries]
            (write-merge-entry! out entry-name result))))
      (println (format "Created standalone jar entries with ParallelScatterZipCreator (%d threads)" thread-count))
      (finally
        (doseq [zip-file @open-zip-files]
          (invoke zip-file "close"))
        (.shutdown executor)
        (.awaitTermination executor 1 TimeUnit/MINUTES)))))

(defn- filter-jar!
  [input-jar output-jar removals-file]
  (let [removed-entry-names (with-open [reader (io/reader removals-file)]
                              (into #{} (line-seq reader)))
        thread-count (thread-count)
        ^ExecutorService executor (Executors/newFixedThreadPool thread-count)
        creator (construct parallel-scatter-zip-creator-class-name executor)]
    (try
      (io/make-parents output-jar)
      (with-closed [zip-file (construct zip-file-class-name (io/file input-jar))]
        (doseq [entry (enumeration-seq (invoke zip-file "getEntries"))]
          (when-not (contains? removed-entry-names (invoke entry "getName"))
            (add-archive-entry! creator zip-file entry)))
        (with-closed [out (create-output-stream output-jar)]
          (invoke creator "writeTo" out)))
      (println (format "Filtered jar entries with ParallelScatterZipCreator (%d threads)" thread-count))
      (finally
        (.shutdown executor)
        (.awaitTermination executor 1 TimeUnit/MINUTES)))))

(defn- parallel-uberjar
  [project main]
  (let [scoped-profiles (set (project/pom-scope-profiles project :provided))
        default-profiles (set (project/expand-profile project :default))
        provided-profiles (into [] (remove (set/difference default-profiles scoped-profiles))
                               (-> project meta :included-profiles))
        project (project/merge-profiles (project/merge-profiles project [:uberjar]) provided-profiles)
        _ #_ (bail early if snapshot) (pom/check-for-snapshot-deps project)
        project (update-in project [:jar-inclusions]
                           concat (:uberjar-inclusions project))
        [_ jar-file] (try (first (jar/jar project main))
                          (catch Exception e
                            (when main/*debug*
                              (.printStackTrace e))
                            (main/abort "Parallel uberjar aborting because jar failed:"
                                        (.getMessage e))))
        standalone-filename (jar/get-jar-filename project :standalone)
        whitelisted (select-keys project project/whitelist-keys)
        dep-project (-> (project/unmerge-profiles project [:default])
                        (merge whitelisted))
        deps (into [] (filter #(.endsWith (.getName ^File %) ".jar"))
                   (classpath/resolve-managed-dependencies
                     :dependencies :managed-dependencies dep-project))
        jars (into [(io/file jar-file)] deps)
        exclusions-file (io/file (:target-path project) "parallel-uberjar-exclusions.edn")]
    (io/make-parents exclusions-file)
    (spit exclusions-file (pr-str (into [] (map str) (:uberjar-exclusions project))))
    (main/info "Creating standalone jar entries with ParallelScatterZipCreator...")
    (invoke-writer! project deps
                    (into ["--create-standalone-jar" standalone-filename (str exclusions-file)]
                          (map str)
                          jars)
                    "Parallel uberjar writer")
    (main/info "Created" standalone-filename)
    standalone-filename))

(defn- parallel-filter-jar
  [project input-jar output-jar removals-file]
  (let [deps (classpath/resolve-managed-dependencies :dependencies :managed-dependencies project)]
    (main/info "Filtering jar entries with ParallelScatterZipCreator...")
    (invoke-writer! project deps
                    ["--filter-jar" input-jar output-jar removals-file]
                    "Parallel jar filter")))

(defn zip-parallel
  "Create and filter jar files with parallel deflate.

  Subcommands:
    uberjar [main]                         Create the standalone editor jar.
    filter-jar <input> <output> <removals> Copy input to output without listed entries."
  ([project]
   (zip-parallel project "uberjar"))
  ([project command & args]
   (case command
     "uberjar"
     (let [[main & extra] args]
       (when (seq extra)
         (main/abort "zip-parallel uberjar accepts at most one main namespace argument."))
       (parallel-uberjar project main))

     "filter-jar"
     (let [[input-jar output-jar removals-file & extra] args]
       (when (or (nil? input-jar) (nil? output-jar) (nil? removals-file) (seq extra))
         (main/abort "Usage: lein zip-parallel filter-jar <input-jar> <output-jar> <removals-file>"))
       (parallel-filter-jar project input-jar output-jar removals-file))

     (main/abort "Unknown zip-parallel command:" command))))

(defn -main
  [command & args]
  (case command
    "--create-standalone-jar"
    (let [[standalone-filename exclusions-file & jars] args]
      (create-standalone-jar! standalone-filename exclusions-file jars))

    "--filter-jar"
    (let [[input-jar output-jar removals-file & extra] args]
      (when (or (nil? input-jar) (nil? output-jar) (nil? removals-file) (seq extra))
        (throw (IllegalArgumentException. "Usage: --filter-jar <input-jar> <output-jar> <removals-file>")))
      (filter-jar! input-jar output-jar removals-file))

    (throw (IllegalArgumentException. (str "Unknown zip-parallel command: " command)))))
