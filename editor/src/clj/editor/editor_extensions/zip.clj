;; Copyright 2020-2025 The Defold Foundation
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
            [editor.fs :as fs]
            [editor.future :as future])
  (:import [java.nio.file Path]
           [org.apache.commons.compress.archivers.zip ZipArchiveEntry ZipArchiveOutputStream]
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
  (when-not (fs/path-exists? p)
    (throw (LuaError. (str "Source path does not exist: " p)))))

(defn- validate-entry-target-path! [^Path p]
  (when (or (.startsWith p "/")
            (.isAbsolute p))
    ;; on Windows, a path starting with "/" is not absolute, but we don't want
    ;; to support such paths since we use unix-style paths in the archive.
    ;; Additionally, a path that does not start with "/" might still be
    ;; absolute, e.g. "C:\Users"
    (throw (LuaError. (str "Target path must be relative: " p))))
  (when (.startsWith p "..")
    (throw (LuaError. (str "Target path is above archive root: " p)))))

(defn- write-file! [^ZipArchiveOutputStream zos ^Path source ^Path target]
  (let [entry-name (string/replace target \\ \/)]
    (.putArchiveEntry
      zos
      (doto (ZipArchiveEntry. entry-name)
        (.setSize (fs/path-size source))
        (.setTime (fs/path-last-modified-time source))))
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
                 (not (fs/path-exists? output-path)) (fs/make-path-parents output-path)
                 (fs/path-is-directory? output-path) (throw (LuaError. (str "Output path is a directory: " archive-path)))
                 :else (fs/delete-path-file! output-path))
               (with-open [zos (ZipArchiveOutputStream. (.toFile output-path))]
                 (run!
                   (fn pack-entry [entry]
                     (let [entry (if (string? entry) {1 entry} entry)
                           source-str ^String (get entry 1)
                           source (doto (.resolve project-path source-str) validate-entry-source-path!)
                           target-str (get entry 2 source-str)
                           target (doto (.normalize (fs/path target-str)) validate-entry-target-path!)
                           method (method-enum->value (:method entry) default-method)
                           level (:level entry default-level)]
                       (.setMethod zos method)
                       (.setLevel zos level)
                       (if (fs/path-is-directory? source)
                         (run! (fn pack-file [^Path file-path]
                                 (write-file! zos file-path (.resolve target (.relativize source file-path))))
                               (fs/path-walker source))
                         (write-file! zos source target))))
                   entries))))
           (future/then (fn [_] (reload-resources!)))
           (future/then rt/and-refresh-context))))))

(defn env [project-path reload-resources!]
  {"pack" (make-ext-pack-fn project-path reload-resources!)
   "METHOD" {"DEFLATED" (coerce/enum-lua-value-cache :deflated)
             "STORED" (coerce/enum-lua-value-cache :stored)}})