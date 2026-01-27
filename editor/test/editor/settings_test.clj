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
  (:require [clojure.test :refer :all]
            [editor.settings-core :as settings-core]))

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
