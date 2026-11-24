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

(ns editor.collection-common-test
  (:require [clojure.test :refer :all]
            [editor.collection-common :as collection-common]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace])
  (:import [com.dynamo.gameobject.proto GameObject$PrototypeDesc]))

(set! *warn-on-reflection* true)

(deftest embedded-sanitizer-owner-resource-test
  (let [read-opts {:marker "sanitized"}
        owner-resource (Object.)
        resource-types
        {"test" (with-redefs [workspace/register-resource-type (fn [_workspace & {:as args}] args)]
                  (resource-node/register-ddf-resource-type nil
                    :ext "test"
                    :ddf-type GameObject$PrototypeDesc
                    :sanitize-fn (fn [actual-read-opts actual-owner-resource source-value]
                                   (is (identical? read-opts actual-read-opts))
                                   (is (identical? owner-resource actual-owner-resource))
                                   (assoc source-value :marker (:marker actual-read-opts)))))}
        collection-desc {:embedded-instances
                         [{:id "go"
                           :data "embedded_components { id: \"comp\" type: \"test\" data: \"\" }"}]}
        sanitized (collection-common/sanitize-collection-desc collection-desc resource-types read-opts owner-resource)]
    (is (= {:marker "sanitized"}
           (get-in sanitized [:embedded-instances 0 :data :embedded-components 0 :data])))))

(deftest sort-instance-descs-for-build-output-test
  (is (= [{:id "chest"
           :children ["left-shoulder"
                      "right-shoulder"]}
          {:id "left-shoulder"
           :children ["left-upper-arm"]}
          {:id "right-shoulder"
           :children ["right-upper-arm"]}
          {:id "left-upper-arm"
           :children ["left-elbow"]}
          {:id "right-upper-arm"
           :children ["right-elbow"]}
          {:id "left-elbow"
           :children ["left-lower-arm"]}
          {:id "right-elbow"
           :children ["right-lower-arm"]}
          {:id "left-lower-arm"
           :children ["left-hand"]}
          {:id "right-lower-arm"
           :children ["right-hand"]}
          {:id "left-hand"
           :children ["left-thumb"
                      "left-index-finger"
                      "left-middle-finger"
                      "left-ring-finger"
                      "left-pinkie-finger"]}
          {:id "right-hand"
           :children ["right-thumb"
                      "right-index-finger"
                      "right-middle-finger"
                      "right-ring-finger"
                      "right-pinkie-finger"]}
          {:id "left-index-finger"}
          {:id "left-middle-finger"}
          {:id "left-pinkie-finger"}
          {:id "left-ring-finger"}
          {:id "left-thumb"}
          {:id "right-index-finger"}
          {:id "right-middle-finger"}
          {:id "right-pinkie-finger"}
          {:id "right-ring-finger"}
          {:id "right-thumb"}]
         (#'collection-common/sort-instance-descs-for-build-output
           [{:id "right-thumb"}
            {:id "right-index-finger"}
            {:id "right-middle-finger"}
            {:id "right-ring-finger"}
            {:id "right-pinkie-finger"}
            {:id "right-hand"
             :children ["right-thumb"
                        "right-index-finger"
                        "right-middle-finger"
                        "right-ring-finger"
                        "right-pinkie-finger"]}
            {:id "right-lower-arm"
             :children ["right-hand"]}
            {:id "right-elbow"
             :children ["right-lower-arm"]}
            {:id "right-upper-arm"
             :children ["right-elbow"]}
            {:id "right-shoulder"
             :children ["right-upper-arm"]}
            {:id "left-thumb"}
            {:id "left-index-finger"}
            {:id "left-middle-finger"}
            {:id "left-ring-finger"}
            {:id "left-pinkie-finger"}
            {:id "left-hand"
             :children ["left-thumb"
                        "left-index-finger"
                        "left-middle-finger"
                        "left-ring-finger"
                        "left-pinkie-finger"]}
            {:id "left-lower-arm"
             :children ["left-hand"]}
            {:id "left-elbow"
             :children ["left-lower-arm"]}
            {:id "left-upper-arm"
             :children ["left-elbow"]}
            {:id "left-shoulder"
             :children ["left-upper-arm"]}
            {:id "chest"
             :children ["left-shoulder"
                        "right-shoulder"]}]))))