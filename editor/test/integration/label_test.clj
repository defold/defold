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

(ns integration.label-test
  (:require [clojure.string :as string]
            [clojure.test :refer :all]
            [clojure.walk :as walk]
            [dynamo.graph :as g]
            [editor.build :as build]
            [editor.camera :as camera]
            [editor.colors :as colors]
            [editor.defold-project :as project]
            [editor.game-object :as game-object]
            [editor.geom :as geom]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.gl.vertex2 :as vtx]
            [editor.label :as label]
            [editor.math :as math]
            [editor.properties :as properties]
            [editor.properties-view :as properties-view]
            [editor.resource :as resource]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [util.coll :as coll]
            [util.fn :as fn]
            [util.murmur :as murmur])
  (:import [com.dynamo.gamesys.proto Label$LabelDesc]
           [editor.types Region]
           [java.nio ByteBuffer]
           [javax.vecmath Matrix4d Point3d Vector3d]))

(deftest label-validation-test
  (test-util/with-loaded-project
    (let [node-id (project/get-resource-node project "/label/test.label")]
      (doseq [[prop cases] [[:font {"no font" ""
                                    "unknown font" "/fonts/unknown.font"}]
                            [:material {"no material" ""
                                        "unknown material" "/materials/unknown.material"}]]
              [case path] cases]
        (testing case
          (test-util/with-prop [node-id prop (workspace/resolve-workspace-resource workspace path)]
            (is (g/error? (test-util/prop-error node-id prop)))))))))

(deftest unassigned-font-label-preview-test
  (test-util/with-loaded-project
    (let [node-id (project/get-resource-node project "/label/test.label")]
      (test-util/with-prop [node-id :font nil]
        (is (nil? (g/node-value node-id :font-map)))
        (let [text-layout (g/node-value node-id :text-layout)]
          (is (= 0 (:line-count text-layout)))
          (is (= 0 (:height text-layout))))
        (let [scene (g/node-value node-id :scene)]
          (is (map? scene))
          (is (nil? (label/render-tris nil {:pass pass/transparent} [(:renderable scene)] 1))))))))

(deftest invalid-markup-is-label-text-property-warning-test
  (test-util/with-loaded-project
    (let [node-id (project/get-resource-node project "/label/test.label")]
      (test-util/with-prop [node-id :text "valid\n<color>bad</size>"]
        (let [property-error (test-util/prop-error node-id :text)]
          (is (g/error-warning? property-error))
          (is (g/error-warning? (g/node-value node-id :markup-error)))
          (is (not (g/error? (g/node-value node-id :text-layout))))
          (is (not (g/error? (g/node-value node-id :build-targets)))))))))

(deftest invalid-font-does-not-become-label-text-property-error-test
  (test-util/with-loaded-project
    (let [node-id (project/get-resource-node project "/label/test.label")
          invalid-font (workspace/resolve-workspace-resource workspace "/fonts/unknown.font")]
      (test-util/with-prop [node-id :font invalid-font]
        (is (g/error-fatal? (test-util/prop-error node-id :font)))
        (is (nil? (g/node-value node-id :markup-error)))
        (is (nil? (test-util/prop-error node-id :text)))))))

(deftest label-aabb-test
  (test-util/with-loaded-project
    (let [node-id (project/get-resource-node project "/label/test.label")]
      (let [aabb (g/node-value node-id :aabb)
            [x y z] (mapv - (math/vecmath->clj (:max aabb)) (math/vecmath->clj (:min aabb)))]
        (is (< 0.0 x))
        (is (< 0.0 y))
        (is (= 0.0 z))))))

