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

(ns integration.render-target-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.form :as form]
            [integration.test-util :as test-util]
            [util.coll :as coll]))

(defn- form-field [form-data path]
  (coll/first-where #(= path (:path %))
                    (eduction (mapcat :fields) (:sections form-data))))

(deftest cubemap-sample-count-validation
  (test-util/with-temp-project-content
    {"/cubemap.render_target"
     {:type :type-cubemap
      :sample-count 4
      :color-attachments [{:width 16
                           :height 16
                           :format :texture-format-rgba}]}}
    (let [node-id (test-util/resource-node project "/cubemap.render_target")]
      (is (true? (:disable (form-field (g/node-value node-id :form-data) [:sample-count]))))
      (is (g/error-fatal? (g/node-value node-id :build-errors)))

      (g/set-property! node-id :sample-count 1)
      (is (true? (:disable (form-field (g/node-value node-id :form-data) [:sample-count]))))
      (is (not (g/error? (g/node-value node-id :build-errors))))

      (g/set-property! node-id :type :type-2d)
      (is (false? (:disable (form-field (g/node-value node-id :form-data) [:sample-count])))))))

(deftest selecting-cubemap-resets-sample-count
  (test-util/with-temp-project-content
    {"/multisampled.render_target"
     {:type :type-2d
      :sample-count 4
      :color-attachments [{:width 16
                           :height 16
                           :format :texture-format-rgba}]}}
    (let [node-id (test-util/resource-node project "/multisampled.render_target")
          form-ops (:form-ops (g/node-value node-id :form-data))]
      (g/transact (form/set-value form-ops [:type] :type-cubemap))

      (is (= :type-cubemap (g/node-value node-id :type)))
      (is (= 1 (g/node-value node-id :sample-count)))
      (is (not (g/error? (g/node-value node-id :build-errors)))))))

(deftest unsupported-texture-type-validation
  (test-util/with-temp-project-content
    {"/unsupported.render_target"
     {:type :type-3d
      :color-attachments [{:width 16
                           :height 16
                           :format :texture-format-rgba}]}}
    (let [node-id (test-util/resource-node project "/unsupported.render_target")]
      (is (g/error-fatal? (g/node-value node-id :build-errors)))
      (is (g/error-fatal? (g/node-value node-id :build-targets))))))
