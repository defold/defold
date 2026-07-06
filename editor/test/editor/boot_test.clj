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

(ns editor.boot-test
  (:require [clojure.data.json :as json]
            [clojure.test :refer :all]
            [editor.boot :as boot]
            [editor.fs :as fs]
            [editor.os :as os]
            [integration.test-util :as test-util]
            [util.path :as path]))

(set! *warn-on-reflection* true)

(deftest write-installations-json-test
  (test-util/with-temp-dir! dir
    (let [config-root (path/of dir "config")
          install-root (path/of dir "install")
          launcher-a (path/of install-root "DefoldA")
          launcher-b (path/of install-root "DefoldB")
          registry-path (path/of config-root "Defold" "installations.json")
          resources-a (path/of install-root "DefoldA.app" "Contents" "Resources")
          resources-b (path/of install-root "DefoldB.app" "Contents" "Resources")]
      (path/create-directories! resources-a)
      (path/create-directories! resources-b)
      (spit launcher-a "")
      (spit launcher-b "")

      (with-redefs [fs/evaluate-path (fn evaluate-path [raw-path]
                                       (case raw-path
                                         "~/Library/Preferences" (str config-root)
                                         nil))
                    os/os (constantly :macos)]
        (boot/write-installations-json! launcher-a resources-a)
        (boot/write-installations-json! launcher-b resources-b)
        (boot/write-installations-json! launcher-a resources-a)

        (let [installations (json/read-str (slurp registry-path) :key-fn keyword)]
          (is (= [(str (path/real launcher-a))
                  (str (path/real launcher-b))]
                 (mapv :launcherPath installations))))))))
