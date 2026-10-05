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

(ns editor.settings-test
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.settings :as settings]
            [editor.settings-core :as settings-core]
            [integration.test-util :as test-util]))

;; The retired project-wide font setting must explain the per-font replacement without blocking builds.
(deftest runtime-font-generation-setting-is-deprecated
  (with-open [reader (io/reader (io/resource "com/dynamo/bob/meta.properties"))]
    (let [meta-setting (settings-core/get-meta-setting (:settings (settings-core/load-meta-properties reader))
                                                     ["font" "runtime_generation"])]
      (is (nil? (settings/get-setting-error nil meta-setting :value)))
      (doseq [value [false true]]
        (let [error (settings/get-setting-error value meta-setting :value)]
          (is (= :warning (:severity error)))
          (is (string/includes? (test-util/localization (g/error-message error)) "runtime: true"))
          (is (nil? (settings/get-setting-build-error value meta-setting :build-targets))))))))

(deftest merge-meta-infos-prefers-known-settings
  (let [known-setting {:path ["section" "key"]
                       :type :string
                       :help "Known"}
        unknown-setting {:path ["section" "key"]
                         :type :string
                         :help "Unknown"
                         :unknown-setting true}]
    (is (= {:settings [known-setting]}
           (settings-core/merge-meta-infos
             {:settings [unknown-setting]}
             {:settings [known-setting]})))
    (is (= {:settings [known-setting]}
           (settings-core/merge-meta-infos
             {:settings [known-setting]}
             {:settings [unknown-setting]})))))

(deftest merge-meta-infos-preserves-default-setting-values
  (let [path ["section" "key"]
        meta-settings
        (:settings
          (reduce
            settings-core/merge-meta-infos
            [{:settings [{:path path
                          :type :string
                          :default "first"}]}
             {:settings [{:path path
                          :type :string
                          :default "second"}]}
             {:settings [{:path path
                          :type :string
                          :default "third"}]}]))]
    (is (= "first"
           (settings-core/get-default-setting meta-settings path)))
    (is (= ["first" "second" "third"]
           (settings-core/get-default-setting-values meta-settings path)))))

(deftest get-setting-or-default-preserves-present-nil-value
  (let [root (io/file ".")
        default-resource (test-util/make-fake-file-resource nil
                                                            (.getPath root)
                                                            (io/file root "default.resource")
                                                            (byte-array 0))
        meta-settings [{:path ["section" "key"]
                        :type :resource
                        :default default-resource}]]
    (is (nil? (settings-core/get-setting-or-default meta-settings
                                                    [{:path ["section" "key"]
                                                      :value nil}]
                                                    ["section" "key"])))
    (is (identical? default-resource
                    (settings-core/get-setting-or-default meta-settings [] ["section" "key"])))))
