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

(ns local-extensions-test
  (:require [clojure.edn :as edn]
            [clojure.java.io :as io]
            [clojure.test :refer :all]
            [editor.fs :as fs]
            [editor.process :as process]
            [support.process-util :as process-util]))

(set! *warn-on-reflection* true)

;; Parse real project defaults in a fresh JVM so already-loaded graph namespaces
;; cannot hide the dependency that raced Schema during parallel startup.
(deftest settings-url-loading-test
  (process-util/with-child-process {:err :inherit}
    '(do
       (System/setProperty "glass.platform" "headless")
       (System/setProperty "prism.order" "sw")
       (System/setProperty "defold.dev" "true")
       (System/setProperty "defold.extension.loading-test.url" "https://example.com/extension.zip")
       (require 'editor.game-project-core)
       (let [parse-url (requiring-resolve 'editor.settings-core/parse-setting-value)]
         (prn {:plain (str (parse-url {:type :url} "https://example.com/plain.zip"))
               :substituted (str (parse-url {:type :url} "{{defold.extension.loading-test.url}}"))
               :missing-property (try
                                   (parse-url {:type :url} "{{defold.extension.missing-test.url}}")
                                   (catch clojure.lang.ExceptionInfo exception
                                     (ex-data exception)))
               :loaded (into #{}
                             (filter (loaded-libs))
                             '[dynamo.graph editor.resource schema.core util.http-server local-extensions.server])}))) []
    (is (= {:plain "https://example.com/plain.zip"
            :substituted "https://example.com/extension.zip"
            :missing-property {:jvm-property-key "defold.extension.missing-test.url"
                               :raw-setting-value "{{defold.extension.missing-test.url}}"}
            :loaded #{}}
           (edn/read-string (process/capture! (process/out process)))))
    (is (zero? (process/await-exit-code process)))))

;; Local path overrides must still start the deferred server and serve a ZIP
;; with relative entry names while excluding generated build files.
(deftest local-extension-archive-test
  (let [directory (fs/create-temp-directory! "local-extension")]
    (try
      (doseq [[path content] [["nested/Mixed Case.txt" "extension source"]
                              ["build/generated.txt" "generated output"]]]
        (let [file (io/file directory path)]
          (io/make-parents file)
          (spit file content)))
      (process-util/with-child-process {:out :inherit :err :inherit}
        '(do
           (System/setProperty "glass.platform" "headless")
           (System/setProperty "prism.order" "sw")
           (System/setProperty "defold.extension.archive-test.url" "https://example.com/extension.zip")
           (System/setProperty "defold.extension.archive-test.path" (first *command-line-args*))
           (let [url ((requiring-resolve 'local-extensions/inject-jvm-properties)
                      "{{defold.extension.archive-test.url}}")]
             (with-open [input (java.util.zip.ZipInputStream. (.openStream (.toURL (java.net.URI. url))))]
               (let [entries (loop [entries {}]
                               (if-let [entry (.getNextEntry input)]
                                 (recur (assoc entries
                                          (.getName entry)
                                          (String. (.readAllBytes input) java.nio.charset.StandardCharsets/UTF_8)))
                                 entries))]
                 (assert (= {"nested/Mixed Case.txt" "extension source"} entries)
                         (pr-str entries)))))) [(.getAbsolutePath directory)]
        (is (zero? (process/await-exit-code process))))
      (finally
        (fs/delete-directory! directory)))))
