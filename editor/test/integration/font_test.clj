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
            [editor.app-view :as app-view]
            [editor.build :as build]
            [editor.code.data :as code.data]
            [editor.defold-project :as project]
            [editor.font :as font]
            [editor.form :as form]
            [editor.game-project :as game-project]
            [editor.gl.pass :as pass]
            [editor.input :as input]
            [editor.protobuf :as protobuf]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [util.coll :as coll])
  (:import [ch.qos.logback.classic Logger]
           [ch.qos.logback.core.read ListAppender]
           [com.dynamo.bob.font FontRenderer$GlyphBank FontRenderer$Params FontRenderer$Properties FontRenderer$Style FontStyles]
           [com.dynamo.render.proto Font$FontDesc]
           [javax.vecmath Matrix4d]
           [org.slf4j LoggerFactory]))

(defn- prop [node-id label]
  (get-in (g/node-value node-id :_properties) [:properties label :value]))

(defn- prop! [node-id label val]
  (g/transact {:undoable false}
    (g/set-property node-id label val)))

(deftest preview-text-respects-glyph-cache-capacity
  (let [font-map {:cache-width 20
                  :cache-height 20
                  :cache-cell-width 10
                  :cache-cell-height 10
                  :glyphs (mapv (fn [character]
                                  {:character character
                                   :width 5
                                   :advance 10.0})
                                (range (int \A) (inc (int \F))))}]
    (is (= "AB\nCD" ((ns-resolve 'editor.font 'produce-preview-text)
                      {:font-map font-map})))))

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

(deftest native-entry-preserves-rich-effect-alpha
  (let [make-native-entry-state (ns-resolve 'editor.font 'make-native-entry-state)
        identity-transform (doto (Matrix4d.)
                             (.setIdentity))
        font-map {:alpha 1.0
                  :outline-alpha 0.0
                  :shadow-alpha 0.0}
        entry {:color [1.0 1.0 1.0 1.0]
               :outline [0.0 0.0 0.0 0.5]
               :shadow [0.0 0.0 0.0 0.5]
               :text-layout {:layout-width 100.0
                             :line-break false
                             :max-ascent 20.0
                             :native-text "A"
                             :text-leading 1.0
                             :text-tracking 0.0
                             :use-rich-text true}
               :world-transform identity-transform}
        rich-entry-state (make-native-entry-state font-map entry)
        legacy-entry-state (make-native-entry-state font-map (assoc-in entry [:text-layout :use-rich-text] false))
        ^FontRenderer$Properties rich-properties (:properties rich-entry-state)
        ^FontRenderer$Properties legacy-properties (:properties legacy-entry-state)]
    (is (= 0.5 (double (aget ^floats (.-outlineColor rich-properties) 3))))
    (is (= 0.5 (double (aget ^floats (.-shadowColor rich-properties) 3))))
    (is (zero? (double (.-baseShadowAlpha rich-properties))))
    (is (zero? (double (aget ^floats (.-outlineColor legacy-properties) 3))))))

