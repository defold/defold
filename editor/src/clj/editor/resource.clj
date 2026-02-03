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

(ns editor.resource
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [cognitect.transit :as transit]
            [dynamo.graph :as g]
            [editor.core :as core]
            [editor.fs :as fs]
            [schema.core :as s]
            [util.coll :as coll :refer [pair]]
            [util.defonce :as defonce]
            [util.digest :as digest]
            [util.fn :as fn]
            [util.http-server :as http-server]
            [util.path :as path]
            [util.text-util :as text-util])
  (:import [clojure.lang PersistentHashMap]
           [com.defold.editor Editor]
           [java.io Closeable File FilterInputStream IOException InputStream]
           [java.net URI]
           [java.nio.file FileSystem FileSystems]
           [java.util.zip ZipEntry ZipFile]
           [org.apache.commons.io FilenameUtils IOUtils]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce/protocol ResourceListener
  (handle-changes [this changes render-progress!]))

(defonce/protocol Resource
  (children [this])
  (ext [this])
  (resource-type [this])
  (source-type [this])
  (exists? [this])
  (read-only? [this])
  (symlink? [this])
  (path [this])
  (abs-path ^String [this])
  (proj-path ^String [this])
  (resource-name ^String [this])
  (workspace [this])
  (resource-hash [this])
  (openable? [this])
  (editable? [this])
  (loaded? [this]))

