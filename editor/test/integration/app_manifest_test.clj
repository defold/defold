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

(ns integration.app-manifest-test
  (:require [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.app-manifest :as app-manifest]
            [editor.code.data :as data]
            [integration.test-util :as test-util]))

(deftest toggle-test
  (testing "contains toggles"
    (let [toggle (app-manifest/contains-toggle :common :libs "foo")]
      (testing "get"
        (is (true? (app-manifest/get-toggle-value {:platforms {:common {:context {:libs ["foo"]}}}} toggle)))
        (is (false? (app-manifest/get-toggle-value {:platforms {:common {:context {:libs []}}}} toggle)))
        (is (false? (app-manifest/get-toggle-value {:platforms {:common {:context {:libs "not-a-list"}}}} toggle)))
        (is (false? (app-manifest/get-toggle-value {:platforms {:common "not-a-map"}} toggle)))
        (is (false? (app-manifest/get-toggle-value ["not a map"] toggle))))
      (testing "set"
        (is (= {:platforms {:common {:context {:libs []}}}}
               (app-manifest/set-toggle-value
                 {:platforms {:common {:context {:libs ["foo"]}}}} toggle false)))
        (is (= {:platforms {:common {:context {:libs ["don't touch this" "foo"]}}}}
               (-> {:platforms {:common {:context {:libs ["foo" "don't touch this"]}}}}
                   (app-manifest/set-toggle-value toggle false)
                   (app-manifest/set-toggle-value toggle true))))
        (is (= {:platforms {:common {:context {:libs ["foo"]}}}}
               (app-manifest/set-toggle-value
                 {:platforms {:common {:context {:libs "not-a-coll"}}}} toggle true)))
        (is (= {:platforms {:common {:context {:libs []}}}}
               (app-manifest/set-toggle-value {:platforms {:common :invalid}} toggle false))))))
  (testing "boolean toggle"
    (testing "get"
      (testing "positive toggle (expects value to be true for toggle to be on)"
        (let [toggle (app-manifest/boolean-toggle :common :enabled true)]
          (is (true? (app-manifest/get-toggle-value {:platforms {:common {:context {:enabled true}}}} toggle)))
          (is (false? (app-manifest/get-toggle-value {:platforms {:common {:context {:enabled false}}}} toggle)))
          (is (false? (app-manifest/get-toggle-value {:platforms {:common {:context {:enabled "not-a-boolean"}}}} toggle)))
          (is (false? (app-manifest/get-toggle-value {:platforms {:common "not-a-map"}} toggle)))))
      (testing "negative toggle (expects value to be false for toggle to be on)"
        (let [toggle (app-manifest/boolean-toggle :common :disabled false)]
          (is (true? (app-manifest/get-toggle-value {:platforms {:common {:context {:disabled false}}}} toggle)))
          (is (false? (app-manifest/get-toggle-value {:platforms {:common {:context {:disabled true}}}} toggle)))
          (is (false? (app-manifest/get-toggle-value {:platforms {:common {:context {:disabled "not-a-boolean"}}}} toggle)))
          (is (false? (app-manifest/get-toggle-value {:platforms "not-a-map"} toggle))))))
    (testing "set"
      (testing "positive toggle"
        (let [toggle (app-manifest/boolean-toggle :common :enabled true)]
          (is (= {:platforms {:common {:context {:enabled true}}}}
                 (app-manifest/set-toggle-value {:platforms {:common {:context {:enabled false}}}} toggle true)))
          (is (= {:platforms {:common {:context {:enabled false}}}}
                 (app-manifest/set-toggle-value {:platforms {:common {:context {:enabled true}}}} toggle false)))
          (is (= {:platforms {:common {:context {:enabled true}}}}
                 (app-manifest/set-toggle-value {:platforms {:common "not-a-map"}} toggle true)))))
      (testing "negative toggle"
        (let [toggle (app-manifest/boolean-toggle :common :disabled false)]
          (is (= {:platforms {:common {:context {:disabled false}}}}
                 (app-manifest/set-toggle-value {:platforms {:common {:context {:disabled true}}}} toggle true)))
          (is (= {:platforms {:common {:context {:disabled true}}}}
                 (app-manifest/set-toggle-value {:platforms {:common {:context {:disabled false}}}} toggle false)))
          (is (= {:platforms {:common {:context {:disabled false}}}}
                 (app-manifest/set-toggle-value {:platforms {:common "not-a-map"}} toggle true))))))))