(deftest rich-text-layer-capability-validation
  (let [bm-font-error (font/markup-error 0 :text
                                         {:rich-text-render-kind :bitmap
                                          :use-rich-text true}
                                         "<shadow x=1>Text</shadow>")
        bm-font-attribute-text-error (font/markup-error 0 :text
                                                       {:rich-text-render-kind :bitmap
                                                        :use-rich-text true}
                                                       "<link id=\"<shadow blur=2>\">Text</link>")
        static-invalid-markup-error (font/markup-error 0 :text
                                                       {:rich-text-render-kind :distance-field
                                                        :use-rich-text true}
                                                       "<outline>Text</shadow>")
        bitmap-outline-error (font/markup-error 0 :text
                                                {:outline-width 3.0
                                                 :rich-text-render-kind :defold
                                                 :use-rich-text true}
                                                "<outline size='2'>Text</outline>")
        bitmap-hidden-outline-shadow-error (font/markup-error 0 :text
                                                               {:outline-alpha 1.0
                                                                :outline-width 3.0
                                                                :rich-text-render-kind :defold
                                                                :rich-text-shadow-blur-capacity 4.0
                                                                :shadow-alpha 1.0
                                                                :use-rich-text true}
                                                               "<shadow x='2'>Text</shadow>")
        bitmap-nested-outline-shadow-error (font/markup-error 0 :text
                                                               {:outline-alpha 1.0
                                                                :outline-width 3.0
                                                                :rich-text-render-kind :defold
                                                                :rich-text-shadow-blur-capacity 4.0
                                                                :shadow-alpha 1.0
                                                                :use-rich-text true}
                                                               "<outline><shadow x='2'>Text</shadow></outline>")
        bitmap-containing-outline-shadow-error (font/markup-error 0 :text
                                                                   {:outline-alpha 1.0
                                                                    :outline-width 3.0
                                                                    :rich-text-render-kind :defold
                                                                    :rich-text-shadow-blur-capacity 4.0
                                                                    :shadow-alpha 1.0
                                                                    :use-rich-text true}
                                                                   "<shadow x='2'><outline>Text</outline></shadow>")
        bitmap-partially-outlined-shadow-error (font/markup-error 0 :text
                                                                   {:outline-alpha 1.0
                                                                    :outline-width 3.0
                                                                    :rich-text-render-kind :defold
                                                                    :rich-text-shadow-blur-capacity 4.0
                                                                    :shadow-alpha 1.0
                                                                    :use-rich-text true}
                                                                   "<shadow x='2'>A<outline>B</outline>C</shadow>")
        bitmap-fully-outlined-shadow-error (font/markup-error 0 :text
                                                               {:outline-alpha 1.0
                                                                :outline-width 3.0
                                                                :rich-text-render-kind :defold
                                                                :rich-text-shadow-blur-capacity 4.0
                                                                :shadow-alpha 1.0
                                                                :use-rich-text true}
                                                               "<shadow x='2'><outline>A</outline><outline>B</outline></shadow>")
        bitmap-disabled-inner-outline-shadow-error (font/markup-error 0 :text
                                                                        {:outline-alpha 1.0
                                                                         :outline-width 3.0
                                                                         :rich-text-render-kind :defold
                                                                         :rich-text-shadow-blur-capacity 4.0
                                                                         :shadow-alpha 1.0
                                                                         :use-rich-text true}
                                                                        "<outline><shadow x='2'>A<outline size='0'>B</outline></shadow></outline>")
        bitmap-inherited-zero-blur-shadow-error (font/markup-error 0 :text
                                                                   {:outline-alpha 1.0
                                                                    :outline-width 3.0
                                                                    :rich-text-render-kind :defold
                                                                    :rich-text-shadow-blur-capacity 4.0
                                                                    :shadow-alpha 1.0
                                                                    :use-rich-text true}
                                                                   "<shadow blur='0'><shadow x='2'>A</shadow></shadow>")
        bitmap-overridden-zero-blur-shadow-error (font/markup-error 0 :text
                                                                    {:outline-alpha 1.0
                                                                     :outline-width 3.0
                                                                     :rich-text-render-kind :defold
                                                                     :rich-text-shadow-blur-capacity 4.0
                                                                     :shadow-alpha 1.0
                                                                     :use-rich-text true}
                                                                    "<shadow x='2'><shadow blur='0'>A</shadow></shadow>")
        unreserved-outline-error (font/markup-error 0 :text
                                                    {:outline-width 0.0
                                                     :rich-text-render-kind :distance-field
                                                     :use-rich-text true}
                                                    "<outline color=#FFFFFFFF>Text</outline>")
        disabled-outline-error (font/markup-error 0 :text
                                                  {:outline-width 0.0
                                                   :rich-text-render-kind :distance-field
                                                   :use-rich-text true}
                                                  "<outline size='0'>Text</outline>")
        mixed-outline-error (font/markup-error 0 :text
                                               {:outline-width 0.0
                                                :rich-text-render-kind :distance-field
                                                :use-rich-text true}
                                               "<outline color=#FFFFFFFF>A</outline><outline size='0'>B</outline>")
        distance-field-outline-error (font/markup-error 0 :text
                                                        {:outline-width 2.0
                                                         :rich-text-render-kind :distance-field
                                                         :use-rich-text true}
                                                        "<outline size='4'>Text</outline>")
        scaled-up-distance-field-outline-error (font/markup-error 0 :text
                                                                  {:outline-width 2.0
                                                                   :rich-text-render-kind :distance-field
                                                                   :size 16
                                                                   :use-rich-text true}
                                                                  "<size=200%><outline size='4'>Text</outline></size>")
        scaled-down-distance-field-outline-error (font/markup-error 0 :text
                                                                    {:outline-width 2.0
                                                                     :rich-text-render-kind :distance-field
                                                                     :size 16
                                                                     :use-rich-text true}
                                                                    "<size=50%><outline size='2'>Text</outline></size>")
        unreserved-blur-error (font/markup-error 0 :text
                                                 {:rich-text-render-kind :distance-field
                                                  :rich-text-shadow-blur-capacity 0.0
                                                  :use-rich-text true}
                                                 "<shadow blur='2'>Text</shadow>")
        overridden-unreserved-blur-error (font/markup-error 0 :text
                                                             {:rich-text-render-kind :distance-field
                                                              :rich-text-shadow-blur-capacity 0.0
                                                              :use-rich-text true}
                                                             "<shadow blur='2'><shadow blur='0'>Text</shadow></shadow>")]
    (is (g/error-warning? bm-font-error))
    (is (s/includes? (test-util/localization (g/error-message bm-font-error)) "not supported by BMFont"))
    (is (nil? bm-font-attribute-text-error))
    (is (g/error-warning? static-invalid-markup-error))
    (is (s/includes? (test-util/localization (g/error-message static-invalid-markup-error)) "mismatched closing tag"))
    (is (g/error-warning? bitmap-outline-error))
    (is (s/includes? (test-util/localization (g/error-message bitmap-outline-error)) "fixed for bitmap fonts"))
    (is (g/error-warning? bitmap-hidden-outline-shadow-error))
    (is (s/includes? (test-util/localization (g/error-message bitmap-hidden-outline-shadow-error)) "spans without an outline tag render crisp"))
    (is (nil? bitmap-nested-outline-shadow-error))
    (is (nil? bitmap-containing-outline-shadow-error))
    (is (g/error-warning? bitmap-partially-outlined-shadow-error))
    (is (s/includes? (test-util/localization (g/error-message bitmap-partially-outlined-shadow-error)) "spans without an outline tag render crisp"))
    (is (nil? bitmap-fully-outlined-shadow-error))
    (is (g/error-warning? bitmap-disabled-inner-outline-shadow-error))
    (is (s/includes? (test-util/localization (g/error-message bitmap-disabled-inner-outline-shadow-error)) "spans without an outline tag render crisp"))
    (is (nil? bitmap-inherited-zero-blur-shadow-error))
    (is (nil? bitmap-overridden-zero-blur-shadow-error))
    (is (g/error-warning? unreserved-outline-error))
    (is (s/includes? (test-util/localization (g/error-message unreserved-outline-error)) "will not be rendered"))
    (is (nil? disabled-outline-error))
    (is (g/error-warning? mixed-outline-error))
    (is (s/includes? (test-util/localization (g/error-message mixed-outline-error)) "will not be rendered"))
    (is (g/error-warning? distance-field-outline-error))
    (is (s/includes? (test-util/localization (g/error-message distance-field-outline-error)) "will be clamped"))
    (is (nil? scaled-up-distance-field-outline-error))
    (is (g/error-warning? scaled-down-distance-field-outline-error))
    (is (s/includes? (test-util/localization (g/error-message scaled-down-distance-field-outline-error)) "will be clamped"))
    (is (g/error-warning? unreserved-blur-error))
    (is (s/includes? (test-util/localization (g/error-message unreserved-blur-error)) "no reserved distance-field data"))
    (is (nil? overridden-unreserved-blur-error))))