(def ^{:arglists '([x])} resource?
  (let [cache (atom {})
        not-found (Object.)]
    (fn resource? [x]
      (let [c (class x)
            ret (@cache c not-found)]
        (if (identical? not-found ret)
          (let [ret (satisfies? Resource x)]
            (swap! cache assoc c ret)
            ret)
          ret)))))

(def placeholder-resource-type-ext "*")

(defn filename->type-ext
  "Given a filename or path string, returns the lower-case file extension
  (without the leading dot character), or nil if the supplied filename is nil.
  Returns an empty string if the supplied string has no file extension."
  ^String [^String filename]
  (string/lower-case (FilenameUtils/getExtension filename)))

(defn type-ext [resource]
  (string/lower-case (ext resource)))

(definline project-directory
  "Returns a File representing the canonical path of the project directory."
  ^File [basis workspace]
  `(io/as-file (g/raw-property-value ~basis ~workspace :root)))

(defn resource-types-by-type-ext [basis workspace editability]
  (let [property-label (case editability
                         (true :editable) :resource-types
                         (false :non-editable) :resource-types-non-editable)]
    (g/raw-property-value basis workspace property-label)))

(defn lookup-resource-type [basis workspace resource]
  (let [resource-types (resource-types-by-type-ext basis workspace (editable? resource))
        ext (type-ext resource)]
    (or (resource-types ext)
        (resource-types placeholder-resource-type-ext))))

(defn overridable-resource-type?
  "Returns whether the supplied value is a resource-type that hosts properties
  that may be overridden."
  [value]
  (contains? (:tags value) :overridable-properties))

(defn overridable?
  "Returns whether the Resource hosts properties that may be overridden. Throws
  if supplied a non-Resource value."
  [resource]
  (overridable-resource-type? (resource-type resource)))

(defn overridable-resource?
  "Returns whether the supplied value is a Resource whose type hosts properties
  that may be overridden."
  [value]
  (and (resource? value)
       (overridable? value)))

(defn openable-resource? [value]
  ;; A resource is considered openable if its kind can be opened. Typically this
  ;; is a resource that is part of the project and is not a directory. Note
  ;; that the resource does not have to be openable in the Defold Editor - an
  ;; external application could be assigned to handle it. Before opening, you
  ;; must also make sure the resource exists.
  (and (resource? value)
       (openable? value)))

(defn editor-openable-resource? [value]
  ;; A resource is considered editable if the Defold Editor can edit it. Before
  ;; opening, you must also make sure the resource exists.
  (and (openable-resource? value)
       (true? (:editor-openable (resource-type value)))))

(defn- ->unix-seps ^String [^String path]
  (FilenameUtils/separatorsToUnix path))

(defn relative-path
  ^String [^File f1 ^File f2]
  ;; The strange comparison below is done due to the fact that we support case
  ;; insensitive file systems. For example NTFS and HFS. We want to compare the
  ;; paths without case but preserve the casing as supplied by the caller in the
  ;; result we produce.
  (let [p1-lower (string/lower-case (->unix-seps (str (.getAbsolutePath f1))))
        p2-abs (->unix-seps (str (.getAbsolutePath f2)))
        p2-lower (string/lower-case p2-abs)
        path (if (string/starts-with? p2-lower p1-lower)
               (subs p2-abs (count p1-lower))
               p2-abs)]
    (if (string/starts-with? path "/")
      (subs path 1)
      path)))

(defn file->proj-path [^File project-directory ^File file]
  (try
    (str "/" (relative-path project-directory file))
    (catch IllegalArgumentException _
      nil)))

(defn parent-proj-path [^String proj-path]
  (when-let [last-slash (string/last-index-of proj-path "/")]
    (subs proj-path 0 last-slash)))

(def ^:private defignore-cache
  ;; path->{:mtime ... :pred ...}
  (atom {}))

(defn lines->proj-path-patterns-pred
  "Returns a predicate that takes a proj-path and returns true if it starts with
  or matches one of the proj-paths listed among the lines, typically the
  contents of something like a `.defignore` file."
  [lines]
  (let [patterns (when lines
                   (coll/into-> lines []
                     (filter #(string/starts-with? % "/"))
                     (map #(string/replace % #"/*$" ""))
                     (distinct)))]
    (if (zero? (count patterns))
      fn/constantly-false
      (fn matched-proj-path? [^String proj-path]
        (let [proj-path-length (.length proj-path)]
          (boolean
            (coll/some
              (fn [^String pattern]
                ;; Make sure a "/dir" pattern matches "/dir" and "/dir/entry",
                ;; but not "/dire".
                (and (string/starts-with? proj-path pattern)
                     (let [pattern-length (.length pattern)]
                       (or (= pattern-length proj-path-length)
                           (= \/ (.charAt proj-path pattern-length))))))
              patterns)))))))

(defn- read-proj-path-patterns-pred [^File proj-path-patterns-file]
  (lines->proj-path-patterns-pred
    (when (.isFile proj-path-patterns-file)
      (string/split-lines (slurp proj-path-patterns-file)))))

(defn defignore-pred [^File root]
  (let [defignore-file (io/file root ".defignore")
        defignore-path (.getCanonicalPath defignore-file)
        latest-mtime (.lastModified defignore-file)
        {:keys [mtime pred]} (get @defignore-cache defignore-path)]
    (if (= mtime latest-mtime)
      pred
      (let [pred (read-proj-path-patterns-pred defignore-file)]
        (swap! defignore-cache assoc defignore-path {:mtime latest-mtime :pred pred})
        pred))))

(defn- defunload-pred-raw [^File root]
  ;; Contrary to .defignore, we only read the .defunload file once and cache it
  ;; for the entire lifetime of the application. You'll have to restart the
  ;; editor for any changes to take effect.
  {:pre [(instance? File root)]} ; Guard against duplicate memoization since io/file accepts multiple types.
  (let [defunload-file (io/file root ".defunload")]
    (read-proj-path-patterns-pred defunload-file)))

(def defunload-pred (fn/memoize defunload-pred-raw))

(defonce ^File defunload-issues-file
  (let [report-file-atom (atom nil)]
    (fn defunload-issues-file
      ^File []
      (if-some [existing-report-file @report-file-atom]
        (pair existing-report-file false)
        (let [log-directory (.getAbsoluteFile (.toFile (Editor/getLogDirectory)))
              report-file (io/file log-directory "defunload-issues.txt")
              we-assigned-atom (compare-and-set! report-file-atom nil report-file)]
          (when we-assigned-atom
            ;; We will append any issues encountered when resources are loaded
            ;; to this report file. Make sure it is initially empty for this
            ;; session.
            (fs/create-file! report-file ""))
          (pair report-file we-assigned-atom))))))

(def ^:dynamic *defignore-pred* nil)

(defmacro with-defignore-pred [root-expr & body]
  `(binding [*defignore-pred* (defignore-pred ~root-expr)]
     ~@body))

(defn ignored-project-path? [^File root proj-path]
  ((or *defignore-pred* (defignore-pred root)) proj-path))

(defn- textual-resource-type?
  "Returns whether the resource type is marked as textual. Note that the
  placeholder resource type reports as textual, but might later call
  [[util.text-util/binary?]] to estimate if the content is textual or not."
  [resource-type]
  {:pre [(some? resource-type)]
   :post [(boolean? %)]}
  (:textual? resource-type))

(defn- content-type [resource]
  (or (http-server/ext->content-type (type-ext resource))
      (if (textual-resource-type? (resource-type resource))
        "text/plain"
        "application/octet-stream")))

;; Note! Used to keep a file here instead of path parts, but on
;; Windows (File. "test") equals (File. "Test") which broke
;; FileResource equality tests.
(defonce/record FileResource [workspace ^String root ^String abs-path ^String project-path ^String name ^String ext source-type editable loaded children]
  Resource
  (children [this] children)
  (ext [this] ext)
  (resource-type [this] (lookup-resource-type (g/unsafe-basis) workspace this))
  (source-type [this] source-type)
  (exists? [this]
    (try
      ;; The path must match the casing of the file on disk exactly. We treat
      ;; this as an error to make the user manually fix mismatches. We could fix
      ;; such references automatically by using the canonical path to create the
      ;; FileResource. However, some ids used by the engine are derived from the
      ;; file names, most notably AtlasImage ids. Since these can be referenced
      ;; from scripts, we make the user aware of the bad reference so she can
      ;; fix it manually and hopefully remember to update the scripts too.
      (let [file (io/file this)]
        (and (.exists file)
             (not (ignored-project-path? (io/file root) project-path))
             (string/ends-with? (->unix-seps (str (path/actual-cased file))) project-path)))
      (catch IOException _
        false)
      (catch SecurityException _
        false)))
  (read-only? [this]
    (try
      (not (.canWrite (io/file this)))
      (catch SecurityException _
        true)))
  (symlink? [this] (path/symlink? this))
  (path [this] (if (= "" project-path) "" (subs project-path 1)))
  (abs-path [this] abs-path)
  (proj-path [this] project-path)
  (resource-name [this] name)
  (workspace [this] workspace)
  (resource-hash [this] (hash (proj-path this)))
  (openable? [this]
    (and (= :file source-type)
         (if (:editor-openable (resource-type this))
           (loaded? this)
           true)))
  (editable? [_this] editable)
  (loaded? [_this] loaded)

  io/IOFactory
  (make-input-stream  [this opts] (io/make-input-stream (io/file this) opts))
  (make-reader        [this opts] (io/make-reader (io/make-input-stream this opts) opts))
  (make-output-stream [this opts] (io/make-output-stream (io/file this) opts))
  (make-writer        [this opts] (io/make-writer (io/make-output-stream this opts) opts))

  io/Coercions
  (as-file [this] (File. abs-path))

  path/Coercions
  (as-path [_this] (path/as-path abs-path))

  http-server/ContentType
  (content-type [resource] (content-type resource))

  http-server/->Data
  (->data [_] (path/as-path abs-path)))

(defn make-file-resource [workspace ^String root-path ^File file children editable-proj-path? unloaded-proj-path?]
  {:pre [(g/node-id? workspace)
         (string? root-path)]}
  (let [source-type (if (.isDirectory file) :folder :file)
        abs-path (.getAbsolutePath file)
        path (.getPath file)
        name (.getName file)
        project-path (if (= "" name) "" (str "/" (relative-path (File. root-path) (io/file path))))
        ext (FilenameUtils/getExtension path)
        editable (editable-proj-path? project-path)
        loaded (not (unloaded-proj-path? project-path))]
    (FileResource. workspace root-path abs-path project-path name ext source-type editable loaded children)))

(defn file-resource? [resource]
  (instance? FileResource resource))

(core/register-read-handler!
  "file-resource"
  (transit/read-handler
    (fn [{:keys [workspace ^String root ^String abs-path ^String project-path ^String name ^String ext source-type editable loaded children]}]
      (FileResource. workspace root abs-path project-path name ext source-type editable loaded children))))

(core/register-write-handler!
 FileResource
 (transit/write-handler
  (constantly "file-resource")
  (fn [^FileResource r]
    {:workspace (:workspace r)
     :abs-path (:abs-path r)
     :project-path (:project-path r)
     :name (:name r)
     :ext (:ext r)
     :source-type (:source-type r)
     :editable (:editable r)
     :children (:children r)})))

(defmethod print-method FileResource [file-resource ^java.io.Writer w]
  (.write w (format "{:FileResource %s}" (pr-str (proj-path file-resource)))))

;; Note that `data` is used for resource-hash, used to name
;; the output of build-resources. So better be unique for the
;; data the MemoryResource represents!
(defonce/record MemoryResource [workspace editable ext data]
  Resource
  (children [this] nil)
  (ext [this] ext)
  (resource-type [this] (lookup-resource-type (g/unsafe-basis) workspace this))
  (source-type [this] :file)
  (exists? [this] true)
  (read-only? [this] false)
  (symlink? [this] false)
  (path [this] nil)
  (abs-path [this] nil)
  (proj-path [this] nil)
  (resource-name [this] nil)
  (workspace [this] workspace)
  (resource-hash [this] (hash data))
  (openable? [this] false)
  (editable? [this] editable)
  (loaded? [_this] true)

  io/IOFactory
  (make-input-stream  [this opts] (io/make-input-stream (IOUtils/toInputStream ^String (:data this)) opts))
  (make-reader        [this opts] (io/make-reader (io/make-input-stream this opts) opts))
  (make-output-stream [this opts] (io/make-output-stream (.toCharArray ^String (:data this)) opts))
  (make-writer        [this opts] (io/make-writer (io/make-output-stream this opts) opts)))

(core/register-record-type! MemoryResource)

(defmethod print-method MemoryResource [memory-resource ^java.io.Writer w]
  (.write w (format "{:MemoryResource %s}" (pr-str (ext memory-resource)))))

(defn make-memory-resource [workspace resource-type data]
  (MemoryResource. workspace (:editable resource-type) (:ext resource-type) data))

(defn memory-resource? [resource]
  (instance? MemoryResource resource))

(defn counterpart-memory-resource
  "Given a MemoryResource, returns its editable or non-editable counterpart. We
  use this during build target fusion to ensure embedded resources from editable
  resources are fused with the equivalent embedded resources from non-editable
  resources."
  [memory-resource]
  {:pre [(memory-resource? memory-resource)]}
  (update memory-resource :editable not))

(defn- make-zip-resource-input-stream
  ^InputStream [zip-resource]
  (let [zip-file (ZipFile. ^File (io/as-file (:zip-uri zip-resource)))
        zip-entry (.getEntry zip-file (:zip-entry zip-resource))]
    (proxy [FilterInputStream] [(.getInputStream zip-file zip-entry)]
      (close []
        (let [^FilterInputStream this this]
          (proxy-super close)
          (.close zip-file))))))

(defonce/record ZipResource [workspace ^URI zip-uri name path zip-entry children]
  Resource
  (children [this] children)
  (ext [this] (FilenameUtils/getExtension name))
  (resource-type [this] (lookup-resource-type (g/unsafe-basis) workspace this))
  (source-type [this] (if (zero? (count children)) :file :folder))
  (exists? [this] (not (nil? zip-entry)))
  (read-only? [this] true)
  (symlink? [this] false) ; Note: Zip archives can contain symlinks. The ZipFile class doesn't support them, but the zip FileSystem implementation does.
  (path [this] path)
  (abs-path [this] nil)
  (proj-path [this] (.concat "/" path))
  (resource-name [this] name)
  (workspace [this] workspace)
  (resource-hash [this] (hash (proj-path this)))
  (openable? [this]
    (and (= :file (source-type this))
         (if (:editor-openable (resource-type this))
           (loaded? this)
           true)))
  (editable? [_this] true)
  (loaded? [_this] true)

  io/IOFactory
  (make-input-stream  [this opts] (make-zip-resource-input-stream this))
  (make-reader        [this opts] (io/make-reader (io/make-input-stream this opts) opts))
  (make-output-stream [this opts] (throw (Exception. "Zip resources are read-only")))
  (make-writer        [this opts] (throw (Exception. "Zip resources are read-only")))

  io/Coercions
  (as-file [this] (io/as-file zip-uri))

  path/Coercions
  (as-path [_this] (path/as-path zip-uri))

  http-server/ContentType
  (content-type [resource] (content-type resource))

  http-server/->Connection
  (->connection [_]
    (let [zip-file (ZipFile. (io/file zip-uri))
          entry (.getEntry zip-file zip-entry)]
      (reify
        http-server/ConnectionContentLength
        (connection-content-length [_]
          (let [size (.getSize entry)]
            (when-not (= -1 size) size)))
        io/IOFactory
        (make-input-stream [_ opts] (io/make-input-stream (.getInputStream zip-file entry) opts))
        (make-reader [this opts] (io/make-reader (io/make-input-stream this opts) opts))
        (make-output-stream [_ _] (throw (Exception. "Zip resources are read-only")))
        (make-writer [_ _] (throw (Exception. "Zip resources are read-only")))
        Closeable
        (close [_] (.close zip-file))))))

(defn zip-resource? [x]
  (instance? ZipResource x))

(core/register-record-type! ZipResource)

(core/register-read-handler!
 "zip-resource"
 (transit/read-handler
  (fn [{:keys [workspace ^String zip-uri name path zip-entry children]}]
    (ZipResource. workspace (URI. zip-uri) name path zip-entry children))))

(core/register-write-handler!
 ZipResource
 (transit/write-handler
  (constantly "zip-resource")
  (fn [^ZipResource r]
    {:workspace (:workspace r)
     :zip-uri   (.toString ^URI (:zip-uri r))
     :name      (:name r)
     :path      (:path r)
     :zip-entry (:zip-entry r)
     :children  (:children r)})))

(defmethod print-method ZipResource [zip-resource ^java.io.Writer w]
  (.write w (format "{:ZipResource %s}" (pr-str (proj-path zip-resource)))))

(defn- outside-base-path? [base-path ^ZipEntry entry]
  (and (seq base-path) (not (.startsWith (->unix-seps (.getName entry)) (str base-path "/")))))

(defn- path-relative-base [base-path zip-entry-name]
  (let [clean-zip-entry-name (->unix-seps zip-entry-name)]
    (if (coll/empty? base-path)
      clean-zip-entry-name
      (.replaceFirst clean-zip-entry-name (str base-path "/") ""))))

(defn- load-zip [^File zip-file ^String base-path]
  ;; NOTE: Even though it may not look like it here, we can only load
  ;; ZipResources from a File, since we later make use of the ZipFile class to
  ;; read the ZipEntries. Unfortunately, the ZipFile class can only operate on
  ;; File objects, so any .zip file we load must exist on disk. We unpack the
  ;; builtins.zip file from the bundled resources in the ResourceUnpacker at
  ;; startup to work around this.
  (when (.exists zip-file)
    (with-open [zip (ZipFile. zip-file)]
      (stream-into!
        []
        (keep (fn [^ZipEntry zip-entry]
                (when-not (or (.isDirectory zip-entry)
                              (outside-base-path? base-path zip-entry))
                  (let [zip-entry-name (.getName zip-entry)]
                    {:name (FilenameUtils/getName zip-entry-name)
                     :path (path-relative-base base-path zip-entry-name)
                     :zip-entry zip-entry-name
                     :crc (.getCrc zip-entry)}))))
        (.stream zip)))))

(defn- ->zip-resources [workspace zip-uri path [key val]]
  (let [path' (if (string/blank? path) key (str path "/" key))]
    (if (:path val) ; i.e. we've reached an actual entry with name, path, zip-entry
      (ZipResource. workspace zip-uri (:name val) (:path val) (:zip-entry val) nil)
      (ZipResource. workspace zip-uri key path' nil (mapv #(->zip-resources workspace zip-uri path' %) val)))))