(deftest setting-test
  (testing "checkbox setting is boolean or nil (indeterminate) setting"
    (let [setting (app-manifest/make-check-box-setting
                    [(app-manifest/contains-toggle :desktop :libs "record")
                     (app-manifest/contains-toggle :mobile :libs "record")])]
      (testing "get"
        (is (true? (app-manifest/get-setting-value
                     {:platforms {:desktop {:context {:libs ["record"]}}
                                  :mobile {:context {:libs ["record"]}}}}
                     setting)))
        (is (false? (app-manifest/get-setting-value
                      {:platforms {:desktop {:context {:libs []}}
                                   :mobile {:context {:libs []}}}}
                      setting)))
        (is (nil? (app-manifest/get-setting-value
                    {:platforms {:desktop {:context {:libs ["record"]}}
                                 :mobile {:context {:libs []}}}}
                    setting))))
      (testing "set"
        (is (= {:platforms {:desktop {:context {:libs ["record"]}}
                            :mobile {:context {:libs ["record"]}}}}
               (app-manifest/set-setting-value :invalid setting true)))
        (is (= {:platforms {:desktop {:context {:libs ["record"]}}
                            :mobile {:context {:libs ["record"]}}}}
               (app-manifest/set-setting-value
                 {:platforms {:desktop {:context {:libs ["record"]}}
                              :mobile {:context {:libs []}}}}
                 setting
                 true)))
        (is (= {:platforms {:desktop {:context {:libs []}}
                            :mobile {:context {:libs []}}}}
               (app-manifest/set-setting-value :invalid setting false))))
      (testing "update"
        (is (= {:platforms {:desktop {:context {:libs ["record"]}}
                            :mobile {:context {:libs ["record"]}}}}
               (app-manifest/update-setting-value :invalid setting not)))
        (is (= {:platforms {:desktop {:context {:libs []}}
                            :mobile {:context {:libs []}}}}
               (app-manifest/update-setting-value
                 {:platforms {:desktop {:context {:libs ["record"]}}
                              :mobile {:context {:libs ["record"]}}}}
                 setting
                 not)))
        (testing "nil is treated as false"
          (is (= {:platforms {:desktop {:context {:libs []}}
                              :mobile {:context {:libs []}}}}
                 (app-manifest/update-setting-value
                   {:platforms {:desktop {:context {:libs ["record"]}}
                                :mobile {:context {:libs []}}}}
                   setting
                   identity)))))))
  (testing "choice setting is a enum of options or nil (indeterminate) setting"
    (let [setting (app-manifest/make-choice-setting
                    :all [(app-manifest/boolean-toggle :desktop :enabled true)
                          (app-manifest/boolean-toggle :mobile :enabled true)
                          (app-manifest/boolean-toggle :web :enabled true)]
                    :desktop [(app-manifest/boolean-toggle :desktop :enabled true)]
                    :mobile [(app-manifest/boolean-toggle :mobile :enabled true)]
                    :none)]
      (testing "get"
        (is (= :all (app-manifest/get-setting-value
                      {:platforms {:desktop {:context {:enabled true}}
                                   :web {:context {:enabled true}}
                                   :mobile {:context {:enabled true}}}}
                      setting)))
        (is (= :desktop (app-manifest/get-setting-value
                          {:platforms {:desktop {:context {:enabled true}}
                                       :mobile {:context {:enabled false}}}}
                          setting)))
        (is (= :mobile (app-manifest/get-setting-value
                         {:platforms {:desktop "nope"
                                      :mobile {:context {:enabled true}}}}
                         setting)))
        (is (= :none (app-manifest/get-setting-value
                       {:platforms {:desktop {:context {:enabled false}}
                                    :mobile {:context {:enabled false}}}}
                       setting)))
        (is (nil? (app-manifest/get-setting-value
                    {:platforms {:web {:context {:enabled true}}}}
                    setting))))
      (testing "set"
        (is (= {:platforms {:desktop {:context {:enabled true}}
                            :mobile {:context {:enabled true}}
                            :web {:context {:enabled true}}}}
               (app-manifest/set-setting-value {} setting :all)))
        (is (= :desktop (-> {}
                            (app-manifest/set-setting-value setting :all)
                            (app-manifest/set-setting-value setting :desktop)
                            (app-manifest/get-setting-value setting))))
        (is (= :none (-> {}
                         (app-manifest/set-setting-value setting :all)
                         (app-manifest/set-setting-value setting :none)
                         (app-manifest/get-setting-value setting)))))
      (testing "update"
        (let [update-fn {:none :all
                         :all :none
                         :desktop :mobile
                         :mobile :desktop}]
          (doseq [v [:none :all :desktop :mobile]]
            (is (= (update-fn v)
                   (-> {}
                       (app-manifest/set-setting-value setting v)
                       (app-manifest/update-setting-value setting update-fn)
                       (app-manifest/get-setting-value setting))))))))))