(deftest native-sdf-limit-test
  (let [native-sdf-limit (ns-resolve 'editor.font 'native-sdf-limit)]
    (is (= 0.75 (native-sdf-limit 3.0 0.0)))
    (is (< (native-sdf-limit 6.0 2.0)
           (native-sdf-limit 6.0 1.0)))))

(deftest static-native-preview-character-set
  (test-util/with-loaded-project
    (let [font-node (test-util/resource-node project "/editor1/test.font")]
      (g/transact {:undoable false}
        [(g/set-property font-node :all-chars false)
         (g/set-property font-node :characters "A")])
      (let [font-map (g/node-value font-node :font-map)
            restricted-layout (font/layout-text font-map "A B" false 0 0 1)
            expected-layout (font/layout-text font-map "A" false 0 0 1)]
        (is (= "A B" (:text restricted-layout)))
        (is (= "A" (:native-text restricted-layout)))
        (is (= (:width expected-layout) (:width restricted-layout)))
        (is (= (:height expected-layout) (:height restricted-layout)))))))

(deftest static-native-preview-filters-rich-text-content
  (test-util/with-loaded-project
    (let [font-node (test-util/resource-node project "/editor1/test.font")]
      (g/transact {:undoable false}
        [(g/set-property font-node :all-chars false)
         (g/set-property font-node :characters "A")])
      (let [font-map (g/node-value font-node :font-map)
            markup-layout (font/layout-text font-map "<color=#FF0000>A</color>B" false 0 0 1)
            invalid-layout (font/layout-text font-map "<color=#FF0000>A</size>B" false 0 0 1)]
        (is (= "<color=#FF0000>A</color>" (:native-text markup-layout)))
        (is (true? (:use-rich-text markup-layout)))
        (is (= "A" (:native-text invalid-layout)))
        (is (= "A" (:native-markup invalid-layout)))
        (is (true? (:use-rich-text invalid-layout)))))))

