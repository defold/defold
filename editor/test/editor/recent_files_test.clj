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

(ns editor.recent-files-test
  (:require [clojure.test :refer :all]
            [editor.fs :as fs]
            [editor.prefs :as prefs]
            [editor.recent-files :as recent-files]
            [editor.resource :as resource]
            [editor.workspace :as workspace]))

(defn- make-test-prefs []
  (prefs/make :scopes {:project (fs/create-temp-file! "recent-files-test-project" ".editor_settings")}
              :schemas [:default]))

(deftest legacy-form-view-recent-files-test
  (let [prefs (make-test-prefs)
        legacy-prefs-data [["/game.project" :cljfx-form-view]]]
    (prefs/set! prefs [:workflow :recent-files] legacy-prefs-data)

    (with-redefs [resource/openable? (constantly true)
                  workspace/find-resource (fn [_basis _workspace _project-path] ::resource)
                  workspace/get-view-type (fn [_workspace view-type-id _evaluation-context]
                                            {:id view-type-id})]
      (is (= [[::resource {:id :form}]]
             (vec (recent-files/some-recent prefs ::workspace {:basis ::basis})))))

    (is (= legacy-prefs-data
           (prefs/get prefs [:workflow :recent-files])))

    (with-redefs [resource/openable? (constantly true)
                  resource/proj-path (constantly "/game.project")]
      (recent-files/add! prefs ::resource {:id :form}))

    (is (= [["/game.project" :form]]
           (prefs/get prefs [:workflow :recent-files])))))

(deftest legacy-form-view-open-tabs-test
  (let [prefs (make-test-prefs)
        legacy-prefs-data [[["/game.project" :cljfx-form-view]
                            ["/main/main.collection" :scene]]
                           [["/input/gamepads.gamepads" :cljfx-form-view]]]]
    (prefs/set! prefs [:workflow :open-tabs] legacy-prefs-data)

    (is (= [[["/game.project" :form]
             ["/main/main.collection" :scene]]
            [["/input/gamepads.gamepads" :form]]]
           (recent-files/get-open-tabs prefs)))

    (is (= legacy-prefs-data
           (prefs/get prefs [:workflow :open-tabs])))))