(deftest manifestation-compatibility-test
  (test-util/with-loaded-project
    (testing "/app_manifest/default.appmanifest"
      (let [manifest (test-util/resource-node project "/app_manifest/default.appmanifest")]
        (is (= :legacy (g/node-value manifest :physics-2d)))
        (is (= true (g/node-value manifest :physics-3d)))
        (is (= false (g/node-value manifest :exclude-record)))
        (is (= :debug-only (g/node-value manifest :profiler)))
        (is (= false (g/node-value manifest :exclude-sound)))
        (is (= false (g/node-value manifest :exclude-input)))
        (is (= false (g/node-value manifest :exclude-liveupdate)))
        (is (= false (g/node-value manifest :exclude-basis-transcoder)))
        (is (= false (g/node-value manifest :use-android-support-lib)))
        (is (= :open-gl (g/node-value manifest :graphics)))
        (is (= :vulkan (g/node-value manifest :graphics-osx)))
        (is (= :web-gl (g/node-value manifest :graphics-web)))))
    (testing "/app_manifest/exclude_physics_2d.appmanifest"
      (let [manifest (test-util/resource-node project "/app_manifest/exclude_physics_2d.appmanifest")]
        (is (= :none (g/node-value manifest :physics-2d)))
        (is (= true (g/node-value manifest :physics-3d)))
        (is (= false (g/node-value manifest :exclude-record)))
        (is (= :debug-only (g/node-value manifest :profiler)))
        (is (= false (g/node-value manifest :exclude-sound)))
        (is (= false (g/node-value manifest :exclude-input)))
        (is (= false (g/node-value manifest :exclude-liveupdate)))
        (is (= false (g/node-value manifest :exclude-basis-transcoder)))
        (is (= false (g/node-value manifest :use-android-support-lib)))
        (is (= :open-gl (g/node-value manifest :graphics)))
        (is (= :vulkan (g/node-value manifest :graphics-osx)))
        (is (= :web-gl (g/node-value manifest :graphics-web)))))
    (testing "/app_manifest/exclude_physics_3d.appmanifest"
      (let [manifest (test-util/resource-node project "/app_manifest/exclude_physics_3d.appmanifest")]
        (is (= :legacy (g/node-value manifest :physics-2d)))
        (is (= false (g/node-value manifest :physics-3d)))
        (is (= false (g/node-value manifest :exclude-record)))
        (is (= :debug-only (g/node-value manifest :profiler)))
        (is (= false (g/node-value manifest :exclude-sound)))
        (is (= false (g/node-value manifest :exclude-input)))
        (is (= false (g/node-value manifest :exclude-liveupdate)))
        (is (= false (g/node-value manifest :exclude-basis-transcoder)))
        (is (= false (g/node-value manifest :use-android-support-lib)))
        (is (= :open-gl (g/node-value manifest :graphics)))
        (is (= :vulkan (g/node-value manifest :graphics-osx)))
        (is (= :web-gl (g/node-value manifest :graphics-web)))))
    (testing "/app_manifest/exclude_physics.appmanifest"
      (let [manifest (test-util/resource-node project "/app_manifest/exclude_physics.appmanifest")]
        (is (= :none (g/node-value manifest :physics-2d)))
        (is (= false (g/node-value manifest :physics-3d)))
        (is (= false (g/node-value manifest :exclude-record)))
        (is (= :debug-only (g/node-value manifest :profiler)))
        (is (= false (g/node-value manifest :exclude-sound)))
        (is (= false (g/node-value manifest :exclude-input)))
        (is (= false (g/node-value manifest :exclude-liveupdate)))
        (is (= false (g/node-value manifest :exclude-basis-transcoder)))
        (is (= false (g/node-value manifest :use-android-support-lib)))
        (is (= :open-gl (g/node-value manifest :graphics)))
        (is (= :vulkan (g/node-value manifest :graphics-osx)))
        (is (= :web-gl (g/node-value manifest :graphics-web)))))
    (testing "/app_manifest/physics_2d_box2dv3.appmanifest"
      (let [manifest (test-util/resource-node project "/app_manifest/physics_2d_box2dv3.appmanifest")]
        (is (= :v3 (g/node-value manifest :physics-2d)))
        (is (= false (g/node-value manifest :physics-3d)))
        (is (= false (g/node-value manifest :exclude-record)))
        (is (= :debug-only (g/node-value manifest :profiler)))
        (is (= false (g/node-value manifest :exclude-sound)))
        (is (= false (g/node-value manifest :exclude-input)))
        (is (= false (g/node-value manifest :exclude-liveupdate)))
        (is (= false (g/node-value manifest :exclude-basis-transcoder)))
        (is (= false (g/node-value manifest :use-android-support-lib)))
        (is (= :open-gl (g/node-value manifest :graphics)))
        (is (= :vulkan (g/node-value manifest :graphics-osx)))
        (is (= :web-gl (g/node-value manifest :graphics-web)))))
    (testing "/app_manifest/physics_box2dv3_3d.appmanifest"
      (let [manifest (test-util/resource-node project "/app_manifest/physics_box2dv3_3d.appmanifest")]
        (is (= :v3 (g/node-value manifest :physics-2d)))
        (is (= true (g/node-value manifest :physics-3d)))
        (is (= false (g/node-value manifest :exclude-record)))
        (is (= :debug-only (g/node-value manifest :profiler)))
        (is (= false (g/node-value manifest :exclude-sound)))
        (is (= false (g/node-value manifest :exclude-input)))
        (is (= false (g/node-value manifest :exclude-liveupdate)))
        (is (= false (g/node-value manifest :exclude-basis-transcoder)))
        (is (= false (g/node-value manifest :use-android-support-lib)))
        (is (= :open-gl (g/node-value manifest :graphics)))
        (is (= :vulkan (g/node-value manifest :graphics-osx)))
        (is (= :web-gl (g/node-value manifest :graphics-web)))))
    (testing "/app_manifest/exclude_many.appmanifest"
      (let [manifest (test-util/resource-node project "/app_manifest/exclude_many.appmanifest")]
        (is (= :legacy (g/node-value manifest :physics-2d)))
        (is (= true (g/node-value manifest :physics-3d)))
        (is (= true (g/node-value manifest :exclude-record)))
        (is (= :none (g/node-value manifest :profiler)))
        (is (= true (g/node-value manifest :exclude-sound)))
        (is (= true (g/node-value manifest :exclude-input)))
        (is (= true (g/node-value manifest :exclude-liveupdate)))
        (is (= true (g/node-value manifest :exclude-basis-transcoder)))
        (is (= false (g/node-value manifest :use-android-support-lib)))
        (is (= :open-gl (g/node-value manifest :graphics)))
        (is (= :vulkan (g/node-value manifest :graphics-osx)))
        (is (= :web-gl (g/node-value manifest :graphics-web)))))
    (testing "/app_manifest/vulkan.appmanifest"
      (let [manifest (test-util/resource-node project "/app_manifest/vulkan.appmanifest")]
        (is (= :legacy (g/node-value manifest :physics-2d)))
        (is (= true (g/node-value manifest :physics-3d)))
        (is (= false (g/node-value manifest :exclude-record)))
        (is (= :debug-only (g/node-value manifest :profiler)))
        (is (= false (g/node-value manifest :exclude-sound)))
        (is (= false (g/node-value manifest :exclude-input)))
        (is (= false (g/node-value manifest :exclude-liveupdate)))
        (is (= false (g/node-value manifest :exclude-basis-transcoder)))
        (is (= false (g/node-value manifest :use-android-support-lib)))
        (is (= :vulkan (g/node-value manifest :graphics)))
        (is (= :vulkan (g/node-value manifest :graphics-osx)))
        (is (= :web-gl (g/node-value manifest :graphics-web)))))
    (testing "/app_manifest/vulkan_and_opengl.appmanifest"
      (let [manifest (test-util/resource-node project "/app_manifest/vulkan_and_opengl.appmanifest")]
        (is (= :legacy (g/node-value manifest :physics-2d)))
        (is (= true (g/node-value manifest :physics-3d)))
        (is (= false (g/node-value manifest :exclude-record)))
        (is (= :debug-only (g/node-value manifest :profiler)))
        (is (= false (g/node-value manifest :exclude-sound)))
        (is (= false (g/node-value manifest :exclude-input)))
        (is (= false (g/node-value manifest :exclude-liveupdate)))
        (is (= false (g/node-value manifest :exclude-basis-transcoder)))
        (is (= false (g/node-value manifest :use-android-support-lib)))
        (is (= :both (g/node-value manifest :graphics)))
        (is (= :vulkan (g/node-value manifest :graphics-osx)))
        (is (= :web-gl (g/node-value manifest :graphics-web)))))
    (testing "/app_manifest/opengl_osx.appmanifest"
      (let [manifest (test-util/resource-node project "/app_manifest/opengl_osx.appmanifest")]
        (is (= :legacy (g/node-value manifest :physics-2d)))
        (is (= true (g/node-value manifest :physics-3d)))
        (is (= false (g/node-value manifest :exclude-record)))
        (is (= :debug-only (g/node-value manifest :profiler)))
        (is (= false (g/node-value manifest :exclude-sound)))
        (is (= false (g/node-value manifest :exclude-input)))
        (is (= false (g/node-value manifest :exclude-liveupdate)))
        (is (= false (g/node-value manifest :exclude-basis-transcoder)))
        (is (= false (g/node-value manifest :use-android-support-lib)))
        (is (= :open-gl (g/node-value manifest :graphics)))
        (is (= :open-gl (g/node-value manifest :graphics-osx)))
        (is (= :web-gl (g/node-value manifest :graphics-web)))))
    (testing "/app_manifest/vulkan_and_opengl_osx.appmanifest"
      (let [manifest (test-util/resource-node project "/app_manifest/vulkan_and_opengl_osx.appmanifest")]
        (is (= :legacy (g/node-value manifest :physics-2d)))
        (is (= true (g/node-value manifest :physics-3d)))
        (is (= false (g/node-value manifest :exclude-record)))
        (is (= :debug-only (g/node-value manifest :profiler)))
        (is (= false (g/node-value manifest :exclude-sound)))
        (is (= false (g/node-value manifest :exclude-input)))
        (is (= false (g/node-value manifest :exclude-liveupdate)))
        (is (= false (g/node-value manifest :exclude-basis-transcoder)))
        (is (= false (g/node-value manifest :use-android-support-lib)))
        (is (= :open-gl (g/node-value manifest :graphics)))
        (is (= :both (g/node-value manifest :graphics-osx)))
        (is (= :web-gl (g/node-value manifest :graphics-web)))))
    (testing "/app_manifest/webgpu.appmanifest"
      (let [manifest (test-util/resource-node project "/app_manifest/webgpu.appmanifest")]
        (is (= :legacy (g/node-value manifest :physics-2d)))
        (is (= true (g/node-value manifest :physics-3d)))
        (is (= false (g/node-value manifest :exclude-record)))
        (is (= :debug-only (g/node-value manifest :profiler)))
        (is (= false (g/node-value manifest :exclude-sound)))
        (is (= false (g/node-value manifest :exclude-input)))
        (is (= false (g/node-value manifest :exclude-liveupdate)))
        (is (= false (g/node-value manifest :exclude-basis-transcoder)))
        (is (= false (g/node-value manifest :use-android-support-lib)))
        (is (= :open-gl (g/node-value manifest :graphics)))
        (is (= :vulkan (g/node-value manifest :graphics-osx)))
        (is (= :web-gpu (g/node-value manifest :graphics-web)))))
    (testing "/app_manifest/webgpu_and_webgl.appmanifest"
      (let [manifest (test-util/resource-node project "/app_manifest/webgpu_and_webgl.appmanifest")]
        (is (= :legacy (g/node-value manifest :physics-2d)))
        (is (= true (g/node-value manifest :physics-3d)))
        (is (= false (g/node-value manifest :exclude-record)))
        (is (= :debug-only (g/node-value manifest :profiler)))
        (is (= false (g/node-value manifest :exclude-sound)))
        (is (= false (g/node-value manifest :exclude-input)))
        (is (= false (g/node-value manifest :exclude-liveupdate)))
        (is (= false (g/node-value manifest :exclude-basis-transcoder)))
        (is (= false (g/node-value manifest :use-android-support-lib)))
        (is (= :open-gl (g/node-value manifest :graphics)))
        (is (= :vulkan (g/node-value manifest :graphics-osx)))
        (is (= :both (g/node-value manifest :graphics-web)))))
    (testing "/app_manifest/android_support.appmanifest"
      (let [manifest (test-util/resource-node project "/app_manifest/android_support.appmanifest")]
        (is (= :legacy (g/node-value manifest :physics-2d)))
        (is (= true (g/node-value manifest :physics-3d)))
        (is (= false (g/node-value manifest :exclude-record)))
        (is (= :debug-only (g/node-value manifest :profiler)))
        (is (= false (g/node-value manifest :exclude-sound)))
        (is (= false (g/node-value manifest :exclude-input)))
        (is (= false (g/node-value manifest :exclude-liveupdate)))
        (is (= false (g/node-value manifest :exclude-basis-transcoder)))
        (is (= true (g/node-value manifest :use-android-support-lib)))
        (is (= :open-gl (g/node-value manifest :graphics)))
        (is (= :vulkan (g/node-value manifest :graphics-osx)))
        (is (= :web-gl (g/node-value manifest :graphics-web)))))))

(deftest modification-test
  (test-util/with-loaded-project
    (let [manifest (test-util/resource-node project "/app_manifest/default.appmanifest")
          text #(slurp (data/lines-reader (g/node-value manifest :modified-lines)))]
      (is (false? (string/includes? (text) "record_null")))
      (is (false? (g/node-value manifest :exclude-record)))
      (g/set-property! manifest :exclude-record true)
      (is (string/includes? (text) "record_null"))
      (is (true? (g/node-value manifest :exclude-record)))
      (g/set-property! manifest :exclude-record false)
      (is (false? (string/includes? (text) "record_null")))
      (is (false? (g/node-value manifest :exclude-record))))))