(deftest static-native-preview-preserves-markup-attributes-and-entities
  (test-util/with-loaded-project
    (let [font-node (test-util/resource-node project "/editor1/test.font")]
      (g/transact {:undoable false}
        [(g/set-property font-node :all-chars false)
         (g/set-property font-node :characters "A&")])
      (let [font-map (g/node-value font-node :font-map)
            text "<link id='A>B'>A&amp;B</link>"
            text-layout (font/layout-text font-map text false 0 0 1)]
        (is (= "<link id='A>B'>A&amp;</link>" (:native-text text-layout)))
        (is (true? (:use-rich-text text-layout)))))))

(deftest static-native-preview-preserves-row-separators
  (test-util/with-loaded-project
    (let [font-node (test-util/resource-node project "/editor1/test.font")]
      (g/transact {:undoable false}
        [(g/set-property font-node :all-chars false)
         (g/set-property font-node :characters "ABCDEFGHIJKLMNOPQRSTUVWXYZ")
         (g/set-property font-node :cache-width 64)])
      (let [font-map (g/node-value font-node :font-map)
            preview-text (g/node-value font-node :preview-text)
            text-layout (font/layout-text font-map preview-text true (:cache-width font-map) 0 1)]
        (is (s/includes? preview-text "\n"))
        (is (= preview-text (:native-text text-layout)))
        (is (< 1 (:line-count text-layout)))))))

(deftest native-preview-invalid-markup-remains-rich-text
  (test-util/with-loaded-project
    (let [font-node (test-util/resource-node project "/editor1/test.font")
          font-map (g/node-value font-node :font-map)
          markup-layout (font/layout-text font-map "<color=#ff0000>red</color>" false 0 0 1)
          plain-layout (font/layout-text font-map "<>" false 0 0 1)]
      (is (true? (:use-rich-text markup-layout)))
      (is (= "<>" (:native-text plain-layout)))
      (is (= "&lt;&gt;" (:native-markup plain-layout)))
      (is (true? (:use-rich-text plain-layout))))))

(defn- font-map-uses-text-shaping? [font-node]
  (let [^FontRenderer$Params render-params (get-in (g/node-value font-node :font-map)
                                                    [:native-renderer-spec :render-params])]
    (.-useTextShaping render-params)))

