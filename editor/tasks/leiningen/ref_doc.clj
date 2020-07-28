;; Copyright 2020 The Defold Foundation
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
  "Takes the path to a zipfile `source` and unzips it to target-dir."
  [source target-dir]
  (with-open [zip (ZipFile. (io/file source))]
    (doseq [entry (enumeration-seq (.entries zip))]
      (let [entryname (.getName entry)
            filename (.getName (io/file entryname))
            dest (io/file target-dir filename)]
        (when (str/ends-with? filename ".sdoc")
          (io/copy (.getInputStream zip entry) dest))))))

(defn- ref-doc-zip
  "Get the ref-doc.zip from either d.defold.com or DYNAMO_HOME."
  [sha]
  (if sha
    (http-cache/download (format "https://d.defold.com/archive/%s/engine/share/ref-doc.zip" sha))
    (io/file (format "%s/share/ref-doc.zip" (get (System/getenv) "DYNAMO_HOME")))))

(defn ref-doc [project & [git-sha]]
  (let [out-path "resources/doc"]
    (delete-dir out-path)
    (.mkdirs (io/file out-path))
    (let [sha (or git-sha (:engine project))]
      (unzip (ref-doc-zip sha)
             out-path))))
