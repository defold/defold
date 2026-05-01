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

(ns integration.font-test
  (:require [clojure.string :as s]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.font :as font]
            [editor.protobuf :as protobuf]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util])
  (:import [com.dynamo.render.proto Font$FontDesc]))

(defn- prop [node-id label]
  (get-in (g/node-value node-id :_properties) [:properties label :value]))

(defn- prop! [node-id label val]
  (g/transact (g/set-property node-id label val)))

(deftest load-material-render-data
  (test-util/with-loaded-project
    (let [node-id   (test-util/resource-node project "/fonts/score.font")
          scene (g/node-value node-id :scene)]
      (is (not (nil? scene))))))

(deftest text-measure
  (test-util/with-loaded-project
    (let [node-id   (test-util/resource-node project "/fonts/score.font")
          font-map (g/node-value node-id :font-map)]
      (let [[w h] (font/measure font-map "test")]
        (is (> w 0))
        (is (> h 0))
        (let [[w' h'] (font/measure font-map "test\ntest")]
          (is (= w' w))
          (is (> h' h))
          (let [[w'' h''] (font/measure font-map "test\u200Btest" true w 0 1)]
            (is (= w'' w'))
            (is (= h'' h')))
          (let [[w'' h''] (font/measure font-map "test test test" true w 0 1)]
            (is (= w'' w'))
            (is (> h'' h')))
          (let [[w'' h''] (font/measure font-map "test test test" true w 0.1 1.1)]
            (is (> w'' w'))
            (is (> h'' h'))))))))

(deftest text-splitting
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/fonts/score.font")
          font-map (g/node-value node-id :font-map)
          {hello-width :width :keys [lines]} (font/layout-text font-map "hello" false 0 0 0)]
      (is (= ["hello"] lines))
      (testing "If the line is too long and does not have spaces, we don't wrap"
        (is (= ["hellohello"] (:lines (font/layout-text font-map "hellohello" true hello-width 0 0)))))
      (testing "If the line is too long and has spaces, we wrap"
        (is (= ["hello" "hello"] (:lines (font/layout-text font-map "hello hello" true hello-width 0 0)))))
      (testing "The whitespace at the beginning and end of lines is trimmed"
        (is (= ["hello" "hello"] (:lines (font/layout-text font-map "  \u200B  hello    \u200Bhello    " true hello-width 0 0)))))
      (testing "Tailing empty lines are trimmed"
        (is (= ["hello" "hello"] (:lines (font/layout-text font-map "hello hello\n \n   \n\n  \n  \n " true hello-width 0 0)))))
      (testing "We always split on \r?\n"
        (is (= ["hello" "hello" "hello"] (:lines (font/layout-text font-map "hello\r\nhello\nhello" true hello-width 0 0))))))))

(deftest preview-text
  (test-util/with-loaded-project
    (let [node-id   (test-util/resource-node project "/fonts/score.font")
          font-map (g/node-value node-id :font-map)
          pre-text (g/node-value node-id :preview-text)
          no-break (s/replace pre-text " " "")
          [w h] (font/measure font-map pre-text true (:cache-width font-map) 0 1)
          [ew eh] (font/measure font-map no-break true (:cache-width font-map) 0 1)]
      (is (.contains pre-text " "))
      (is (not (.contains no-break " ")))
      (is (< w ew))
      (is (< eh h)))))

(deftest validation
  (test-util/with-loaded-project
    (let [node-id   (test-util/resource-node project "/fonts/score.font")]
      (is (nil? (test-util/prop-error node-id :font)))
      (is (nil? (test-util/prop-error node-id :material)))
      (test-util/with-prop [node-id :font nil]
        (is (g/error-fatal? (test-util/prop-error node-id :font))))
      (test-util/with-prop [node-id :font (workspace/resolve-workspace-resource workspace "/not_found.ttf")]
        (is (g/error-fatal? (test-util/prop-error node-id :font))))
      (test-util/with-prop [node-id :material nil]
        (is (g/error-fatal? (test-util/prop-error node-id :material))))
      (test-util/with-prop [node-id :material (workspace/resolve-workspace-resource workspace "/not_found.material")]
        (is (g/error-fatal? (test-util/prop-error node-id :material))))
      (doseq [p [:size :alpha :outline-alpha :outline-width :shadow-alpha :shadow-blur :cache-width :cache-height]]
        (test-util/with-prop [node-id p -1]
          (is (g/error-fatal? (test-util/prop-error node-id p))))))))

(defn pb-property [node-id property]
  (if-some [pb-value ((g/valid-node-value node-id :save-value) property)]
    pb-value
    (protobuf/default Font$FontDesc property)))

(deftest antialias
  (test-util/with-loaded-project
    (let [score (test-util/resource-node project "/fonts/score.font")
          score-not-antialias (test-util/resource-node project "/fonts/score_not_antialias.font")
          score-no-antialias (test-util/resource-node project "/fonts/score_no_antialias.font")]

      (is (= true (g/node-value score :antialias)))
      (is (= 1 (pb-property score :antialias)))

      (g/set-property! score :antialias false)
      (is (= 0 (pb-property score :antialias)))

      (is (= false (g/node-value score-not-antialias :antialias)))
      (is (= 0 (pb-property score-not-antialias :antialias)))

      (g/set-property! score-not-antialias :antialias true)
      (is (= 1 (pb-property score-not-antialias :antialias)))

      (is (= true (g/node-value score-no-antialias :antialias))) ; font_ddf defaults antialias to 1 = true
      (is (= 1 (pb-property score-no-antialias :antialias)))

      (g/set-property! score-no-antialias :antialias false)
      (is (= 0 (pb-property score-no-antialias :antialias)))

      (g/set-property! score-no-antialias :antialias true)
      (is (= 1 (pb-property score-no-antialias :antialias))))))

(deftest font-scene
  (test-util/with-loaded-project
    (let [node-id (project/get-resource-node project "/fonts/logo.font")]
      (test-util/test-uses-assigned-material workspace project node-id
                                             :material
                                             [:renderable :user-data :shader]
                                             [:renderable :user-data :texture]))))