(defn- font-map-uses-rich-text? [font-node]
  (get-in (g/node-value font-node :font-map)
          [:native-renderer-spec :use-rich-text]))

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
      (testing "static fonts continue using legacy layout"
        (is (false? (font-map-uses-text-shaping? font-node))))
      (testing "runtime-generated fonts use text shaping"
        (game-project/set-setting! game-project ["font" "runtime_generation"] true)
        (is (true? (font-map-uses-text-shaping? font-node)))))))

(deftest app-manifest-rich-text-selection
  (test-util/with-loaded-project
    (let [game-project (test-util/resource-node project "/game.project")
          font-node (test-util/resource-node project "/editor1/test.font")
          app-manifest (test-util/resource-node project "/app_manifest/default.appmanifest")]
      (testing "rich text is enabled without an app manifest"
        (is (true? (font-map-uses-rich-text? font-node))))
      (g/transact {:undoable false}
        (form/set-value (:form-ops (g/node-value game-project :form-data))
                        ["native_extension" "app_manifest"]
                        (g/node-value app-manifest :resource)))
      (testing "rich text is enabled by default in an app manifest"
        (is (true? (font-map-uses-rich-text? font-node))))
      (g/transact {:undoable false}
        (g/set-property app-manifest :use-rich-text false))
      (testing "rich text can be disabled in an app manifest"
        (is (false? (font-map-uses-rich-text? font-node)))))))

(deftest bitmap-font-uses-native-rich-text-preview
  (test-util/with-loaded-project
    (let [font-node (test-util/resource-node project "/editor1/test.font")]
      (g/transact {:undoable false}
        (g/set-property font-node :output-format :type-bitmap))
      (let [^FontRenderer$Params render-params (get-in (g/node-value font-node :font-map)
                                                       [:native-renderer-spec :render-params])]
        (is (= :defold (g/node-value font-node :type)))
        (is (true? (font-map-uses-rich-text? font-node)))
        (is (true? (.-outputBitmap render-params))))
      (let [error (font/markup-error font-node
                                     :text
                                     (g/node-value font-node :font-map)
                                     "valid\n<color>bad</size>")]
        (is (g/error-warning? error))
        (is (= {:byte-offset 16
                :column 11
                :cursor-range (code.data/line-number->CursorRange 2 11)
                :line 2}
               (:user-data error)))
        (is (s/includes? (test-util/localization (g/error-message error))
                         "mismatched closing tag")))
      (let [font-map (g/node-value font-node :font-map)
            unknown-tag-error (font/markup-error font-node :text font-map "<s ize=14>Text</size>")
            unknown-attribute-error (font/markup-error font-node :text font-map "<size sdf=14>Text</size>")
            unknown-constant-error (font/markup-error font-node :text font-map "<wave fit=word>Text</wave>")]
        (is (= 1 (get-in unknown-tag-error [:user-data :byte-offset])))
        (is (s/includes? (test-util/localization (g/error-message unknown-tag-error))
                         "unknown tag"))
        (is (= 6 (get-in unknown-attribute-error [:user-data :byte-offset])))
        (is (s/includes? (test-util/localization (g/error-message unknown-attribute-error))
                         "unknown attribute"))
        (is (= 10 (get-in unknown-constant-error [:user-data :byte-offset])))
        (is (s/includes? (test-util/localization (g/error-message unknown-constant-error))
                         "unknown value for attribute"))))))

(deftest bmfont-uses-glyph-bank-native-rich-text-preview
  (test-util/with-loaded-project "test/resources/reload_unchanged_project"
    (let [font-node (test-util/resource-node project "/editable/bitmap-font.font")
          font-map (g/node-value font-node :font-map)
          renderer-spec (:native-renderer-spec font-map)
          [plain-width plain-height] (font/measure font-map "A")
          [markup-width markup-height] (font/measure font-map "<color=#ff0000>A</color>")]
      (is (= :bitmap (g/node-value font-node :type)))
      (is (instance? FontRenderer$GlyphBank (:glyph-bank renderer-spec)))
      (is (nil? (:font-bytes renderer-spec)))
      (is (pos? plain-width))
      (is (pos? plain-height))
      (is (= plain-width markup-width))
      (is (= plain-height markup-height)))))

