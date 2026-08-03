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
            [editor.form :as form]
            [editor.protobuf :as protobuf]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [util.coll :as coll])
  (:import [com.dynamo.bob.font FontRenderer$Params]
           [com.dynamo.render.proto Font$FontDesc]
           [javax.vecmath Matrix4d]))

(defn- prop [node-id label]
  (get-in (g/node-value node-id :_properties) [:properties label :value]))

(defn- prop! [node-id label val]
  (g/transact {:undoable false}
    (g/set-property node-id label val)))

(deftest effective-sdf-scale-test
  (let [effective-sdf-scale (ns-resolve 'editor.font 'effective-sdf-scale)
        identity-transform (doto (Matrix4d.)
                             (.setIdentity))
        scaled-transform (doto (Matrix4d.)
                           (.setIdentity)
                           (.setScale 2.0))]
    (testing "uses the projected screen scale when available"
      (is (= 0.5 (effective-sdf-scale 0.25 identity-transform)))
      (is (= 2.0 (effective-sdf-scale 2.0 identity-transform))))
    (testing "falls back to the local transform scale when projection is invalid"
      (is (= 1.0 (effective-sdf-scale 0.0 identity-transform)))
      (is (= 2.0 (effective-sdf-scale 0.0 scaled-transform))))))

(deftest native-sdf-limit-test
  (let [native-sdf-limit (ns-resolve 'editor.font 'native-sdf-limit)]
    (is (= 0.75 (native-sdf-limit 3.0 0.0)))
    (is (< (native-sdf-limit 6.0 2.0)
           (native-sdf-limit 6.0 1.0)))))

(defn- font-map-uses-text-shaping? [font-node]
  (let [^FontRenderer$Params render-params (get-in (g/node-value font-node :font-map)
                                                    [:native-renderer-spec :render-params])]
    (.-useTextShaping render-params)))

(deftest app-manifest-layout-selection
  (test-util/with-loaded-project
    (let [game-project (test-util/resource-node project "/game.project")
          font-node (test-util/resource-node project "/editor1/test.font")
          app-manifest (test-util/resource-node project "/app_manifest/default.appmanifest")]
      (is (true? (g/node-value app-manifest :loaded)))
      (is (false? (font-map-uses-text-shaping? font-node)))
      (g/transact {:undoable false}
        (form/set-value (:form-ops (g/node-value game-project :form-data))
                        ["native_extension" "app_manifest"]
                        (g/node-value app-manifest :resource)))
      (is (false? (font-map-uses-text-shaping? font-node)))
      (g/transact {:undoable false}
        (g/set-property app-manifest :use-font-layout true))
      (is (true? (font-map-uses-text-shaping? font-node))))))

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
          [ew eh] (font/measure font-map no-break true (:cache-width font-map) 0 1)
          text-layout (font/layout-text font-map pre-text false 0 0.125 1)]
      (is (.contains pre-text " "))
      (is (not (.contains no-break " ")))
      (is (< w ew))
      (is (< eh h))
      (is (= 0.125 (:text-tracking text-layout))))))

(deftest build-targets-do-not-generate-font-map
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/fonts/score.font")]
      (g/clear-system-cache!)
      (with-redefs [font/compile-font (fn [& _]
                                       (throw (AssertionError. "font-map should not be generated for build-targets")))]
        (let [build-targets (g/node-value node-id :build-targets)]
          (when (is (not (g/error? build-targets)))
            (is (some? (coll/some #(get-in % [:user-data :pb-map :glyph-bank]) build-targets)))))))))

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
