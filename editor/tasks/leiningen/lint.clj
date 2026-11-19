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

(ns leiningen.lint
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [leiningen.core.classpath :as classpath]
            [leiningen.core.eval :as eval]
            [leiningen.core.main :as main]
            [leiningen.core.project :as project]
            [leiningen.core.utils :as utils])
  (:import [java.io File InputStream RandomAccessFile]
           [java.net URI]
           [java.nio.file CopyOption Files StandardCopyOption]
           [java.util.zip ZipFile]
           [org.apache.commons.codec.digest DigestUtils]
           [org.apache.commons.compress.archivers.tar TarArchiveInputStream]
           [org.apache.commons.compress.compressors.gzip GzipCompressorInputStream]))

(set! *warn-on-reflection* true)

(def ^:private babashka
  {:repository "babashka/babashka"
   :name "babashka"
   :executable "bb"
   :version "1.13.229"
   :archives {[:macosx :aarch_64] ["macos-aarch64.tar.gz" "2d96ff06ce66883caa873fb717941a58c45523e3db9d91424e9f81b038896331"]
              [:macosx :x86_64] ["macos-amd64.tar.gz" "5d7aff8ffa8237f6e12cf879fde98777d9378e7fa78bb943815c3cfba184d78f"]
              [:linux :aarch_64] ["linux-aarch64-static.tar.gz" "d85a95210efe6d8fd3b534f3d594948e3f30fa1c5864044773105ad3e2d79096"]
              [:linux :x86_64] ["linux-amd64-static.tar.gz" "64fb984991c6e68f44e8a9391176a9bb411f9b9ae9959a6bca5307c75a7414da"]
              [:windows :x86_64] ["windows-amd64.zip" "eda55ef005e99503f6be97966f9a956f348f29affc7b2df9142d3fc6d239c855"]}})

(def ^:private clj-kondo
  {:repository "clj-kondo/clj-kondo"
   :name "clj-kondo"
   :executable "clj-kondo"
   :version "2026.05.25"
   :archives {[:macosx :aarch_64] ["macos-aarch64.zip" "595e2dcac9e2c8f355e41f406583bcd26851bb2d192f71f4d7ec169d16004b97"]
              [:macosx :x86_64] ["macos-amd64.zip" "c54a5e2af73e943e51a48801605d61d4e45e234dc3a62034cf4807a61d12ad23"]
              [:linux :aarch_64] ["linux-aarch64.zip" "aaec0fe483ad4542f6739f30fdf46334cb94ecd9bdcd205962de18ca11603240"]
              [:linux :x86_64] ["linux-static-amd64.zip" "ba8742f90555332852a3f63260611ca75cc7ba4e2b6dfe601bf5011bb9402b45"]
              [:windows :x86_64] ["windows-amd64.zip" "40b35248186733c916f2e25ec06e23b1ee264c0923e39aaa605fe79707e017e8"]}})

(defn- extract-executable! [archive ^File download ^String executable ^File staged]
  (if (string/ends-with? archive ".zip")
    (with-open [zip (ZipFile. download)]
      (let [entry (.getEntry zip executable)]
        (when-not entry
          (throw (ex-info (str "Archive is missing " executable) {})))
        (with-open [input (.getInputStream zip entry)]
          (io/copy input staged))))
    (with-open [input (io/input-stream download)
                gzip (GzipCompressorInputStream. input)
                tar (TarArchiveInputStream. gzip)]
      (loop []
        (let [entry (.getNextTarEntry tar)]
          (cond
            (nil? entry)
            (throw (ex-info (str "Archive is missing " executable) {}))

            (= executable (.getName entry))
            (io/copy tar staged)

            :else
            (recur)))))))

(defn- cached-executable
  ^String [project [os arch :as platform] {:keys [repository name executable version archives]}]
  (let [[archive sha256]
        (or (get archives platform)
            (main/abort (format "No %s lint binary is available for %s %s." name os arch)))

        executable (cond-> executable (= :windows os) (str ".exe"))

        directory
        (io/file (:root project) ".cache" "lint-tools" (str archive "-" (subs sha256 0 12)))

        binary (io/file directory executable)]
    (when-not (.isFile binary)
      (.mkdirs directory)
      (with-open [lock-file (RandomAccessFile. (io/file directory ".lock") "rw")
                  channel (.getChannel lock-file)
                  _lock (.lock channel)]
        (when-not (.isFile binary)
          (when (:offline? project)
            (main/abort (format "%s %s is missing from the lint cache. Run lein lint online once." name version)))
          (let [url
                (format "https://github.com/%s/releases/download/v%s/%s-%s-%s"
                        repository
                        version
                        name
                        version
                        archive)

                download (File/createTempFile "download-" ".tmp" directory)]
            (try
              (main/info (format "Downloading %s %s for lein lint" name version))
              (let [connection (.openConnection (.toURL (URI/create url)))]
                (.setConnectTimeout connection 10000)
                (.setReadTimeout connection 30000)
                (with-open [input (.getInputStream connection)]
                  (io/copy input download)))
              (let [actual
                    (with-open [^InputStream input (io/input-stream download)]
                      (DigestUtils/sha256Hex input))]
                (when-not (= sha256 actual)
                  (throw (ex-info (format "SHA-256 mismatch for %s" url)
                                  {:expected sha256 :actual actual}))))
              (let [staged (File/createTempFile "executable-" ".tmp" directory)]
                (try
                  (extract-executable! archive download executable staged)
                  (when-not (or (= :windows os) (.setExecutable staged true))
                    (throw (ex-info (str "Could not make executable: " staged) {})))
                  (Files/move (.toPath staged) (.toPath binary)
                              (into-array CopyOption [StandardCopyOption/ATOMIC_MOVE]))
                  (finally
                    (Files/deleteIfExists (.toPath staged)))))
              (finally
                (Files/deleteIfExists (.toPath download))))))))
    (.getAbsolutePath binary)))

(defn lint
  "Check Clojure files with clj-kondo, cljfmt, and the editor layout rules.

  Use - to read Clojure source from stdin.
  Use --uncommitted to include staged, unstaged, and untracked Clojure files.
  Use --test on its own to run the local tests under test-lint.
  Babashka and clj-kondo are cached under .cache/lint-tools on first use."
  [project & filenames]
  (let [filenames (cond-> filenames (= "--" (first filenames)) rest)]
    (when-not (pos? (count filenames))
      (println "Usage: lein lint [--uncommitted] [<file> | -] ...")
      (println "       lein lint --test")
      (main/exit 1))
    (let [platform [(utils/get-os) (utils/get-arch)]
          bb (cached-executable project platform babashka)
          kondo (cached-executable project platform clj-kondo)

          lint-project
          (assoc #_{:clj-kondo/ignore [:unresolved-var]}
            (project/set-profiles project [:lint])
            :offline? (:offline? project))

          jars
          (classpath/resolve-managed-dependencies :dependencies :managed-dependencies lint-project)

          classpath
          (String/join File/pathSeparator
                       ^Iterable (into (cond-> (vec (:source-paths lint-project))
                                         (contains? (set filenames) "--test")
                                         (conj (str (io/file (:root project) "test-lint"))))
                                       (mapv str jars)))

          status
          (binding [eval/*dir* (:root project)
                    eval/*pump-in* (contains? (set filenames) "-")]
            (apply eval/sh bb "--classpath" classpath "--main" "lint-runner" kondo filenames))]
      (when-not (zero? status)
        (main/exit status)))))