(deftest invalid-font-compilation-does-not-log-exception
  (test-util/with-loaded-project "test/resources/font_error_project"
    (let [font-node (test-util/resource-node project "/main/broken.font")
          ^Logger logger (LoggerFactory/getLogger "editor.font")
          ^ListAppender appender (doto (ListAppender.)
                                   (.start))]
      (.addAppender logger appender)
      (try
        (is (g/error-fatal? (g/node-value font-node :font-map)))
        (is (zero? (count (.-list appender))))
        (finally
          (.detachAppender logger appender)
          (.stop appender))))))

(deftest native-shadow-blur-capacity-does-not-depend-on-alpha
  (test-util/with-loaded-project
    (let [font-node (test-util/resource-node project "/editor1/test.font")]
      (g/transact {:undoable false}
        [(g/set-property font-node :alpha 0.0)
         (g/set-property font-node :shadow-alpha 0.0)
         (g/set-property font-node :shadow-blur 4.0)])
      (let [^FontRenderer$Params render-params (get-in (g/node-value font-node :font-map)
                                                       [:native-renderer-spec :render-params])]
        (is (= 4.0 (double (.-shadowBlur render-params))))
        (is (true? (.-hasShadow render-params)))))))

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

(deftest legacy-text-splitting
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/fonts/score.font")
          font-map (dissoc (g/node-value node-id :font-map) :native-renderer-spec)
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
          no-break (s/replace pre-text "\n" "")
          [w h] (font/measure font-map pre-text true (:cache-width font-map) 0 1)
          [ew eh] (font/measure font-map no-break true (:cache-width font-map) 0 1)
          text-layout (font/layout-text font-map pre-text false 0 0.125 1)]
      (is (.contains pre-text "\n"))
      (is (not (.contains no-break "\n")))
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