(defn load-zip-resources
  ([workspace ^File zip-file]
   (load-zip-resources workspace zip-file nil))
  ([workspace ^File zip-file ^String base-path]
   (let [entries (load-zip zip-file base-path)
         zip-uri (.toURI zip-file)]
     {:tree (->> (reduce (fn [acc node] (assoc-in acc (string/split (:path node) #"/") node)) {} entries)
                 (mapv (fn [x] (->zip-resources workspace zip-uri "" x))))
      :crc (into {}
                 (map (juxt #(str "/" (:path %)) :crc))
                 entries)})))

(defn save-tracked?
  "Checks if the specified Resource should be connected to the save system."
  [resource]
  (and (file-resource? resource)
       (editable? resource)
       (loaded? resource)))

(g/defnode ResourceNode
  (property resource Resource :unjammable
            (dynamic visible (g/constantly false))))

(definline node-resource [basis resource-node]
  `(g/raw-property-value* ~basis ~resource-node :resource))

(defn base-name ^String [resource]
  (FilenameUtils/getBaseName (resource-name resource)))

(defn- non-empty-children [resource]
  (coll/not-empty (children resource)))

(defn resource-seq [root]
  (tree-seq non-empty-children non-empty-children root))

(def xform-recursive-resources
  (mapcat resource-seq))

(defn resource-map [roots]
  (coll/pair-map-by proj-path roots))

(defn resource->proj-path [resource]
  (if resource
    (proj-path resource)
    ""))

(defn resource->sha1-hex
  ^String [resource]
  (with-open [rs (io/input-stream resource)]
    (digest/stream->sha1-hex rs)))

(defn resource->sha256-hex
  ^String [resource]
  (with-open [rs (io/input-stream resource)]
    (digest/stream->sha256-hex rs)))

(defn resource->bytes
  ^bytes [resource]
  (with-open [rs (io/input-stream resource)]
    (IOUtils/toByteArray rs)))

(defn resource->path-inclusive-sha1-hex
  "For certain files, we want to include the proj-path in the sha1 identifier
  along with the file contents. For example, it used to be that two atlases
  that referred to separate copies of an image would be packed into a shared
  `.texturec` resource since the image contents matched. While this appears good
  on paper, there is an issue with it: If a texture is modified at runtime, it
  will affect both atlases. The user might expect this if both atlases refer to
  the same image file, but most would assume that having one of them refer to a
  copy of the image would make the packed atlas textures unique.

  See DEFEDIT-4218 for additional info about the rationale."
  ^String [resource]
  (with-open [input-stream (io/input-stream resource)
              digest-output-stream (digest/make-digest-output-stream "SHA-1")]
    (io/copy input-stream digest-output-stream)
    (when-some [^String proj-path (proj-path resource)]
      (.write digest-output-stream (.getBytes proj-path "UTF-8")))
    (.flush digest-output-stream)
    (digest/completed-stream->hex digest-output-stream)))

(defn resource->path-inclusive-sha1
  "Returns SHA-1 digest as bytes."
  ^bytes [resource]
  (with-open [input-stream (io/input-stream resource)
              digest-output-stream (digest/make-digest-output-stream "SHA-1")]
    (io/copy input-stream digest-output-stream)
    (when-some [^String proj-path (proj-path resource)]
      (.write digest-output-stream (.getBytes proj-path "UTF-8")))
    (.flush digest-output-stream)
    (.digest (.getMessageDigest digest-output-stream))))

(defn read-source-value+sha256-hex
  "Returns a pair of [read-fn-result, disk-sha256-or-nil]. If the resource is a
  file in the project, wraps the read in a DigestInputStream that concurrently
  hashes the file contents as they are read by the read-fn and returns the
  SHA-256 hex string in disk-sha256-or-nil. If the resource is not a project
  file, we use a plain InputStream, and disk-sha256-or-nil will be nil."
  [resource read-fn]
  (with-open [^InputStream input-stream
              (cond-> (io/input-stream resource)
                      (file-resource? resource)
                      (digest/make-digest-input-stream "SHA-256"))]
    (let [source-value (read-fn input-stream)
          disk-sha256 (digest/completed-stream->hex input-stream)]
      (pair source-value disk-sha256))))

(g/deftype ResourceVec [(s/maybe (s/protocol Resource))])

(defn temp-path [resource]
  (when (and resource (= :file (source-type resource)))
    (let [^File f (fs/create-temp-file! "tmp" (format ".%s" (ext resource)))]
      (with-open [in (io/input-stream resource)]
        (io/copy in f))
      (.getAbsolutePath f))))

(def ^:private ext->style-class
  ;; TODO: make extension-spine use :icon-class
  (let [config {"design" ["spinemodel" "spinescene"]}]
   (->> (for [[kind extensions] config
              :let [style-class (str "resource-kind-" kind)]
              ext extensions
              el [ext style-class]]
          el)
        seq
        PersistentHashMap/createWithCheck)))

(def icon-class->style-class
  (coll/pair-map-by identity #(str "resource-kind-" (name %)) [:design :property :script]))

(defn type-style-classes [resource-type]
  (if-let [explicit-class (or (some-> (:icon-class resource-type) icon-class->style-class)
                              (some-> (:ext resource-type) ext->style-class))]
    #{"resource" explicit-class}
    #{"resource"}))

(defn style-classes [resource]
  (case (source-type resource)
    :file (or (some-> (resource-type resource) type-style-classes)
              #{"resource"})
    :folder #{"resource" "resource-folder"}
    #{"resource"}))

(defn filter-resources [resources query]
  (let [file-system ^FileSystem (FileSystems/getDefault)
        matcher (.getPathMatcher file-system (str "glob:" query))]
    (filter (fn [r] (let [path (.getPath file-system (path r) (into-array String []))] (.matches matcher path))) resources)))

(defn internal? [resource]
  (string/starts-with? (resource->proj-path resource) "/_defold"))

(defn placeholder-resource-type?
  "Returns true if the specified resource-type is the placeholder resource type,
  or false otherwise."
  [resource-type]
  (= placeholder-resource-type-ext (:ext resource-type)))

(defn textual? [resource]
  "Returns whether the resource is considered textual based on its type. If
  we're unable to determine the type of the resource (i.e. it is a placeholder
  resource), we scan through part of the file to determine if it looks textual.
  The resource is expected to exist. If it does not, scanning the file will
  throw an exception."
  (let [resource-type (resource-type resource)]
    (if (placeholder-resource-type? resource-type)
      (not (text-util/binary? resource))
      (textual-resource-type? resource-type))))

(defn stateful-resource-type?
  "Returns whether the resource type is marked as stateful."
  [resource-type]
  {:pre [(some? resource-type)]
   :post [(boolean? %)]}
  (not (:stateless? resource-type)))

(defn stateful?
  "Returns whether the resource is stateful textual based on its type.
  Placeholder resources have a nil resource-type, but are assumed stateful since
  they can be edited as plain text using the code editor."
  [resource]
  (stateful-resource-type? (resource-type resource)))

(defn folder?
  "Returns true if the supplied resource is a folder."
  [resource]
  (case (source-type resource)
    :file false
    :folder true))

(def ^:private known-ext->language
  ;; See known language identifiers:
  ;; https://code.visualstudio.com/docs/languages/identifiers#_known-language-identifiers
  {"clj" "clojure"
   "cljc" "clojure"
   "css" "css"
   "html" "html"
   "java" "java"
   "js" "javascript"
   "md" "markdown"
   "plist" "xml"
   "py" "python"
   "sh" "shellscript"
   "xml" "xml"
   "yaml" "yaml"
   "yml" "yaml"})

(defn language [resource]
  (or (:language (resource-type resource))
      (known-ext->language (ext resource))
      "plaintext"))

(defn resource->text-matches [resource pattern]
  (when (and (exists? resource)
             (textual? resource))
    (text-util/readable->text-matches resource pattern)))

(defonce ^:private externally-available-extract-directory-delay
  (delay (fs/create-temp-directory! "extract-")))

(defn externally-available-absolute-path
  ^String [resource]
  (or (abs-path resource)
      (when-some [relative-path (path resource)]
        (let [temp-directory @externally-available-extract-directory-delay
              temp-file (io/file temp-directory relative-path)]
          (when-not (fs/existing-file? temp-file)
            (fs/create-parent-directories! temp-file)
            (with-open [input-stream (io/input-stream resource)]
              (io/copy input-stream temp-file))
            (fs/set-writable! temp-file false))
          (.getAbsolutePath temp-file)))))
