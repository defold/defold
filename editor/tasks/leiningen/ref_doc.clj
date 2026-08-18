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

(ns leiningen.ref-doc
  "Copy/download ref-doc and unzip."
  (:require [clojure.java.io :as io]
            [clojure.string :as str]
            [leiningen.util.http-cache :as http-cache])
  (:import (java.util.zip ZipFile)))

(defn- delete-dir
  "Delete a directory and its contents."
  [path]
  (let [dir (io/file path)]
    (when (.exists dir)
      (doseq [file (reverse (file-seq dir))]
        (io/delete-file file)))))

(defn- unzip
  "Copies SDoc and Lua annotation entries from `source` to their target directories."
  [source sdoc-target-dir lua-target-dir]
  (with-open [zip (ZipFile. (io/file source))]
    (let [entries (.entries zip)]
      (loop []
        (when (.hasMoreElements entries)
          (let [entry (.nextElement entries)
                entryname (.getName entry)
                filename (.getName (io/file entryname))
                annotation-match (re-matches #"doc/lua-annotations/(runtime|editor)/([^/]+\.lua)" entryname)]
            (cond
              (and (str/ends-with? filename ".sdoc")
                   (not= filename "editor_doc.sdoc"))
              (io/copy (.getInputStream zip entry)
                       (io/file sdoc-target-dir filename))

              annotation-match
              (let [[_ context annotation-name] annotation-match
                    dest (io/file lua-target-dir context annotation-name)]
                (.mkdirs (.getParentFile dest))
                (io/copy (.getInputStream zip entry) dest)))
            (recur)))))))

(defn- ref-doc-zip
  "Get the ref-doc.zip from either S3 archive or DYNAMO_HOME."
  [archive sha]
  (if sha
    (http-cache/download (format "https://%s/archive/%s/engine/share/ref-doc.zip" archive sha))
    (io/file (format "%s/share/ref-doc.zip" (get (System/getenv) "DYNAMO_HOME")))))

(defn ref-doc [project & [git-sha]]
  (let [sdoc-target-dir "resources/doc"
        lua-target-dir "resources/lua-annotations"]
    (doseq [target-dir [sdoc-target-dir lua-target-dir]]
      (delete-dir target-dir)
      (.mkdirs (io/file target-dir)))
    (let [sha (or git-sha (:engine project))
          archive-domain (get project :archive-domain)]
      (unzip (ref-doc-zip archive-domain sha)
             sdoc-target-dir
             lua-target-dir))))
