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

(ns leiningen.sass
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [leiningen.util.http-cache :as http-cache])
  (:import [java.util List Locale]
           [org.apache.commons.compress.archivers ArchiveInputStream]
           [org.apache.commons.compress.archivers.tar TarArchiveEntry TarArchiveInputStream]
           [org.apache.commons.compress.archivers.zip ZipArchiveEntry ZipArchiveInputStream]
           [org.apache.commons.compress.compressors.gzip GzipCompressorInputStream]))

(defprotocol GetArchiveEntryMode
  (get-archive-entry-mode [archive-entry]))

(extend-protocol GetArchiveEntryMode
  ZipArchiveEntry
  (get-archive-entry-mode [archive-entry]
    (.getUnixMode archive-entry))
  TarArchiveEntry
  (get-archive-entry-mode [archive-entry]
    (.getMode archive-entry)))

(defn- extract [archive-file output-path ext]
  (with-open [^ArchiveInputStream is
              (case ext
                "tar.gz" (-> archive-file io/input-stream GzipCompressorInputStream. TarArchiveInputStream.)
                "zip" (-> archive-file io/input-stream ZipArchiveInputStream.))]
    (loop []
      (when-let [e (.getNextEntry is)]
        (when-not (.isDirectory e)
          (let [output (io/file output-path (.getName e))
                executable (not (zero? (bit-and (get-archive-entry-mode e) 2r001000000)))]
            (io/make-parents output)
            (io/copy is output)
            (when executable (.setExecutable output true))))
        (recur)))))

(defn- get-dart-sass-executable-path [version packages-path]
  (let [os (.toLowerCase (System/getProperty "os.name") Locale/ROOT)
        platform (condp #(string/includes? %2 %1) os
                   "mac" "macos"
                   "win" "windows"
                   "linux" "linux")
        arch (if (and (= "macos" platform)
                      (= "aarch64" (System/getProperty "os.arch")))
               "arm64"
               "x64")
        ext (case platform
              ("macos" "linux") "tar.gz"
              "windows" "zip")
        url (format
              "https://github.com/sass/dart-sass/releases/download/%s/dart-sass-%s-%s-%s.%s"
              version
              version
              platform
              arch
              ext)
        extract-path (io/file packages-path "dart-sass" version)]
    (when-not (.exists extract-path)
      (extract (http-cache/download url) extract-path ext))
    (str (io/file extract-path
                  "dart-sass"
                  (str "sass"
                       (when (-> (System/getProperty "os.name") .toLowerCase (.contains "win"))
                         ".bat"))))))

(defn sass [project & [mode]]
  {:pre [(#{"once" "auto"} mode)]}
  (let [{:keys [target-path sass]} project
        {:keys [src output-directory]} sass
        packages-path (io/file target-path "packages")
        executable-path (get-dart-sass-executable-path "1.62.1" packages-path)
        _ (println (format "sass: %s -> %s" src output-directory))
        exit (-> (ProcessBuilder. ^List (concat
                                          [executable-path]
                                          (when (= "auto" mode)
                                            ["--watch"])
                                          ["--no-source-map"
                                           (str src ":" output-directory)]))
                 (doto (.inheritIO))
                 (.start)
                 (.waitFor))]
    (when-not (zero? exit)
      (throw (Exception. (format "non-zero exit code: %s" exit))))))