(deftest label-scene-test
  (test-util/with-loaded-project
    (let [node-id (project/get-resource-node project "/label/test.label")
          scene (g/node-value node-id :scene)
          aabb (g/node-value node-id :aabb)]
      (is (= aabb (:aabb scene)))
      (is (= node-id (:node-id scene)))
      (is (= node-id (some-> scene :renderable :select-batch-key)))
      (is (= :blend-mode-alpha (some-> scene :renderable :batch-key :blend-mode)))
      (is (= "Label" (some-> scene :renderable :user-data :text-data :text-layout :text)))
      (is (string/includes? (some-> scene :renderable :user-data :material-shader shader/vertex-shader-source) "gl_Position"))
      (let [fragment-source (some-> scene :renderable :user-data :material-shader shader/fragment-shader-source)]
        (is (string/starts-with? fragment-source "#version 330"))
        (is (re-find #"\bout vec4 \w+;" fragment-source))
        (is (not (re-find #"\bgl_FragColor\b" fragment-source))))
      (test-util/test-uses-assigned-material
        workspace project node-id
        :material
        [:renderable :user-data :material-shader]
        [:renderable :user-data :gpu-texture]))))

(deftest native-label-text-box-alignment-test
  (test-util/with-loaded-project
    (let [node-id (project/get-resource-node project "/label/test.label")
          distance-field-font (workspace/find-resource workspace "/editor1/test.font")]
      (test-util/with-prop [node-id :font distance-field-font]
        (let [text-data (g/node-value node-id :text-data)]
          (is (some? (get-in text-data [:font-data :font-map :native-renderer-spec])))
          (is (= {:box-height 32.0
                  :offset [-64.0 -16.0 0.0]
                  :vertical-align :middle}
                 (select-keys text-data [:box-height :offset :vertical-align]))))))))

(defn- get-render-calls-by-pass
  [scene camera selection key-fn]
  (let [scene-render-data (scene/produce-scene-render-data {:scene scene :selection selection :hidden-renderable-tags #{} :hidden-node-outline-key-paths #{} :local-camera camera})
        render-data scene-render-data
        renderables (:renderables render-data)
        old-render-lines label/render-lines
        old-render-tris label/render-tris]
    (into {}
          (keep (fn [pass]
                  (let [render-args (scene/pass-render-args (Region. 0 100 0 100) camera pass)
                        calls (fn/with-logged-calls [label/render-lines label/render-tris]
                                (let [patched-renderables (walk/postwalk-replace {old-render-lines label/render-lines
                                                                                  old-render-tris label/render-tris}
                                                                                 renderables)]
                                  (scene/batch-render nil render-args (get patched-renderables pass) key-fn)))]
                    (when (seq calls)
                      [pass calls]))))
          pass/render-passes)))

(defn- map-render-calls
  [transform-calls-fn render-calls-by-pass]
  (into {}
        (map (fn [[pass calls-by-fn]]
               [pass (into {}
                           (map (fn [[fn calls]]
                                  [fn (transform-calls-fn calls)]))
                           calls-by-fn)]))
        render-calls-by-pass))

(deftest label-batch-render-test
  (test-util/with-loaded-project
    (let [make-restore-point! #(test-util/make-system-reverter)
          add-label-component! #(test-util/add-embedded-component! % (workspace/get-resource-type workspace "label"))
          [go view] (test-util/open-scene-view! project app-view "/game_object/test.go" 128 128)
          render-calls (fn [selection key-fn]
                         (get-render-calls-by-pass
                           (g/node-value go :scene)
                           (g/node-value view :camera)
                           selection
                           key-fn))
          render-call-counts (comp (partial map-render-calls count)
                                   render-calls)]

      (testing "Single label"
        (with-open [_ (make-restore-point!)]
          (add-label-component! go)
          (is (= {pass/outline {label/render-lines 1}
                  pass/transparent {label/render-tris 1}}
                 (render-call-counts #{} :batch-key)))
          (is (= {pass/outline {label/render-lines 1}
                  pass/transparent {label/render-tris 1}}
                 (render-call-counts #{} :select-batch-key)))))

      (testing "Identical labels"
        (with-open [_ (make-restore-point!)]
          (add-label-component! go)
          (add-label-component! go)
          (add-label-component! go)
          (add-label-component! go)
          (is (= {pass/outline {label/render-lines 1}
                  pass/transparent {label/render-tris 1}}
                 (render-call-counts #{} :batch-key)))
          (is (= {pass/outline {label/render-lines 4}
                  pass/transparent {label/render-tris 4}}
                 (render-call-counts #{} :select-batch-key)))))

      (testing "Blend mode differs"
        (with-open [_ (make-restore-point!)]
          (test-util/prop! (add-label-component! go) :blend-mode :blend-mode-add)
          (test-util/prop! (add-label-component! go) :blend-mode :blend-mode-add)
          (test-util/prop! (add-label-component! go) :blend-mode :blend-mode-mult)
          (test-util/prop! (add-label-component! go) :blend-mode :blend-mode-mult)
          (is (= {pass/outline {label/render-lines 1}
                  pass/transparent {label/render-tris 2}}
                 (render-call-counts #{} :batch-key)))))

      (testing "Font differs"
        (with-open [_ (make-restore-point!)]
          (test-util/prop! (add-label-component! go) :font (workspace/find-resource workspace "/fonts/active_menu_item.font"))
          (test-util/prop! (add-label-component! go) :font (workspace/find-resource workspace "/fonts/active_menu_item.font"))
          (test-util/prop! (add-label-component! go) :font (workspace/find-resource workspace "/fonts/big_score.font"))
          (test-util/prop! (add-label-component! go) :font (workspace/find-resource workspace "/fonts/big_score.font"))
          (is (= {pass/outline {label/render-lines 1}
                  pass/transparent {label/render-tris 2}}
                 (render-call-counts #{} :batch-key)))))

      (testing "Material differs"
        (with-open [_ (make-restore-point!)]
          (test-util/prop! (add-label-component! go) :material (workspace/find-resource workspace "/fonts/active_menu_item.material"))
          (test-util/prop! (add-label-component! go) :material (workspace/find-resource workspace "/fonts/active_menu_item.material"))
          (test-util/prop! (add-label-component! go) :material (workspace/find-resource workspace "/fonts/big_score_font.material"))
          (test-util/prop! (add-label-component! go) :material (workspace/find-resource workspace "/fonts/big_score_font.material"))
          (is (= {pass/outline {label/render-lines 1}
                  pass/transparent {label/render-tris 2}}
                 (render-call-counts #{} :batch-key))))))))

(deftest label-migration-test
  (test-util/with-loaded-project "test/resources/label_migration_project"
    (let [resources-with-dirty-save-data (into #{}
                                               (map :resource)
                                               (project/dirty-save-data project))]
      (letfn [(resource-has-dirty-save-data? [resource]
                (assert (resource/file-resource? resource))
                (contains? resources-with-dirty-save-data resource))

              (verify-embedded-component [host-resource-proj-path embedded-component-outline-path expected-scale expected-dirty]
                (let [host-resource (workspace/find-resource workspace host-resource-proj-path)]
                  (is (resource/resource? host-resource))
                  (let [host-resource-node-id (project/get-resource-node project host-resource)
                        embedded-component-node-id (:node-id (test-util/outline host-resource-node-id embedded-component-outline-path))]
                    (is (g/node-instance? game-object/EmbeddedComponent embedded-component-node-id))
                    (is (= expected-scale (g/node-value embedded-component-node-id :scale)))
                    (is (= expected-dirty (resource-has-dirty-save-data? host-resource))))))

              (verify-referenced-component [host-resource-proj-path referenced-component-outline-path expected-scale expected-dirty]
                (let [host-resource (workspace/find-resource workspace host-resource-proj-path)]
                  (is (resource/resource? host-resource))
                  (let [host-resource-node-id (project/get-resource-node project host-resource)
                        referenced-component-node-id (:node-id (test-util/outline host-resource-node-id referenced-component-outline-path))]
                    (is (g/node-instance? game-object/ReferencedComponent referenced-component-node-id))
                    (is (= expected-scale (g/node-value referenced-component-node-id :scale)))
                    (let [referenced-label-resource (g/node-value referenced-component-node-id :source-resource)]
                      (is (resource/resource? referenced-label-resource))
                      (is (= expected-dirty (resource-has-dirty-save-data? referenced-label-resource)))
                      (is (= expected-dirty (resource-has-dirty-save-data? host-resource)))))))]

        (testing "Scale value was moved from LabelDesc to ComponentDesc in game object."
          (verify-embedded-component "/scale_migration/embedded_scaled_label.go" [0] [3.0 4.0 5.0] false)
          (verify-referenced-component "/scale_migration/referenced_scaled_label.go" [0] [2.0 3.0 4.0] true))

        (testing "Scale value was moved from LabelDesc to ComponentDesc in game object embedded inside collection."
          (verify-embedded-component "/scale_migration/embedded_scaled_label.collection" [0 0] [3.0 4.0 5.0] false)
          (verify-referenced-component "/scale_migration/referenced_scaled_label.collection" [0 0] [2.0 3.0 4.0] true))

        (testing "Scale value was moved from LabelDesc to ComponentDesc in child game object embedded inside collection."
          (verify-embedded-component "/scale_migration/embedded_scaled_label_child.collection" [0 0 0] [3.0 4.0 5.0] false)
          (verify-referenced-component "/scale_migration/referenced_scaled_label_child.collection" [0 0 0] [2.0 3.0 4.0] true))

        (testing "After migration, the default scale read from the LabelDesc does not overwrite the migrated scale in the ComponentDesc."
          (verify-referenced-component "/scale_migration/referenced_unscaled_label.go" [0] [2.0 3.0 4.0] false)
          (verify-referenced-component "/scale_migration/referenced_unscaled_label.collection" [0 0] [2.0 3.0 4.0] false)
          (verify-referenced-component "/scale_migration/referenced_unscaled_label_child.collection" [0 0 0] [2.0 3.0 4.0] false))))))

;; Label effect controls reflect the outline and shadow support of the selected
;; font.
(deftest label-effect-controls-follow-font-support
  (test-util/with-loaded-project
    (let [font-node (project/get-resource-node project "/editor1/test.font")
          label-node (project/get-resource-node project "/label/test.label")
          original-outline (g/node-value label-node :outline)
          original-shadow (g/node-value label-node :shadow)]
      (g/transact {:undoable false}
        [(g/set-property font-node :all-chars false)
         (g/set-property font-node :characters "A")
         (g/set-property font-node :size 20)
         (g/set-property label-node :font (workspace/find-resource workspace "/editor1/test.font"))])
      (doseq [[mode material] [[:vector-font-mode-sdf "/builtins/fonts/font-df.material"]
                               [:vector-font-mode-vector "/builtins/fonts/font-vector.material"]]
              runtime [false true]
              [outline shadow] [[false false] [true false] [false true] [true true]]]
        (g/transact {:undoable false}
          [(g/set-property font-node :vector-font-mode mode)
           (g/set-property font-node :material (workspace/find-resource workspace material))
           (g/set-property font-node :runtime runtime)
           (g/set-property font-node :outline-alpha (if outline 1.0 0.0))
           (g/set-property font-node :shadow-alpha (if shadow 1.0 0.0))])
        (let [properties (:properties (g/node-value label-node :_properties))]
          (is (= (not outline) (get-in properties [:outline :read-only?])))
          (is (= (not shadow) (get-in properties [:shadow :read-only?])))))
      (is (= original-outline (g/node-value label-node :outline)))
      (is (= original-shadow (g/node-value label-node :shadow))))))

(deftest selected-font-style-validation-and-preview
  (test-util/with-scratch-project test-util/project-path
    (let [node (project/get-resource-node project "/label/test.label")]
      (is (= "default" (g/node-value node :style)))
      (doseq [style ["" "default" "link"]]
        (test-util/with-prop [node :style style]
          (is (nil? (test-util/prop-error node :style)))
          (is (= style (:style (g/node-value node :text-layout))))
          (is (= style (:style (g/node-value node :save-value) "default")))
          (is (not (contains? (g/node-value node :save-value) :style-hash)))
          (with-open [_ (test-util/build! node)]
            (let [built (test-util/built-pb node Label$LabelDesc)]
              (is (= style (.getStyle built)))
              (is (= (if (= "" style) 0 (murmur/hash64 style)) (.getStyleHash built)))))))
      (test-util/with-prop [node :style "missing"]
        (is (g/error-fatal? (test-util/prop-error node :style)))
        (is (g/error-fatal? (g/node-value node :build-targets)))))))

;; Invalid vector sizes must block label builds, while hidden bitmap font sizes
;; remain ignored.
(deftest vector-font-size-validation-blocks-build
  (test-util/with-loaded-project
    (let [label-node (project/get-resource-node project "/label/test.label")]
      (test-util/prop! label-node :font-size 0.0)
      (is (nil? (test-util/prop-error label-node :font-size)))
      (is (not (g/error? (g/node-value label-node :build-targets))))
      (test-util/prop! label-node :font (workspace/find-resource workspace "/fonts/vector_implicit_dynamic.font"))
      (test-util/prop! label-node :material (workspace/find-resource workspace "/builtins/fonts/label-vector.material"))
      (doseq [size [0.0 -1.0]]
        (test-util/prop! label-node :font-size size)
        (is (g/error-fatal? (test-util/prop-error label-node :font-size)))
        (is (g/error-fatal? (build/resolve-node-dependencies label-node project))))
      (test-util/prop! label-node :font-size 32.0)
      (is (nil? (test-util/prop-error label-node :font-size)))
      (is (not (g/error? (g/node-value label-node :build-targets)))))))

;; Label builds retain the vector display size and rich-text style.
(deftest vector-font-size-and-style-survive-build
  (test-util/with-temp-project-content
    {"/styled.font" {:font "/builtins/fonts/vera_mo_bd.ttf"
                     :material "/builtins/fonts/font-vector.material"
                     :vector-font-mode :vector-font-mode-vector
                     :runtime false
                     :size 37
                     :outline-alpha 1.0
                     :outline-width 2.0
                     :characters "A"
                     :styles [{:name "default"}
                              {:name "notice" :markup "<color=#ff6600>"}]}
     "/styled.label" {:font "/styled.font"
                      :material "/builtins/fonts/label-vector.material"
                      :text "A"
                      :size [128.0 32.0 0.0 0.0]
                      :font-size 64.0
                      :style "notice"}}
    (test-util/set-setting! (test-util/resource-node project "/game.project") ["shader" "exclude_gles_sm100"] true)
    (let [font-node (test-util/resource-node project "/styled.font")
          label-node (test-util/resource-node project "/styled.label")]
      (doseq [runtime [false true]]
        (test-util/prop! font-node :runtime runtime)
        (is (= "notice" (:style (g/node-value label-node :text-layout))))
        (let [width (:width (g/node-value label-node :text-layout))]
          (test-util/with-prop [label-node :font-size 32.0]
            (is (= width (* 2.0 (:width (g/node-value label-node :text-layout)))))))
        (with-open [_ (test-util/build! label-node)]
          (let [built-label (test-util/built-pb label-node Label$LabelDesc)
                built-font (test-util/built-pb font-node com.dynamo.render.proto.Font$FontMap)]
            (is (= 64.0 (.getFontSize built-label)))
            (is (= (murmur/hash64 "notice") (.getStyleHash built-label)))
            (is (= 37 (.getSize built-font)))
            (is (= ["default" "notice"] (mapv #(.getName %) (.getStylesList built-font))))))))))

;; Loading and building legacy bitmap labels preserves their font and material
;; settings.
(deftest legacy-bitmap-font-and-label-materials-are-preserved
  (test-util/with-temp-project-content
    {"/legacy.font" {:font "/builtins/fonts/vera_mo_bd.ttf"
                     :material "/builtins/fonts/font.material"
                     :size 24
                     :antialias 0
                     :characters "A"}
     "/legacy.label" {:font "/legacy.font"
                      :material "/builtins/fonts/label.material"
                      :size [128.0 32.0 0.0 0.0]
                      :text "A"}}
    (let [font-node (test-util/resource-node project "/legacy.font")
          label-node (test-util/resource-node project "/legacy.label")]
      (is (= :vector-font-mode-bitmap (g/node-value font-node :vector-font-mode)))
      (is (= :defold (g/node-value font-node :type)))
      (is (false? (g/node-value font-node :antialias)))
      (is (= "/builtins/fonts/font.material" (:material (g/node-value font-node :save-value))))
      (with-open [_ (test-util/build! label-node)]
        (is (= "/builtins/fonts/label.materialc" (.getMaterial (test-util/built-pb label-node Label$LabelDesc))))
        (let [built-font (test-util/built-pb font-node com.dynamo.render.proto.Font$FontMap)]
          (is (= com.dynamo.render.proto.Font$FontTextureFormat/TYPE_BITMAP (.getOutputFormat built-font)))
          (is (= 0 (.getAntialias built-font)))
          (is (not (string/blank? (.getGlyphBank built-font))))))
      (test-util/save-project! project)
      (workspace/resource-sync! workspace)
      (is (= :vector-font-mode-bitmap (g/node-value font-node :vector-font-mode)))
      (is (= "/builtins/fonts/font.material" (:material (g/node-value font-node :save-value))))
      (is (false? (g/node-value font-node :antialias))))))

;; Arc settings survive compilation and preview. Invalid edits report errors
;; and remain previewable.
(deftest label-arc-properties-test
  (test-util/with-scratch-project test-util/project-path
    (let [node (project/get-resource-node project "/label/test.label")
          defaults {:arc-radius 100.0
                    :arc-angle 0.0}
          authored {:arc-radius -200.0
                    :arc-angle 120.0}
          arc-properties (into [] (coll/keys authored))
          straight-layout (g/node-value node :text-layout)]
      (doseq [[property value] defaults]
        (is (= value (g/node-value node property)))
        (is (not (contains? (g/node-value node :save-value) property))))
      (doseq [[property value] authored]
        (test-util/prop! node property value))
      (is (= (dissoc straight-layout :layout-width) (dissoc (g/node-value node :text-layout) :layout-width)))
      (is (= authored (select-keys (g/node-value node :text-data) arc-properties)))
      (is (= authored (select-keys (g/node-value node :save-value) arc-properties)))
      (with-open [_ (test-util/build! node)]
        (let [built (test-util/built-pb node Label$LabelDesc)]
          (is (= -200.0 (.getArcRadius built)))
          (is (= 120.0 (.getArcAngle built)))))
      (test-util/with-prop [node :arc-angle 361.0]
        (is (g/error-fatal? (test-util/prop-error node :arc-angle)))
        (is (g/error-fatal? (g/node-value node :build-targets)))
        (is (not (g/error? (g/node-value node :text-data))))
        (is (not (contains? (g/node-value node :text-data) :arc-radius)))))))

;; Typed fields, property edits and scrubbing stay within the inclusive angle
;; range instead of displaying invalid values or falling back to flat text.
(deftest label-arc-angle-edit-range-test
  (test-util/with-scratch-project test-util/project-path
    (let [node (project/get-resource-node project "/label/test.label")]
      (doseq [[entered expected] [[-1.0 0.0]
                                  [-360.0 0.0]
                                  [0.0 0.0]
                                  [360.0 360.0]
                                  [361.0 360.0]]]
        (let [property (get-in (properties/coalesce [(g/node-value node :_properties)])
                               [:properties :arc-angle])]
          (is (= [0.0 360.0] ((juxt :min :max) (:edit-type property))))
          (is (= expected ((:to-value (properties-view/make-control-view property nil nil))
                           (str entered))))
          (properties/set-values! property [entered])
          (is (= expected (g/node-value node :arc-angle)))
          (is (nil? (test-util/prop-error node :arc-angle))))))))

;; The curved box replaces the textbox, keeps short radial end edges,
;; closes both full-circle endpoints, and follows transforms and selection
;; colors. Selected arcs show a center cross without affecting tight framing.
(deftest label-arc-box-guide-test
  (test-util/with-scratch-project test-util/project-path
    (let [node (project/get-resource-node project "/label/test.label")]
      (is (= 8 (count (get-in (g/node-value node :scene) [:children 0 :renderable :user-data :line-data]))))
      (test-util/prop! node :text "AA\nAA")
      (test-util/prop! node :arc-radius 200.0)
      (doseq [degrees [60.0 90.0]]
        (test-util/prop! node :arc-angle degrees)
        (let [label-scene (g/node-value node :scene)
              render-data (fn render-data [label-scene selection]
                            (scene/produce-scene-render-data {:scene label-scene
                                                              :selection selection
                                                              :hidden-renderable-tags #{}
                                                              :hidden-node-outline-key-paths #{}
                                                              :local-camera (camera/make-camera)}))
              renderable (get-in (render-data label-scene [node]) [:renderables pass/outline 0])
              guide (get-in renderable [:user-data :line-data])
              selected-lines (get-in renderable [:user-data :selected-line-data])
              [inner outer] guide
              cap-length (Math/hypot (- (double (first outer)) (double (first inner)))
                                     (- (double (second outer)) (double (second inner))))
              transform (doto (Matrix4d.)
                          (.setIdentity)
                          (.setTranslation (Vector3d. 10.0 -5.0 0.0)))
              unselected (#'label/gen-lines-vb (get-in (render-data label-scene []) [:renderables pass/outline]))
              selected (#'label/gen-lines-vb [(assoc renderable :world-transform transform)])
              ^ByteBuffer buffer (vtx/buf selected)]
          (is (< 8 (count guide)))
          (is (< (Math/abs (- (Math/atan2 (double (first inner)) (double (second inner)))
                              (* -0.5 (Math/toRadians (double degrees)))))
                 0.001))
          (is (< 0.0 cap-length 150.0))
          (is (= :self-selected (:selected renderable)))
          (is (= :self-selected (get-in (render-data (scene/claim-scene label-scene 0 "label") [0])
                                        [:renderables pass/outline 0 :selected])))
          (is (= guide (get-in label-scene [:renderable :user-data :line-data])))
          (is (= (count guide) (count unselected)))
          (is (= [[-5.0 -5.0 0.0] [5.0 5.0 0.0] [-5.0 5.0 0.0] [5.0 -5.0 0.0]]
                 (mapv vec (subvec (vec selected-lines) (- (count selected-lines) 4)))))
          (is (identical? selected-lines (:selected-line-data (g/node-value node :arc-data))))
          (is (= (count selected-lines) (count selected)))
          (is (< (count unselected) (count selected)))
          (is (< (Math/abs (- (+ (double (first inner)) 10.0) (.getFloat buffer 0))) 0.001))
          (is (< (Math/abs (- (- (double (second inner)) 5.0) (.getFloat buffer Float/BYTES))) 0.001))
          (is (= (mapv float colors/selected-outline-color)
                 (mapv #(.getFloat buffer (* (+ 3 %) Float/BYTES)) (range 4))))
          (let [marker-offset (* (- (count selected) 4) 7 Float/BYTES)]
            (is (= 5.0 (.getFloat buffer marker-offset)))
            (is (= -10.0 (.getFloat buffer (+ marker-offset Float/BYTES)))))))
      (doseq [degrees [360.0]]
        (test-util/prop! node :arc-angle degrees)
        (is (nil? (test-util/prop-error node :arc-angle)))
        (let [guide (:line-data (g/node-value node :arc-data))]
          (doseq [[start end] [[(first guide) (nth guide (- (count guide) 3))]
                               [(nth guide 2) (nth guide (dec (count guide)))]]]
            (is (< (Math/hypot (- (double (first end)) (double (first start)))
                               (- (double (second end)) (double (second start))))
                   0.001)))))
      (test-util/prop! node :arc-angle 0.0)
      (is (= 8 (count (get-in (g/node-value node :scene) [:children 0 :renderable :user-data :line-data])))))))

;; Arc mode wraps at the arc length while keeping Line Break editable and Size
;; disabled. Switching back restores the stored textbox dimensions.
(deftest label-arc-layout-mode-test
  (test-util/with-scratch-project test-util/project-path
    (let [node (project/get-resource-node project "/label/test.label")]
      (test-util/prop! node :text "Alpha beta gamma\nDelta epsilon")
      (test-util/prop! node :size [40.0 200.0 0.0])
      (test-util/prop! node :line-break true)
      (test-util/prop! node :arc-radius 30.0)
      (let [wrapped (g/node-value node :text-layout)]
        (is (< 2 (:line-count wrapped)))
        (doseq [degrees [60.0 90.0]]
          (test-util/prop! node :arc-angle degrees)
          (is (< 2 (:line-count (g/node-value node :text-layout))))
          (is (true? (get-in (g/node-value node :_properties) [:properties :size :read-only?])))
          (is (not (get-in (g/node-value node :_properties) [:properties :line-break :read-only?])))
          (test-util/with-prop [node :line-break false]
            (is (= 2 (:line-count (g/node-value node :text-layout)))))
          (test-util/with-prop [node :arc-radius 300.0]
            (is (= 2 (:line-count (g/node-value node :text-layout)))))
          (let [bounds (g/node-value node :aabb)]
            (test-util/with-prop [node :size [800.0 1000.0 0.0]]
              (is (= bounds (g/node-value node :aabb))))))
        (test-util/prop! node :arc-angle 0.0)
        (is (= wrapped (g/node-value node :text-layout)))
        (is (= [40.0 200.0 0.0] (g/node-value node :size)))
        (is (true? (g/node-value node :line-break)))
        (is (false? (get-in (g/node-value node :_properties) [:properties :size :read-only?])))
        (is (not (get-in (g/node-value node :_properties) [:properties :line-break :read-only?])))))))

;; Framing encloses the curved box and text, excluding the distant circle center
;; and original layout rectangle, for changed angles.
(deftest label-arc-framing-test
  (test-util/with-scratch-project test-util/project-path
    (let [[node view] (test-util/open-scene-view! project app-view "/label/test.label" 128 128)]
      (test-util/prop! node :text "Arc\nsecond line\nthird")
      (test-util/prop! node :size [800.0 200.0 0.0])
      (test-util/prop! node :tracking 10.0)
      (let [straight-aabb (g/node-value node :aabb)]
        (doseq [degrees [60.0 90.0]]
          (test-util/prop! node :arc-radius 2000.0)
          (test-util/prop! node :arc-angle degrees)
          (let [arc-data (g/node-value node :arc-data)
                aabb (g/node-value view :selected-aabb)
                center (Point3d. 0.0 0.0 0.0)
                extent (math/vecmath->clj (geom/aabb-extent aabb))]
            (is (= (:aabb arc-data) aabb))
            (is (not (geom/aabb-contains? aabb center)))
            (doseq [point (subvec (vec (:line-data arc-data)) 0 4)]
              (is (geom/aabb-contains? aabb (Point3d. (double-array point)))))
            (is (< (double (first extent)) 3000.0))
            (is (< (double (second extent)) 800.0))))
        (test-util/prop! node :arc-angle 0.0)
        (is (= straight-aabb (g/node-value node :aabb)))))))

;; Ordinary text remains planar even when the unused Size Z component
;; is nonzero.
(deftest label-textbox-planar-bounds-test
  (test-util/with-scratch-project test-util/project-path
    (let [node (project/get-resource-node project "/label/test.label")]
      (test-util/prop! node :size [40.0 200.0 0.0])
      (let [bounds (g/node-value node :aabb)]
        (test-util/prop! node :size [40.0 200.0 100.0])
        (is (= bounds (g/node-value node :aabb)))))))

;; Radius changes move the first baseline about the component origin, regardless
;; of textbox pivot or wrapping width, while retaining line spacing.
(deftest label-arc-center-test
  (test-util/with-scratch-project test-util/project-path
    (let [node (project/get-resource-node project "/label/test.label")]
      (test-util/prop! node :text "AA\nAA")
      (test-util/prop! node :line-break true)
      (test-util/prop! node :arc-angle 90.0)
      (doseq [pivot [:pivot-center :pivot-nw :pivot-se]
              radius [-200.0 200.0 400.0]]
        (test-util/prop! node :pivot pivot)
        (test-util/prop! node :arc-radius radius)
        (let [arc-data (g/node-value node :arc-data)
              [x y] (nth (:selected-line-data arc-data) (count (:line-data arc-data)))
              expected (* (double radius) (Math/sqrt 0.5))]
          (is (= [0.0 0.0] (:offset (g/node-value node :text-data))))
          (is (< (Math/abs (+ (double x) expected)) 0.001))
          (is (< (Math/abs (- (double y) expected)) 0.001)))))))
