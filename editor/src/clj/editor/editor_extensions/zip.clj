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

(ns editor.editor-extensions.zip
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.editor-extensions.coerce :as coerce]
            [editor.editor-extensions.runtime :as rt]
            [editor.future :as future]
            [editor.os :as os]
            [util.coll :as coll]
            [util.path :as path])
  (:import [java.nio.file Files Path]
           [java.nio.file.attribute PosixFilePermission]
           [java.util EnumSet]
           [org.apache.commons.compress.archivers.zip ZipArchiveEntry ZipArchiveOutputStream ZipFile]
           [org.apache.commons.io FilenameUtils]
           [org.luaj.vm2 LuaError LuaValue]))

(set! *warn-on-reflection* true)

(def common-opts
  {:method (coerce/enum :deflated :stored)
   :level (coerce/wrap-with-pred coerce/integer #(<= 0 % 9) "should be between 0 and 9")})

(def create-opts-coercer
  (coerce/one-of
    coerce/null
    (coerce/hash-map :opt common-opts
                     :extra-keys false)))

(def entries-coercer
  (coerce/vector-of
    (coerce/one-of
      coerce/string
      (coerce/hash-map :req {1 coerce/string}
                       :opt (assoc common-opts 2 coerce/string)
                       :extra-keys false))))

(def method-enum->value
  {:deflated ZipArchiveOutputStream/DEFLATED
   :stored ZipArchiveOutputStream/STORED})

(defn- validate-entry-source-path! [p]
  (when-not (path/exists? p)
    (throw (LuaError. (str "Source path does not exist: " p)))))

(defn- absolute-path? [^Path p]
  ;; on Windows, a path starting with "/" is not absolute, but we don't want
  ;; to support such paths since we use unix-style paths in the archive.
  ;; Additionally, a path that does not start with "/" might still be
  ;; absolute, e.g. "C:\Users"
  (or (.startsWith p "/")
      (.isAbsolute p)))

(defn- validate-entry-target-path! [^Path p]
  (when (absolute-path? p)
    (throw (LuaError. (str "Target path must be relative: " p))))
  (when (.startsWith p "..")
    (throw (LuaError. (str "Target path is above archive root: " p)))))

(def ^:private permission-bits
  [(coll/pair PosixFilePermission/OWNER_READ     0400)
   (coll/pair PosixFilePermission/OWNER_WRITE    0200)
   (coll/pair PosixFilePermission/OWNER_EXECUTE  0100)
   (coll/pair PosixFilePermission/GROUP_READ     0040)
   (coll/pair PosixFilePermission/GROUP_WRITE    0020)
   (coll/pair PosixFilePermission/GROUP_EXECUTE  0010)
   (coll/pair PosixFilePermission/OTHERS_READ    0004)
   (coll/pair PosixFilePermission/OTHERS_WRITE   0002)
   (coll/pair PosixFilePermission/OTHERS_EXECUTE 0001)])

(defn- get-unix-mode [^Path source]
  (let [permissions (path/posix-file-permissions source)]
    (transduce (map #(if (.contains permissions (key %)) (val %) 0)) + 0 permission-bits)))

(defn- get-permissions [unix-mode]
  (reduce
    (fn [^EnumSet acc e]
      (if (zero? (bit-and unix-mode (val e)))
        acc
        (doto acc (.add (key e)))))
    (EnumSet/noneOf PosixFilePermission)
    permission-bits))

(defn- write-file! [^ZipArchiveOutputStream zos ^Path source ^Path target]
  (let [entry-name (string/replace target \\ \/)]
    (.putArchiveEntry
      zos
      (doto (ZipArchiveEntry. entry-name)
        (cond-> (not (os/is-win32?)) (.setUnixMode (get-unix-mode source)))
        (.setSize (path/byte-size source))
        (.setTime (path/last-modified-ms source))))
    (with-open [is (io/input-stream source)]
      (io/copy is zos))
    (.closeArchiveEntry zos)))

(defn- make-ext-pack-fn [^Path project-path reload-resources!]
  (rt/suspendable-lua-fn ext-pack
    ([ctx lua-archive-path lua-entries]
     (ext-pack ctx lua-archive-path LuaValue/NIL lua-entries))
    ([{:keys [rt]} lua-archive-path lua-opts lua-entries]
     (let [^String archive-path (rt/->clj rt coerce/string lua-archive-path)
           opts (rt/->clj rt create-opts-coercer lua-opts)
           entries (rt/->clj rt entries-coercer lua-entries)
           default-method (method-enum->value (:method opts :deflated))
           default-level (:level opts ZipArchiveOutputStream/DEFAULT_COMPRESSION)]
       (-> (future/io
             (let [output-path (.normalize (.resolve project-path archive-path))]
               (cond
                 (not (path/exists? output-path)) (path/create-parent-directories! output-path)
                 (path/directory? output-path) (throw (LuaError. (str "Output path is a directory: " archive-path)))
                 :else (path/delete! output-path))
               (with-open [zos (ZipArchiveOutputStream. (.toFile output-path))]
                 (run!
                   (fn pack-entry [entry]
                     (let [entry (if (string? entry) {1 entry} entry)
                           source-str ^String (get entry 1)
                           source (doto (.resolve project-path source-str) validate-entry-source-path!)
                           target-str (get entry 2 source-str)
                           target (doto (path/normalized target-str) validate-entry-target-path!)
                           method (method-enum->value (:method entry) default-method)
                           level (:level entry default-level)]
                       (.setMethod zos method)
                       (.setLevel zos level)
                       (if (path/directory? source)
                         (run! (fn pack-file [^Path file-path]
                                 (write-file! zos file-path (.resolve target (.relativize source file-path))))
                               (path/tree-walker source))
                         (write-file! zos source target))))
                   entries))))
           (future/then (fn [_] (reload-resources!)))
           (future/then rt/and-refresh-context))))))

(def ^:private unpack-args-coercer
  (coerce/regex :archive-path coerce/string
                :target-path :? coerce/string
                :opts :? (coerce/wrap-with-pred
                           (coerce/hash-map :opt {:on_conflict (coerce/enum :error :skip :overwrite)}
                                            :extra-keys false)
                           #(pos? (count %)) "at least one option is required")
                :paths :? (coerce/vector-of
                            (-> coerce/string
                                (coerce/wrap-transform path/normalized)
                                (coerce/wrap-with-pred #(not (.startsWith ^Path % "..")) "is above root")
                                (coerce/wrap-with-pred #(not (absolute-path? %)) "is absolute")
                                (coerce/wrap-transform #(FilenameUtils/separatorsToUnix (str %)))
                                (coerce/wrap-with-pred #(not (coll/empty? %)) "is empty"))
                            :min-count 1)))

(def ^:private default-unpack-opts {:on_conflict :error})

(defn- make-ext-unpack-fn [^Path project-path reload-resources!]
  (rt/suspendable-varargs-lua-fn ext-unpack [{:keys [rt]} varargs]
    (let [{:keys [^String archive-path ^String target-path opts paths]
           :or {opts default-unpack-opts}} (rt/->clj rt unpack-args-coercer varargs)]
      (-> (future/io
            (let [archive-path (.resolve project-path archive-path)
                  ^Path target-path (if target-path
                                      (.resolve project-path target-path)
                                      (.getParent archive-path))]
              (cond
                (not (path/exists? archive-path)) (throw (LuaError. (str "Archive path does not exist: " archive-path)))
                (path/directory? archive-path) (throw (LuaError. (str "Archive path is a directory: " archive-path))))
              (with-open [zip (ZipFile. (.toFile archive-path))]
                (let [entries (.getEntries zip)]
                  (while (.hasMoreElements entries)
                    (let [^ZipArchiveEntry e (.nextElement entries)]
                      (when-not (.isDirectory e)
                        (let [entry-path-str (FilenameUtils/separatorsToUnix (.getName e))]
                          (when (or (nil? paths)
                                    (coll/some (fn [^String s]
                                                 (or (= s entry-path-str)
                                                     (and (< (.length s) (.length entry-path-str))
                                                          (= \/ (.charAt entry-path-str (.length s)))
                                                          (.startsWith entry-path-str s))))
                                               paths))
                            (let [target-relative-path (path/normalized entry-path-str)]
                              (when-not paths
                                ;; when we have paths, we only allow the valid
                                ;; ones already; if we don't have paths, we need
                                ;; to validate them separately
                                (validate-entry-target-path! target-relative-path))
                              (let [target (.resolve target-path target-relative-path)]
                                (when (or (not (path/exists? target))
                                          (case (:on_conflict opts)
                                            :skip false
                                            :overwrite (if (path/directory? target)
                                                         (throw (LuaError. (str "Can't overwrite directory: " target)))
                                                         true)
                                            :error (throw (LuaError. (str "Path already exists: " target)))))
                                  (path/create-parent-directories! target)
                                  (with-open [in (.getInputStream zip e)
                                              out (io/output-stream target)]
                                    (io/copy in out))
                                  (some->> (.getLastModifiedTime e) (Files/setLastModifiedTime target))
                                  (when-not (os/is-win32?)
                                    (let [unix-mode (.getUnixMode e)]
                                      (when-not (zero? unix-mode)
                                        (Files/setPosixFilePermissions target (get-permissions unix-mode)))))))))))))))))
          (future/then (fn [_] (reload-resources!)))
          (future/then rt/and-refresh-context)))))

(defn env [project-path reload-resources!]
  {"pack" (make-ext-pack-fn project-path reload-resources!)
   "unpack" (make-ext-unpack-fn project-path reload-resources!)
   "ON_CONFLICT" {"ERROR" (coerce/enum-lua-value-cache :error)
                  "SKIP" (coerce/enum-lua-value-cache :skip)
                  "OVERWRITE" (coerce/enum-lua-value-cache :overwrite)}
   "METHOD" {"DEFLATED" (coerce/enum-lua-value-cache :deflated)
             "STORED" (coerce/enum-lua-value-cache :stored)}})