(deftest style-authoring-undo-and-validation
  (test-util/with-loaded-project
    (let [node (test-util/resource-node project "/editor1/test.font")
          font-outline (g/node-value node :node-outline)
          styles-outline (first (:children font-outline))
          children (:children styles-outline)
          default-node (:node-id (first children))
          link-node (:node-id (second children))]
      (is (= ["Styles"] (mapv (comp test-util/localization :label) (:children font-outline))))
      (is (= (g/node-value node :styles-node) (:node-id styles-outline)))
      (is (true? (:read-only styles-outline)))
      (is (= ["(default)" "link" "link:hover" "link:active"] (mapv (comp test-util/localization :label) children)))
      (is (= "Name" (test-util/localization (get-in (g/node-value link-node :_properties) [:properties :id :label]))))
      (is (= "Markup" (test-util/localization (get-in (g/node-value link-node :_properties) [:properties :markup :label]))))
      (is (= ["" "(default)" "link" "link:hover" "link:active"]
             (mapv (comp test-util/localization second) (:options (font/style-choices (g/node-value node :font-map))))))
      (is (= "Font style 'missing' does not exist."
             (test-util/localization (g/error-message (font/style-error node (g/node-value node :font-map) "missing")))))
      (is (coll/every? #(not= (:icon font-outline) (:icon %)) children))
      (is (true? (:read-only (first children))))
      (is (true? (get-in (g/node-value default-node :_properties) [:properties :id :read-only?])))
      (g/reset-undo! :undo/global)
      (g/set-property! link-node :id "notice")
      (g/set-property! link-node :markup "<color=#ff6600><ul><wave amplitude=2>")
      (is (nil? (g/node-value link-node :build-errors)))
      (is (= "notice" (get-in (g/node-value node :save-value) [:styles 1 :name])))
      (is (= ["notice" "notice"] (get-in (font/style-choices (g/node-value node :font-map)) [:options 2])))
      (g/undo! :undo/global)
      (g/undo! :undo/global)
      (is (= "link" (g/node-value link-node :id)))
      (g/redo! :undo/global)
      (g/redo! :undo/global)
      (is (= "notice" (g/node-value link-node :id)))
      (test-util/with-prop [link-node :id "link:hover"]
        (is (g/error-fatal? (test-util/prop-error link-node :id))))
      (test-util/with-prop [link-node :markup "<size=48>"]
        (is (g/error-fatal? (test-util/prop-error link-node :markup)))
        (is (g/error-fatal? (g/node-value node :build-targets))))
      (g/transact (mapv #(g/delete-node (:node-id %)) (subvec children 1)))
      (let [saved (g/node-value node :save-value)]
        (is (= [{:name "default"}] (:styles saved)))
        (is (= ["default"] (mapv :name (:styles (font/sanitize-font saved)))))
        (is (= 1 (count (FontStyles/compileStyles (protobuf/map->pb Font$FontDesc saved)))))))))

(deftest style-capability-warnings-allow-builds
  (test-util/with-loaded-project
    (let [node (test-util/resource-node project "/editor1/test.font")
          style-node (:node-id (second (g/node-value node :style-infos)))]
      (g/set-property! style-node :markup "<outline size=2>")
      (is (g/error-warning? (test-util/prop-error style-node :markup)))
      (is (vector? (g/node-value node :build-targets)))
      (is (not (g/error? (build/resolve-node-dependencies node project))))
      (g/set-property! style-node :markup "<outline size=1>")
      (is (nil? (test-util/prop-error style-node :markup)))
      (g/set-property! style-node :markup "<outline alpha=0.5><shadow alpha=0.25>")
      (is (nil? (test-util/prop-error style-node :markup)))
      (let [compiled (g/node-value style-node :compiled-style)]
        (is (instance? FontRenderer$Style compiled))
        (test-util/with-prop [node :outline-width 0]
          (is (g/error-warning? (test-util/prop-error style-node :markup)))
          (is (vector? (g/node-value node :build-targets)))
          (is (identical? compiled (g/node-value style-node :compiled-style)))))
      (g/set-property! style-node :markup "<size=48>")
      (is (g/error-fatal? (g/node-value node :build-targets))))))

(deftest default-style-markup-follows-font-settings
  (test-util/with-loaded-project
    (let [node (test-util/resource-node project "/editor1/test.font")
          default-node (:node-id (first (g/node-value node :style-infos)))
          original-markup "<outline size=1.0 alpha=0.1><shadow x=1.0 y=2.0 blur=1.0 alpha=0.5>"
          updated-markup "<outline size=2.375 alpha=0.12345679><shadow x=-1.25 y=2.5 blur=0.0 alpha=0.375>"]
      (is (= original-markup (get-in (g/node-value default-node :_properties) [:properties :markup :value])))
      (is (true? (get-in (g/node-value default-node :_properties) [:properties :markup :read-only?])))
      (g/reset-undo! :undo/global)
      (g/transact [(g/set-property node :outline-width 2.375)
                   (g/set-property node :outline-alpha 0.12345679)
                   (g/set-property node :shadow-x -1.25)
                   (g/set-property node :shadow-y 2.5)
                   (g/set-property node :shadow-blur 0)
                   (g/set-property node :shadow-alpha 0.375)])
      (is (= updated-markup (g/node-value default-node :markup)))
      (let [saved (g/node-value node :save-value)
            [generated copied] (mapv protobuf/pb->map-with-defaults
                                     (FontStyles/compileStyles
                                       (protobuf/map->pb Font$FontDesc
                                         (assoc saved :styles [{:name "default"}
                                                               {:name "copy" :markup updated-markup}]))))]
        (is (= {:name "default"} (first (:styles saved))))
        (is (= (dissoc generated :name :name-hash) (dissoc copied :name :name-hash))))
      (g/undo! :undo/global)
      (is (= original-markup (g/node-value default-node :markup)))
      (g/redo! :undo/global)
      (is (= updated-markup (g/node-value default-node :markup)))
      (g/transact [(g/set-property node :outline-alpha 0)
                   (g/set-property node :shadow-alpha 0)])
      (is (= "" (g/node-value default-node :markup)))
      (is (= {:name "default"} (g/node-value default-node :style-msg))))))

(deftest styles-save-and-reload
  (test-util/with-scratch-project "test/resources/reload_unchanged_project"
    (let [node (test-util/resource-node project "/editable/bitmap-font.font")
          children (get-in (g/node-value node :node-outline) [:children 0 :children])
          link (:node-id (second children))]
      (g/set-property! link :id "notice")
      (g/set-property! link :markup "<color=#aa3300><ul>")
      (test-util/save-project! project)
      (let [saved (g/node-value node :save-value)]
        (is (= "notice" (get-in saved [:styles 1 :name])))
        (test-util/write-file-resource! workspace "/editable/roundtrip.font" saved)
        (workspace/resource-sync! workspace)
        (let [reloaded (test-util/resource-node project "/editable/roundtrip.font")]
          (is (= (:styles saved) (:styles (g/node-value reloaded :save-value))))
          (is (= ["(default)" "notice" "link:hover" "link:active"]
                 (mapv (comp test-util/localization :label) (get-in (g/node-value reloaded :node-outline) [:children 0 :children])))))))))

(deftest opening-font-view-dispatches-input
  (test-util/with-loaded-project
    (let [[_node view] (test-util/open-scene-view! project app-view "/editor1/test.font" 320 240
                                                (get-in (workspace/get-resource-type workspace "font") [:view-opts :scene]))
          input-context (scene/input-dispatch-context view)
          final-state (reduce (fn [input-state action-type]
                                (scene/dispatch-input-action
                                  input-context
                                  input-state
                                  (scene/augment-action view {:type action-type
                                                             :x 160.0
                                                             :y 120.0
                                                             :screen-x 160.0
                                                             :screen-y 120.0
                                                             :button :primary
                                                             :click-count 1
                                                             :modifiers #{}})))
                              (input/make-input-state)
                              [:mouse-moved :mouse-pressed :mouse-released])]
      (is (= [160.0 120.0] (:view-pos final-state)))
      (is (= #{} (:mouse-buttons final-state))))))

(deftest add-style-command-and-selected-preview
  (test-util/with-loaded-project
    (let [[node view] (test-util/open-scene-view! project app-view "/editor1/test.font" 320 240
                                                       (get-in (workspace/get-resource-type workspace "font") [:view-opts :scene]))
          styles-node (g/node-value node :styles-node)]
      (is (= "Style: (default)" (g/node-value view :tool-info-text)))
      (doseq [[parent name] [[node "style1"] [styles-node "style2"]]]
        (let [contexts [{:name :workbench :env {:selection [parent] :app-view app-view}}]]
          (is (test-util/handler-enabled? :edit.add-embedded-component contexts nil))
          (test-util/handler-run :edit.add-embedded-component contexts nil)
          (let [styles (g/node-value node :style-infos)
                added (:node-id (peek styles))]
            (is (= name (g/node-value added :id)))
            (is (= added (:node-id (peek (get-in (g/node-value node :node-outline) [:children 0 :children])))))
            (g/set-property! added :markup "<color=#ff6600><ul>")
            (app-view/select! app-view [added])
            (is (= {nil {:style name}} (g/node-value view :preview-overrides)))
            (is (= (str "Style: " name) (g/node-value view :tool-info-text)))
            (let [renderables (g/node-value view :all-renderables)
                  font-renderable (coll/first-where #(= node (:node-id %)) (get renderables pass/transparent))]
              (is (= name (get-in font-renderable [:user-data :text-layout :style])))))))
      (let [style-node (:node-id (peek (g/node-value node :style-infos)))]
        (g/set-property! style-node :id "notice")
        (is (= "Style: notice" (g/node-value view :tool-info-text)))
        (is (= {nil {:style "notice"}} (g/node-value view :preview-overrides))))
      (doseq [selection [[(:node-id (first (g/node-value node :style-infos)))] [styles-node] [node] []]]
        (app-view/select! app-view selection)
        (is (= "Style: (default)" (g/node-value view :tool-info-text)))
        (is (= {nil {:style "default"}} (g/node-value view :preview-overrides)))
        (let [renderables (g/node-value view :all-renderables)
              font-renderable (coll/first-where #(= node (:node-id %)) (get renderables pass/transparent))]
          (is (= "default" (get-in font-renderable [:user-data :text-layout :style]))))))))
