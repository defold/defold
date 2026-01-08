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

(ns integration.gui-test
  (:require [clojure.string :as str]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.defold-project :as project]
            [editor.gl.pass :as pass]
            [editor.gui :as gui]
            [editor.handler :as handler]
            [editor.localization :as localization]
            [editor.math :as math]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [internal.node :as in]
            [support.test-support :as test-support]
            [util.coll :as coll :refer [pair]]
            [util.fn :as fn])
  (:import [com.dynamo.gamesys.proto Gui$NodeDesc]))

(defn- prop [node-id label]
  (test-util/prop node-id label))

(defn- prop! [node-id label val]
  (test-util/prop! node-id label val))

(defn- gui-node [scene id]
  (let [id->node (->> (get-in (g/valid-node-value scene :node-outline) [:children 0])
                      (tree-seq fn/constantly-true :children)
                      (map :node-id)
                      (map (fn [node-id] [(g/valid-node-value node-id :id) node-id]))
                      (into {}))]
    (id->node id)))

(defn- gui-resources-node [resources-node-outline-key scene]
  (->> (g/node-value scene :node-outline)
       (:children)
       (some (fn [resources-node-outline]
               (when (= resources-node-outline-key (:node-outline-key resources-node-outline))
                 (:node-id resources-node-outline))))))

(def ^:private gui-textures (partial gui-resources-node "Textures"))
(def ^:private gui-fonts (partial gui-resources-node "Fonts"))
(def ^:private gui-layers (partial gui-resources-node "Layers"))
(def ^:private gui-particlefx-resources (partial gui-resources-node "Particle FX"))
(def ^:private gui-materials (partial gui-resources-node "Materials"))
(def ^:private gui-spine-scenes (partial gui-resources-node "Spine Scenes"))

(defn- gui-resource-node [gui-resources-node-fn scene resource-name]
  (some (fn [resource-node-outline]
          (when (= resource-name (:node-outline-key resource-node-outline))
            (:node-id resource-node-outline)))
        (some-> (gui-resources-node-fn scene)
                (g/node-value :node-outline)
                (:children))))

(def ^:private gui-texture (partial gui-resource-node gui-textures))
(def ^:private gui-font (partial gui-resource-node gui-fonts))
(def ^:private gui-layer (partial gui-resource-node gui-layers))
(def ^:private gui-particlefx-resource (partial gui-resource-node gui-particlefx-resources))
(def ^:private gui-material (partial gui-resource-node gui-materials))
(def ^:private gui-spine-scene (partial gui-resource-node gui-spine-scenes))

(defn- property-value-choices [node-id label]
  (->> (g/node-value node-id :_properties)
       :properties
       label
       :edit-type
       :options
       (mapv first)))

(deftest load-gui
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/logic/main.gui")
          _gui-node (ffirst (g/sources-of node-id :child-outlines))]
      (is (some? _gui-node)))))

(deftest gui-scene-generation
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/logic/main.gui")
          scene (g/node-value node-id :scene)]
      (is (= 0.25 (get-in scene [:children 2 :children 1 :renderable :user-data :color 3]))))))

(deftest gui-scene-material
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/gui/simple.gui")
          scene (g/node-value node-id :scene)]
      (test-util/test-uses-assigned-material workspace project node-id
                                             :material
                                             [:children 0 :renderable :user-data :material-shader]
                                             [:children 0 :renderable :user-data :gpu-texture]))))

(deftest gui-scene-validation
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/logic/main.gui")]
      (is (nil? (test-util/prop-error node-id :script)))
      ;; Script is not required, so nil would be ok
      (test-util/with-prop [node-id :script nil]
        (is (nil? (test-util/prop-error node-id :script))))
      (test-util/with-prop [node-id :script (workspace/resolve-workspace-resource workspace "/not_found.script")]
        (is (g/error-fatal? (test-util/prop-error node-id :script))))
      (is (nil? (test-util/prop-error node-id :material)))
      (doseq [v [nil (workspace/resolve-workspace-resource workspace "/not_found.material")]]
        (test-util/with-prop [node-id :material v]
          (is (g/error-fatal? (test-util/prop-error node-id :material)))))
      (is (nil? (test-util/prop-error node-id :max-nodes)))
      (doseq [v [0 8193]]
        (test-util/with-prop [node-id :max-nodes v]
          (is (g/error-fatal? (test-util/prop-error node-id :max-nodes)))))
      ;; Valid number, but now the amount of nodes exceeds the max
      (test-util/with-prop [node-id :max-nodes 1]
        (is (g/error-fatal? (test-util/prop-error node-id :max-nodes)))))))

(deftest gui-box-auto-size
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/logic/main.gui")
          box (gui-node node-id "left")
          size [150.0 50.0 0.0]
          sizes {:ball [64.0 32.0 0.0]
                 :left-hud [200.0 640.0 0.0]}]
      (is (= :size-mode-manual (g/node-value box :size-mode)))
      (is (= size (g/node-value box :size)))
      (g/set-property! box :texture "atlas_texture/left_hud")
      (is (= size (g/node-value box :size)))
      (g/set-property! box :texture "atlas_texture/ball")
      (is (= size (g/node-value box :size)))
      (g/set-property! box :size-mode :size-mode-auto)
      (is (= (:ball sizes) (g/node-value box :size)))
      (g/set-property! box :texture "atlas_texture/left_hud")
      (is (= (:left-hud sizes) (g/node-value box :size))))))

(deftest gui-scene-pie
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/logic/main.gui")
          pie (gui-node node-id "hexagon")
          scene (g/node-value node-id :scene)]
      (is (> (count (get-in scene [:children 3 :renderable :user-data :line-data])) 0))
      (test-util/with-prop [pie :perimeter-vertices 3]
        (is (g/error-fatal? (test-util/prop-error pie :perimeter-vertices))))
      (test-util/with-prop [pie :perimeter-vertices 4]
        (is (not (g/error-fatal? (test-util/prop-error pie :perimeter-vertices)))))
      (test-util/with-prop [pie :perimeter-vertices 1000]
        (is (not (g/error-fatal? (test-util/prop-error pie :perimeter-vertices)))))
      (test-util/with-prop [pie :perimeter-vertices 1001]
        (is (g/error-fatal? (test-util/prop-error pie :perimeter-vertices)))))))

(deftest gui-textures-test
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/logic/main.gui")
          outline (g/node-value node-id :node-outline)
          png-node (get-in outline [:children 0 :children 1 :node-id])
          png-tex (get-in outline [:children 1 :children 0 :node-id])]
      (is (some? png-tex))
      (is (= "png_texture" (prop png-node :texture)))
      (prop! png-tex :name "new-name")
      (is (= "new-name" (prop png-node :texture))))))

(deftest gui-texture-validation
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/logic/main.gui")
          atlas-tex (:node-id (test-util/outline node-id [1 1]))]
      (test-util/with-prop [atlas-tex :name ""]
        (is (g/error-fatal? (test-util/prop-error atlas-tex :name))))
      (doseq [v [nil (workspace/resolve-workspace-resource workspace "/not_found.atlas")]]
        (test-util/with-prop [atlas-tex :texture v]
          (is (g/error-fatal? (test-util/prop-error atlas-tex :texture))))))))

(deftest gui-atlas
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/logic/main.gui")
          outline (g/node-value node-id :node-outline)
          box (get-in outline [:children 0 :children 2 :node-id])
          atlas-tex (get-in outline [:children 1 :children 1 :node-id])]
      (is (some? atlas-tex))
      (is (= "atlas_texture/anim" (prop box :texture)))
      (prop! atlas-tex :name "new-name")
      (is (= "new-name/anim" (prop box :texture))))))

(deftest gui-shaders
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/logic/main.gui")]
      (is (some? (g/node-value node-id :material-shader))))))

(defn- font-resource-node [project gui-font-node]
  (project/get-resource-node project (g/node-value gui-font-node :font)))

(defn- build-targets-deps [gui-scene-node]
  (map :node-id (:deps (first (g/node-value gui-scene-node :build-targets)))))

(deftest gui-fonts-test
  (test-util/with-loaded-project
    (let [gui-scene-node (test-util/resource-node project "/logic/main.gui")
          outline (g/node-value gui-scene-node :node-outline)
          gui-font-node (get-in outline [:children 3 :children 0 :node-id])
          old-font (font-resource-node project gui-font-node)
          new-font (project/get-resource-node project "/fonts/big_score.font")]
      (is (some? (g/node-value gui-font-node :font-data)))
      (is (some #{old-font} (build-targets-deps gui-scene-node)))
      (g/transact (g/set-property gui-font-node :font (g/node-value new-font :resource)))
      (is (not (some #{old-font} (build-targets-deps gui-scene-node))))
      (is (some #{new-font} (build-targets-deps gui-scene-node))))))

(deftest gui-font-validation
  (test-util/with-loaded-project
    (let [gui-scene-node (test-util/resource-node project "/logic/main.gui")
          gui-font-node (:node-id (test-util/outline gui-scene-node [3 0]))]
      (is (nil? (test-util/prop-error gui-font-node :font)))
      (doseq [v [nil (workspace/resolve-workspace-resource workspace "/not_found.font")]]
        (test-util/with-prop [gui-font-node :font v]
          (is (g/error-fatal? (test-util/prop-error gui-font-node :font))))))))

(deftest gui-text-node
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/logic/main.gui")
          outline (g/node-value node-id :node-outline)
          nodes (into {} (map (fn [item] [(:label item) (:node-id item)]) (get-in outline [:children 0 :children])))
          text-node (get nodes "hexagon_text")]
      (is (= false (g/node-value text-node :line-break))))))

(deftest gui-text-node-validation
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/logic/main.gui")
          outline (g/node-value node-id :node-outline)
          nodes (into {} (map (fn [item] [(:label item) (:node-id item)]) (get-in outline [:children 0 :children])))
          text-node (get nodes "hexagon_text")]
      (are [prop v test] (test-util/with-prop [text-node prop v]
                           (is (test (test-util/prop-error text-node prop))))
        :font ""                   g/error-fatal?
        :font "not_a_defined_font" g/error-fatal?
        :font "highscore"          nil?))))

(deftest gui-text-node-text-layout
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/logic/main.gui")
          outline (g/node-value node-id :node-outline)
          nodes (into {} (map (fn [item] [(:label item) (:node-id item)]) (get-in outline [:children 0 :children])))
          text-node (get nodes "multi_line_text")]
      (is (some? (g/node-value text-node :text-layout)))
      (is (some? (g/node-value text-node :aabb)))
      (is (some? (g/node-value text-node :text-data))))))

(defn- render-order [view]
  (let [renderables (g/node-value view :all-renderables)]
    (->> (get renderables pass/transparent)
         (map :node-id)
         (filter #(and (some? %) (g/node-instance? gui/GuiNode %)))
         (map #(g/node-value % :id))
         vec)))

(deftest gui-layers-test
  (test-util/with-loaded-project
    (let [[node-id view] (test-util/open-scene-view! project app-view "/gui/layers.gui" 16 16)]
      (is (= ["box" "pie" "box1" "text"] (render-order view)))
      (g/set-property! (gui-node node-id "box") :layer "layer1")
      (is (= ["box1" "box" "pie" "text"] (render-order view)))
      (g/set-property! (gui-node node-id "box") :layer "")
      (is (= ["box" "pie" "box1" "text"] (render-order view))))))

;; Templates

(deftest gui-templates
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/gui/scene.gui")
          original-template (test-util/resource-node project "/gui/sub_scene.gui")
          tmpl-node (gui-node node-id "sub_scene")
          path [:children 0 :node-id]]
      (is (not= (get-in (g/node-value tmpl-node :scene) path)
                (get-in (g/node-value original-template :scene) path))))))

(deftest gui-template-ids
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/gui/scene.gui")
          original-template (test-util/resource-node project "/gui/sub_scene.gui")
          tmpl-node (gui-node node-id "sub_scene")
          old-name "sub_scene/sub_box"
          new-name "sub_scene2/sub_box"]
      (is (not (nil? (gui-node node-id old-name))))
      (is (nil? (gui-node node-id new-name)))
      (g/transact (g/set-property tmpl-node :id "sub_scene2"))
      (is (not (nil? (gui-node node-id new-name))))
      (is (nil? (gui-node node-id old-name)))
      (is (true? (get-in (g/node-value tmpl-node :_declared-properties) [:properties :id :visible])))
      (is (false? (get-in (g/node-value tmpl-node :_declared-properties) [:properties :generated-id :visible])))
      (let [sub-node (gui-node node-id new-name)
            props (get (g/node-value sub-node :_declared-properties) :properties)]
        (is (= new-name (prop sub-node :generated-id)))
        (is (= (g/node-value sub-node :generated-id)
               (get-in props [:generated-id :value])))
        (is (false? (get-in props [:id :visible])))))))

(deftest gui-templates-complex-property
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/gui/scene.gui")
          sub-node (gui-node node-id "sub_scene/sub_box")]
      (let [^double alpha (prop sub-node :alpha)]
        (g/transact (g/set-property sub-node :alpha (* 0.5 alpha)))
        (is (not= alpha (prop sub-node :alpha)))
        (g/transact (g/clear-property sub-node :alpha))
        (is (= alpha (prop sub-node :alpha)))))))

(deftest gui-template-hierarchy
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/gui/super_scene.gui")
          sub-node (gui-node node-id "scene/sub_scene/sub_box")]
      (is (not= nil sub-node))
      (let [template (gui-node node-id "scene/sub_scene")
            resource (workspace/find-resource workspace "/gui/sub_scene.gui")]
        (is (= resource (get-in (g/node-value template :_properties) [:properties :template :value :resource])))
        (is (true? (get-in (g/node-value template :_properties) [:properties :template :read-only?]))))
      (let [template (gui-node node-id "scene")
            overrides (get-in (g/node-value template :_properties) [:properties :template :value :overrides])]
        (is (contains? overrides "sub_scene/sub_box"))
        (is (false? (get-in (g/node-value template :_properties) [:properties :template :read-only?])))))))

(deftest gui-template-selection
  (test-util/with-loaded-project
    (let [node-id (test-util/open-tab! project app-view "/gui/super_scene.gui")
          tmpl-node (gui-node node-id "scene/sub_scene")]
      (app-view/select! app-view [tmpl-node])
      (let [props (g/node-value app-view :selected-node-properties)]
        (is (not (empty? (keys props))))))))

(deftest gui-template-set-leak
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/gui/scene.gui")
          sub-node (test-util/resource-node project "/gui/sub_scene.gui")
          tmpl-node (gui-node node-id "sub_scene")]
      (is (= 1 (count (g/overrides sub-node))))
      (g/transact (g/set-property tmpl-node :template {:resource (workspace/find-resource workspace "/gui/layers.gui") :overrides {}}))
      (is (= 0 (count (g/overrides sub-node)))))))

(defn- options [node-id prop]
  (mapv second (get-in (g/node-value node-id :_properties) [:properties prop :edit-type :options])))

(deftest gui-template-dynamics
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/gui/super_scene.gui")
          box (gui-node node-id "scene/box")
          text (gui-node node-id "scene/text")]
      (is (= ["" "main/particle_blob" "main_super/particle_blob"] (options box :texture)))
      (is (= ["" "layer"] (options text :layer)))
      (is (= ["default_font" "default_font_super"] (options text :font)))
      (g/transact (g/set-property text :layer "layer"))
      (let [l (gui-layer node-id "layer")]
        (g/transact (g/set-property l :name "new-name"))
        (is (= "new-name" (prop text :layer)))))))

(deftest gui-template-box-overrides
  (test-util/with-loaded-project
    (let [scene-node-id (test-util/resource-node project "/gui/scene.gui")
          sub-scene-node-id (test-util/resource-node project "/gui/sub_scene.gui")
          box (gui-node sub-scene-node-id "sub_box")
          or-box (gui-node scene-node-id "sub_scene/sub_box")]
      (doseq [[p v] {:texture "main/particle_blob" :size [200.0 150.0 0.0]}]
        (is (not= (g/node-value box p) (g/node-value or-box p)))
        (is (= (g/node-value or-box p) v))))))

(deftest gui-template-overrides-remain-after-external-template-change
  (test-util/with-scratch-project "test/resources/gui_project"
    (letfn [(select-labels [node-id labels]
              (g/with-auto-evaluation-context evaluation-context
                (into {}
                      (map (fn [label]
                             (pair label
                                   (g/node-value node-id label evaluation-context))))
                      labels)))

            (select-gui-node-labels [gui-resource gui-node-name labels]
              (-> (project/get-resource-node project gui-resource)
                  (gui-node gui-node-name)
                  (select-labels labels)))]

      (let [button-gui-resource (workspace/find-resource workspace "/gui/template/button.gui")
            panel-gui-resource (workspace/find-resource workspace "/gui/template/panel.gui")
            window-gui-resource (workspace/find-resource workspace "/gui/template/window.gui")
            panel-box-props-before (select-gui-node-labels panel-gui-resource "button/box" [:color :layer :texture])
            window-box-props-before (select-gui-node-labels window-gui-resource "panel/button/box" [:color :layer :texture])]

        (testing "Overrides should exist before external change."
          (is (= {:color [0.0 1.0 0.0 1.0]
                  :layer "panel_layer"
                  :texture "panel_texture/button_checkered"}
                 panel-box-props-before))
          (is (= {:color [0.0 0.0 1.0 1.0]
                  :layer "window_layer"
                  :texture "window_texture/button_cloudy"}
                 window-box-props-before)))

        ;; Simulate external changes to 'button.gui'.
        (let [modified-button-gui-content
              (-> button-gui-resource
                  (slurp)
                  (str/replace "max_nodes: 123" "max_nodes: 100"))]
          (test-support/spit-until-new-mtime button-gui-resource modified-button-gui-content)
          (workspace/resource-sync! workspace))

        (testing "Overrides should remain after external change."
          (let [panel-box-props-after (select-gui-node-labels panel-gui-resource "button/box" [:color :layer :texture])
                window-box-props-after (select-gui-node-labels window-gui-resource "panel/button/box" [:color :layer :texture])]
            (is (= panel-box-props-before panel-box-props-after))
            (is (= window-box-props-before window-box-props-after))))))))

(defn- strip-scene [scene]
  (-> scene
      (select-keys [:node-id :children :renderable])
      (update :children (fn [c] (mapv #(strip-scene %) c)))
      (update-in [:renderable :user-data] select-keys [:color])
      (update :renderable select-keys [:user-data :tags])))

(defn- scene-by-nid [root-id node-id]
  (let [scene (g/node-value root-id :scene)
        scenes (into {}
                     (comp
                       ;; we're not interested in the outline subscenes for gui nodes
                       (remove (fn [scene] (contains? (get-in scene [:renderable :tags]) :outline)))
                       (map (fn [s] [(:node-id s) s])))
                     (tree-seq fn/constantly-true :children (strip-scene scene)))]
    (scenes node-id)))

(deftest gui-template-alpha
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/gui/super_scene.gui")
          scene-fn (comp (partial scene-by-nid node-id) (partial gui-node node-id))]
      (is (= 1.0 (get-in (scene-fn "scene/box") [:renderable :user-data :color 3])))
      (g/transact
        (concat
          (g/set-property (gui-node node-id "scene") :alpha 0.5)))
      (is (= 0.5 (get-in (scene-fn "scene/box") [:renderable :user-data :color 3]))))))

(deftest gui-template-reload
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/gui/super_scene.gui")
          template (gui-node node-id "scene")
          box (gui-node node-id "scene/box")]
      (g/transact (g/set-property box :position [-100.0 0.0 0.0]))
      (is (= -100.0 (get-in (g/node-value template :_properties) [:properties :template :value :overrides "box" :position 0])))
      (use 'editor.gui :reload)
      (is (= -100.0 (get-in (g/node-value template :_properties) [:properties :template :value :overrides "box" :position 0]))))))

(deftest gui-template-add
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/gui/scene.gui")
          super (test-util/resource-node project "/gui/super_scene.gui")
          parent (:node-id (test-util/outline node-id [0]))
          new-tmpl (gui/add-gui-node! project node-id parent :type-template 0 (fn [node-ids] (app-view/select app-view node-ids)))
          super-template (gui-node super "scene")]
      (is (= new-tmpl (gui-node node-id "template")))
      (is (not (contains? (:overrides (prop super-template :template)) "template/sub_box")))
      (prop! new-tmpl :template {:resource (workspace/resolve-workspace-resource workspace "/gui/sub_scene.gui") :overrides {}})
      (is (contains? (:overrides (prop super-template :template)) "template/sub_box")))))

(deftest gui-template-overrides-anchors
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/gui/scene.gui")
          box (gui-node node-id "sub_scene/sub_box")]
      (is (= :xanchor-left (g/node-value box :x-anchor))))))

;; Layouts

(deftest gui-layout
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/gui/scene.gui")]
      (is (= ["Landscape"] (map :name (:layouts (g/node-value node-id :save-value))))))))

(defn add-layout! [project app-view scene name]
  (let [parent (g/node-value scene :layouts-node)
        user-data {:scene scene :parent parent :display-profile name :handler-fn gui/add-layout-handler}]
    (test-util/handler-run :edit.add-embedded-component [{:name :workbench :env {:selection [parent] :project project :user-data user-data :app-view app-view}}] user-data)))

(defn- run-add-gui-node! [project scene app-view parent node-type custom-type]
  (let [user-data {:scene scene :parent parent :node-type node-type :custom-type custom-type :handler-fn gui/add-gui-node-handler}]
    (test-util/handler-run :edit.add-embedded-component [{:name :workbench :env {:selection [parent] :project project :user-data user-data :app-view app-view}}] user-data)))

(defn set-visible-layout! [scene layout]
  (g/transact (g/set-property scene :visible-layout layout)))

(defmacro with-visible-layout! [scene layout & body]
  `(let [scene# ~scene
         layout# ~layout
         prev-layout# (g/node-value scene# :visible-layout)]
     (try
       (set-visible-layout! scene# layout#)
       ~@body
       (finally
         (set-visible-layout! scene# prev-layout#)))))

(deftest gui-layout-active
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/gui/layouts.gui")
          box (gui-node node-id "box")
          box-default-pos (g/valid-node-value box :position)
          dims (g/valid-node-value node-id :scene-dims)]
      (set-visible-layout! node-id "Landscape")
      (is (identical? box (gui-node node-id "box")))
      (is (= "Landscape" (:current-layout (g/valid-node-value box :trivial-gui-scene-info))))
      (let [box-landscape-pos (g/valid-node-value box :position)]
        (is (and box-landscape-pos (not= box-default-pos box-landscape-pos))))
      (let [new-dims (g/valid-node-value node-id :scene-dims)]
        (is (and new-dims (not= dims new-dims)))))))

(deftest gui-layout-add
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/gui/layouts.gui")
          box (gui-node node-id "box")]
      (add-layout! project app-view node-id "Portrait")
      (set-visible-layout! node-id "Portrait")
      (is (identical? box (gui-node node-id "box")))
      (is (= "Portrait" (:current-layout (g/valid-node-value box :trivial-gui-scene-info)))))))

(deftest gui-layout-add-node
  (test-util/with-loaded-project
    (let [scene (test-util/resource-node project "/gui/layouts.gui")]
      (add-layout! project app-view scene "Portrait")
      (set-visible-layout! scene "Portrait")
      (let [node-tree (g/node-value scene :node-tree)]
        (is (= #{"box"} (set (map :label (:children (test-util/outline scene [0]))))))
        (run-add-gui-node! project scene app-view node-tree :type-box 0)
        (is (= #{"box" "box1"} (set (map :label (:children (test-util/outline scene [0]))))))))))

(defn- gui-text [scene id]
  (-> (gui-node scene id)
      (g/node-value :text)))

(deftest gui-layout-template
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/gui/super_scene.gui")]
      (testing "regular layout override, through templates"
        (is (= "Test" (gui-text node-id "scene/text")))
        (set-visible-layout! node-id "Landscape")
        (is (= "Testing Text" (gui-text node-id "scene/text"))))
      (testing "scene generation"
        (is (= {:width 1280 :height 720}
               (g/node-value node-id :scene-dims)))
        (set-visible-layout! node-id "Portrait")
        (is (= {:width 720 :height 1280}
               (g/node-value node-id :scene-dims)))))))

(deftest gui-legacy-alpha
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/gui/legacy_alpha.gui")
          box (gui-node node-id "box")
          text (gui-node node-id "text")]
      (is (= 0.5 (g/node-value box :alpha)))
      (is (every? #(= 0.5 (g/node-value text %)) [:alpha :outline-alpha :shadow-alpha])))))

(deftest set-gui-layout
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/gui/layouts.gui")
          gui-resource (g/node-value node-id :resource)
          context (handler/->context :workbench {:active-resource gui-resource :project project})
          options (test-util/handler-options :scene.set-gui-layout [context] nil)
          options-by-label (zipmap (map :label options) options)
          default-label (localization/message "gui.layout.default")]
      (is (= [default-label "Landscape"] (map :label options)))
      (is (= (get options-by-label default-label) (test-util/handler-state :scene.set-gui-layout [context] nil)))
      (g/set-property! node-id :visible-layout "Landscape")
      (is (= (get options-by-label "Landscape") (test-util/handler-state :scene.set-gui-layout [context] nil))))))

(deftest paste-gui-resource-test
  (test-util/with-loaded-project "test/resources/gui_project"
    (let [make-restore-point! #(test-util/make-graph-reverter (project/graph project))
          scene (test-util/resource-node project "/gui/resources/button.gui")

          check!
          (fn check! [resource-name gui-resources-node-fn shape-name shape-resource-prop-kw expected-resource-choices]
            (testing (format "Pasting %s resource" resource-name)
              (let [resources-node (gui-resources-node-fn scene)
                    resource-node (gui-resource-node gui-resources-node-fn scene resource-name)
                    shape-node (gui-node scene shape-name)]
                (is (some? resources-node))
                (is (some? resource-node))
                (is (some? shape-node))
                (let [shape-resource-before (g/node-value shape-node shape-resource-prop-kw)]
                  (with-open [_ (make-restore-point!)]
                    (test-util/outline-duplicate! project resources-node [0])
                    (is (= shape-resource-before (g/node-value shape-node shape-resource-prop-kw))
                        "Shape should still refer to the original resource name.")
                    (is (= expected-resource-choices
                           (property-value-choices shape-node shape-resource-prop-kw))
                        "The pasted resource should get a unique name among the selectable resources."))))))]
      (check!
        "button_font" gui-fonts "text" :font
        ["button_font"
         "button_font1"])
      (check!
        "button_layer" gui-layers "box" :layer
        [""
         "button_layer"
         "button_layer1"])
      (check!
        "button_material" gui-materials "box" :material
        [""
         "button_material"
         "button_material1"])
      (check!
        "button_particlefx" gui-particlefx-resources "particlefx" :particlefx
        ["button_particlefx"
         "button_particlefx1"])
      (check!
        "button_spinescene" gui-spine-scenes "spine" :spine-scene
        ["button_spinescene"
         "button_spinescene1"])
      (check!
        "button_texture" gui-textures "box" :texture
        [""
         "button_texture/button_striped"
         "button_texture1/button_striped"]))))

(deftest rename-referenced-gui-resource
  (test-util/with-loaded-project
    (let [make-restore-point! #(test-util/make-graph-reverter (project/graph project))
          scene (test-util/resource-node project "/gui_resources/gui_resources.gui")]
      (are [resource-id res-fn shape-id res-label new-name expected-name expected-choices]
        (testing (format "Renaming %s resource updates references" resource-id)
          (let [res-node (res-fn scene resource-id)
                shape-node (gui-node scene shape-id)]
            (is (some? res-node))
            (is (some? shape-node))
            (with-open [_ (make-restore-point!)]
              (g/set-property! res-node :name new-name)
              (is (= expected-name (g/node-value shape-node res-label)))
              (is (= expected-choices (property-value-choices shape-node res-label)))

              (testing "Reference remains updated after resource deletion"
                (g/delete-node! res-node)
                (is (= expected-name (g/node-value shape-node res-label)))))))
        "font" gui-font "text" :font "renamed_font" "renamed_font" ["renamed_font"]
        "layer" gui-layer "pie" :layer "renamed_layer" "renamed_layer" ["" "renamed_layer"]
        "texture" gui-texture "box" :texture "renamed_texture" "renamed_texture/particle_blob" ["" "renamed_texture/particle_blob"]
        "particlefx" gui-particlefx-resource "particlefx" :particlefx "renamed_particlefx" "renamed_particlefx" ["renamed_particlefx"]))))

(deftest rename-referenced-gui-resource-in-template
  ;; Note: This test does not verify that override values in the outer scene
  ;; that refer to resources in the template scene are updated after the rename.
  ;; This is covered by template-layout-resource-rename-test below.
  (test-util/with-loaded-project
    (let [make-restore-point! #(test-util/make-graph-reverter (project/graph project))
          template-scene (test-util/resource-node project "/gui_resources/gui_resources.gui")
          scene (test-util/resource-node project "/gui_resources/uses_gui_resources.gui")]
      (are [resource-id res-fn shape-id res-label new-name expected-name expected-choices expected-tmpl-choices]
        (let [tmpl-res-node (res-fn template-scene resource-id)
              tmpl-shape-node (gui-node template-scene shape-id)
              shape-node (gui-node scene (str "gui_resources/" shape-id))]
          (is (some? tmpl-res-node))
          (is (some? tmpl-shape-node))
          (is (some? shape-node))
          (testing (format "Renaming %s in template scene" resource-id)
            (with-open [_ (make-restore-point!)]
              (g/set-property! tmpl-res-node :name new-name)
              (is (= expected-name (g/node-value tmpl-shape-node res-label)))
              (is (= expected-name (g/node-value shape-node res-label)))
              (is (= expected-tmpl-choices (property-value-choices tmpl-shape-node res-label)))
              (is (= expected-choices (property-value-choices shape-node res-label)))
              (is (false? (g/property-overridden? shape-node res-label))))))
        "font" gui-font "text" :font "renamed_font" "renamed_font" ["renamed_font"] ["renamed_font"]
        "layer" gui-layer "pie" :layer "renamed_layer" "renamed_layer" [""] ["" "renamed_layer"]
        "texture" gui-texture "box" :texture "renamed_texture" "renamed_texture/particle_blob" ["" "renamed_texture/particle_blob"] ["" "renamed_texture/particle_blob"]
        "particlefx" gui-particlefx-resource "particlefx" :particlefx "renamed_particlefx" "renamed_particlefx" ["renamed_particlefx"] ["renamed_particlefx"]))))

(deftest rename-referenced-gui-resource-in-outer-scene
  (test-util/with-loaded-project
    (let [make-restore-point! #(test-util/make-graph-reverter (project/graph project))
          template-scene (test-util/resource-node project "/gui_resources/gui_resources.gui")
          scene (test-util/resource-node project "/gui_resources/replaces_gui_resources.gui")]
      (are [resource-id res-fn shape-id res-label new-name expected-name expected-tmpl-name expected-choices]
        (let [res-node (res-fn scene (str "replaced_" resource-id))
              tmpl-res-node (res-fn template-scene resource-id)
              shape-node (gui-node scene (str "gui_resources/" shape-id))
              tmpl-shape-node (gui-node template-scene shape-id)
              tmpl-node (gui-node scene "gui_resources")]
          (is (some? res-node))
          (is (some? tmpl-res-node))
          (is (some? shape-node))
          (is (some? tmpl-shape-node))
          (testing (format "Renaming %s in outer scene" resource-id)
            (with-open [_ (make-restore-point!)]
              (g/set-property! res-node :name new-name)
              (is (= expected-tmpl-name (g/node-value tmpl-shape-node res-label)))
              (is (= expected-name (g/node-value shape-node res-label)))
              (is (= expected-choices (property-value-choices shape-node res-label)))

              (testing "Reference remains updated after resource deletion"
                (g/delete-node! res-node)
                (is (= expected-name (g/node-value shape-node res-label)))))))
        "font" gui-font "text" :font "renamed_font" "renamed_font" "font" ["font" "renamed_font"]
        "layer" gui-layer "pie" :layer "renamed_layer" "renamed_layer" "layer" ["" "renamed_layer"]
        "texture" gui-texture "box" :texture "renamed_texture" "renamed_texture/particle_blob" "texture/particle_blob" ["" "renamed_texture/particle_blob" "texture/particle_blob"]
        "particlefx" gui-particlefx-resource "particlefx" :particlefx "renamed_particlefx" "renamed_particlefx" "particlefx" ["particlefx" "renamed_particlefx"]))))

(defn- add-font! [scene name resource]
  (first
    (g/tx-nodes-added
      (g/transact
        (gui/add-font scene (g/node-value scene :fonts-node) resource name)))))

(defn- add-layer! [scene name index]
  (first
    (g/tx-nodes-added
      (g/transact
        (gui/add-layer scene (g/node-value scene :layers-node) name index nil)))))

(defn- add-texture! [scene name resource]
  (first
    (g/tx-nodes-added
      (g/transact
        (gui/add-texture scene (g/node-value scene :textures-node) resource name)))))

(defn- add-particlefx-resource! [scene name resource]
  (first
    (g/tx-nodes-added
      (g/transact
        (gui/add-particlefx-resource scene (g/node-value scene :particlefx-resources-node) resource name)))))

(deftest introduce-missing-referenced-gui-resource
  (test-util/with-loaded-project
    (let [[workspace project _app-view] (test-util/setup! world)
          make-restore-point! #(test-util/make-graph-reverter (project/graph project))
          scene (test-util/resource-node project "/gui_resources/broken_gui_resources.gui")
          shapes {:box (gui-node scene "box")
                  :pie (gui-node scene "pie")
                  :text (gui-node scene "text")
                  :particlefx (gui-node scene "particlefx")}]
      (is (every? (comp some? val) shapes))

      (testing "Introduce missing referenced font"
        (with-open [_ (make-restore-point!)]
          (let [font-path "/fonts/highscore.font"
                font-resource (test-util/resource workspace font-path)
                font-resource-node (test-util/resource-node project font-path)
                after-font-data (g/node-value font-resource-node :font-data)]
            (is (not= after-font-data (g/node-value (:text shapes) :font-data)))
            (add-font! scene (g/node-value (:text shapes) :font) font-resource)
            (is (= after-font-data (g/node-value (:text shapes) :font-data))))))

      (testing "Introduce missing referenced layer"
        (with-open [_ (make-restore-point!)]
          (is (nil? (g/node-value (:pie shapes) :layer-index)))
          (add-layer! scene (g/node-value (:pie shapes) :layer) 0)
          (is (= 0 (g/node-value (:pie shapes) :layer-index)))))

      (testing "Introduce missing referenced texture"
        (with-open [_ (make-restore-point!)]
          (let [texture-path "/gui/gui.atlas"
                texture-resource (test-util/resource workspace texture-path)
                texture-resource-node (test-util/resource-node project texture-path)
                [missing-texture-name anim-name] (str/split (g/node-value (:box shapes) :texture) #"/")
                after-anim-data (get (g/node-value texture-resource-node :anim-data) anim-name)]
            (is (string? (not-empty missing-texture-name)))
            (is (string? (not-empty anim-name)))
            (is (some? after-anim-data))
            (is (not= after-anim-data (g/node-value (:box shapes) :anim-data)))
            (add-texture! scene missing-texture-name texture-resource)
            (is (= after-anim-data (g/node-value (:box shapes) :anim-data))))))

      (testing "Introduce missing referenced particlefx"
        (with-open [_ (make-restore-point!)]
          (let [particlefx-path "/particlefx/default.particlefx"
                particlefx-resource (test-util/resource workspace particlefx-path)
                particlefx-resource-node (test-util/resource-node project particlefx-path)]
            (is (nil? (g/node-value (:particlefx shapes) :source-scene)))
            (add-particlefx-resource! scene (g/node-value (:particlefx shapes) :particlefx) particlefx-resource)
            (is (some? (g/node-value (:particlefx shapes) :source-scene)))))))))

(deftest introduce-missing-referenced-gui-resource-in-template
  (test-util/with-loaded-project
    (let [[workspace project _app-view] (test-util/setup! world)
          make-restore-point! #(test-util/make-graph-reverter (project/graph project))
          template-scene (test-util/resource-node project "/gui_resources/broken_gui_resources.gui")
          template-shapes {:box (gui-node template-scene "box")
                           :pie (gui-node template-scene "pie")
                           :text (gui-node template-scene "text")
                           :particlefx (gui-node template-scene "particlefx")}
          scene (test-util/resource-node project "/gui_resources/uses_broken_gui_resources.gui")
          shapes {:box (gui-node scene "gui_resources/box")
                  :pie (gui-node scene "gui_resources/pie")
                  :text (gui-node scene "gui_resources/text")
                  :particlefx (gui-node scene "gui_resources/particlefx")}]
      (is (every? (comp some? val) template-shapes))
      (is (every? (comp some? val) shapes))

      (testing "Introduce missing referenced font in template scene"
        (with-open [_ (make-restore-point!)]
          (let [font-path "/fonts/highscore.font"
                font-resource (test-util/resource workspace font-path)
                font-resource-node (test-util/resource-node project font-path)
                after-font-data (g/node-value font-resource-node :font-data)]
            (is (not= after-font-data (g/node-value (:text template-shapes) :font-data)))
            (is (not= after-font-data (g/node-value (:text shapes) :font-data)))
            (add-font! template-scene (g/node-value (:text template-shapes) :font) font-resource)
            (is (= after-font-data (g/node-value (:text template-shapes) :font-data)))
            (is (= after-font-data (g/node-value (:text shapes) :font-data))))))

      (testing "Introduce missing referenced layer in template scene"
        (with-open [_ (make-restore-point!)]
          (let [template-pie (:pie template-shapes)
                imported-pie (:pie shapes)
                template-layer (g/node-value template-pie :layer)]
            (is (= template-layer (g/node-value imported-pie :layer))) ; Layer property is inherited from template.
            (is (nil? (g/node-value template-pie :layer-index)))
            (is (nil? (g/node-value imported-pie :layer-index)))
            (add-layer! template-scene template-layer 0)
            (is (= 0 (g/node-value template-pie :layer-index)))
            (is (nil? (g/node-value imported-pie :layer-index))) ; Layers are isolated between scenes. The imported pie inherits the layer property, but there is still no matching layer in its scene.
            (add-layer! scene template-layer 0)
            (is (= 0 (g/node-value imported-pie :layer-index))))))

      (testing "Introduce missing referenced texture in template scene"
        (with-open [_ (make-restore-point!)]
          (let [texture-path "/gui/gui.atlas"
                texture-resource (test-util/resource workspace texture-path)
                texture-resource-node (test-util/resource-node project texture-path)
                [missing-texture-name anim-name] (str/split (g/node-value (:box template-shapes) :texture) #"/")
                after-anim-data (get (g/node-value texture-resource-node :anim-data) anim-name)]
            (is (string? (not-empty missing-texture-name)))
            (is (string? (not-empty anim-name)))
            (is (some? after-anim-data))
            (is (not= after-anim-data (g/node-value (:box template-shapes) :anim-data)))
            (is (not= after-anim-data (g/node-value (:box shapes) :anim-data)))
            (add-texture! template-scene missing-texture-name texture-resource)
            (is (= after-anim-data (g/node-value (:box template-shapes) :anim-data)))
            (is (= after-anim-data (g/node-value (:box shapes) :anim-data))))))

      (testing "Introduce missing referenced particlefx in template scene"
        (with-open [_ (make-restore-point!)]
          (let [particlefx-path "/particlefx/default.particlefx"
                particlefx-resource (test-util/resource workspace particlefx-path)
                particlefx-resource-node (test-util/resource-node project particlefx-path)]
            (is (nil? (g/node-value (:particlefx template-shapes) :source-scene)))
            (is (nil? (g/node-value (:particlefx shapes) :source-scene)))
            (add-particlefx-resource! template-scene (g/node-value (:particlefx template-shapes) :particlefx) particlefx-resource)
            (is (some? (g/node-value (:particlefx template-shapes) :source-scene)))
            (is (some? (g/node-value (:particlefx shapes) :source-scene)))))))))

(defn- outline-order [parent]
  ;; tree-seq does a pre-order traversal of the nodes. Sibling nodes will get mapped to
  ;; increasing indices/ordinals.
  (zipmap (map :label (tree-seq :children :children (g/node-value parent :node-outline)))
          (range)))

(defn- save-order [scene]
  (let [save-value (g/node-value scene :save-value)]
    (zipmap (map :id (:nodes save-value)) (range))))

(defn- build-order [scene]
  (let [pb (-> (g/node-value scene :build-targets) (get 0) :user-data :pb)]
    (zipmap (map :id (:nodes pb)) (range))))

(defmacro check-order [scene rel lhs-label rhs-label]
  `(let [~'outline-order (outline-order (g/node-value ~scene :node-tree))
         ~'save-order (save-order ~scene)
         ~'build-order (build-order ~scene)]
     (is (~rel (~'outline-order ~lhs-label) (~'outline-order ~rhs-label)))
     (is (~rel (~'save-order ~lhs-label) (~'save-order ~rhs-label)))
     (is (~rel (~'build-order ~lhs-label) (~'build-order ~rhs-label)))))

(defn move-child-node! [node-id offset]
  (#'gui/move-child-node! node-id offset))

(defn- scene-gui-node-map [scene]
  (g/node-value scene :node-ids))

(deftest reorder-child-nodes
  ;; /gui/reorder.gui
  ;; .
  ;; ├── box1
  ;; │   ├── text1
  ;; │   └── text2
  ;; └── box2
  (test-util/with-loaded-project
    (let [[workspace project _] (test-util/setup! world)
          scene (project/get-resource-node project "/gui/reorder.gui")
          id-map (scene-gui-node-map scene)]

      (check-order scene < "box1" "box2")
      (check-order scene < "text1" "text2")

      ;; move up box2
      (move-child-node! (id-map "box2") -1)
      (check-order scene < "box2" "box1")

      ;; move up box1 (restore order)
      (move-child-node! (id-map "box1") -1)
      (check-order scene < "box1" "box2")



      ;; move up text2
      (move-child-node! (id-map "text2") -1)
      (check-order scene < "text2" "text1")

      ;; move up text1 (restore order)
      (move-child-node! (id-map "text1") -1)
      (check-order scene < "text1" "text2")



      ;; move down box1
      (move-child-node! (id-map "box1") 1)
      (check-order scene < "box2" "box1")

      ;; move down box2 (restore order)
      (move-child-node! (id-map "box2") 1)
      (check-order scene < "box1" "box2")


      ;; move down text1
      (move-child-node! (id-map "text1") 1)
      (check-order scene < "text2" "text1")

      ;; move-down text2 (restore order)
      (move-child-node! (id-map "text2") 1)
      (check-order scene < "text1" "text2"))))

(deftest reordering-does-not-wipe-layout-overrides
  (test-util/with-loaded-project
    (let [make-restore-point! #(test-util/make-graph-reverter (project/graph project))
          scene (project/get-resource-node project "/gui/reorder.gui")
          id-map (scene-gui-node-map scene)
          layouts (g/node-feeding-into scene :layout-names)
          [landscape portrait] (map first (g/sources-of layouts :names))]

      ;; sanity
      (is (= "Landscape" (g/node-value landscape :name)))
      (is (= "Portrait" (g/node-value portrait :name)))

      (with-visible-layout! scene ""
        (is (= [0.0 0.0 0.0] (g/node-value (id-map "box1") :position)))
        (is (= [0.0 0.0 0.0] (g/node-value (id-map "box2") :position)))
        (is (= [-45.0 0.0 0.0] (g/node-value (id-map "text1") :position)))
        (is (= [45.0 0.0 0.0] (g/node-value (id-map "text2") :position))))

      ;; before move
      (with-visible-layout! scene "Landscape"
        (is (= [300.0 400.0 0.0] (g/node-value (id-map "box1") :position)))
        (is (= [1000.0 400.0 0.0] (g/node-value (id-map "box2") :position)))
        (is (= [45.0 0.0 0.0] (g/node-value (id-map "text1") :position))) ;; text1 & text2 swapped places in landscape vs default layout
        (is (= [-45.0 0.0 0.0] (g/node-value (id-map "text2") :position))))

      (with-visible-layout! scene "Portrait"
        (is (= [360.0 900.0 0.0] (g/node-value (id-map "box1") :position)))
        (is (= [360.0 350.0 0.0] (g/node-value (id-map "box2") :position)))
        (is (= [0.0 -45.0 0.0] (g/node-value (id-map "text1") :position))) ;; text1 & text2 vertically stacked in portrait
        (is (= [0.0 45.0 0.0] (g/node-value (id-map "text2") :position))))

      ;; reordering nodes should not change overrides for the layouts at all
      (doseq [layout-during-reorder ["" "Landscape" "Portrait"]]
        (with-open [_ (make-restore-point!)]
          (with-visible-layout! scene layout-during-reorder
            (move-child-node! (id-map "box2") -1)
            (move-child-node! (id-map "text1") 1))

          (with-visible-layout! scene ""
            (is (= [0.0 0.0 0.0] (g/node-value (id-map "box1") :position)))
            (is (= [0.0 0.0 0.0] (g/node-value (id-map "box2") :position)))
            (is (= [-45.0 0.0 0.0] (g/node-value (id-map "text1") :position)))
            (is (= [45.0 0.0 0.0] (g/node-value (id-map "text2") :position))))

          (with-visible-layout! scene "Landscape"
            (is (= [300.0 400.0 0.0] (g/node-value (id-map "box1") :position)))
            (is (= [1000.0 400.0 0.0] (g/node-value (id-map "box2") :position)))
            (is (= [45.0 0.0 0.0] (g/node-value (id-map "text1") :position)))
            (is (= [-45.0 0.0 0.0] (g/node-value (id-map "text2") :position))))

          (with-visible-layout! scene "Portrait"
            (is (= [360.0 900.0 0.0] (g/node-value (id-map "box1") :position)))
            (is (= [360.0 350.0 0.0] (g/node-value (id-map "box2") :position)))
            (is (= [0.0 -45.0 0.0] (g/node-value (id-map "text1") :position)))
            (is (= [0.0 45.0 0.0] (g/node-value (id-map "text2") :position)))))))))

(def ^:private template-layout-gui-scene-proj-paths
  ["/gui/template_layout/button.gui"
   "/gui/template_layout/button_l.gui"
   "/gui/template_layout/button_lp.gui"
   "/gui/template_layout/panel_button.gui"
   "/gui/template_layout/panel_button_l.gui"
   "/gui/template_layout/panel_button_lp.gui"
   "/gui/template_layout/panel_l_button.gui"
   "/gui/template_layout/panel_l_button_l.gui"
   "/gui/template_layout/panel_l_button_lp.gui"
   "/gui/template_layout/panel_lp_button.gui"
   "/gui/template_layout/panel_lp_button_l.gui"
   "/gui/template_layout/panel_lp_button_lp.gui"])

(deftest data-unaffected-by-visible-layout
  (test-util/with-loaded-project "test/resources/gui_project"
    (doseq [proj-path template-layout-gui-scene-proj-paths]
      (testing proj-path
        (let [scene (project/get-resource-node project proj-path)

              scene-output-permutations
              (coll/transfer ["" "Landscape" "Portrait"] []
                (map (fn [layout]
                       (with-visible-layout! scene layout
                         (g/with-auto-evaluation-context evaluation-context
                           (coll/transfer [:build-targets :save-value] {}
                             (map (fn [output-label]
                                    (let [output-value (g/valid-node-value scene output-label evaluation-context)]
                                      (assert (some? output-value))
                                      (pair output-label output-value)))))))))
                (distinct))]

          (is (= [(first scene-output-permutations)] scene-output-permutations)
              "Changing the visible layout should not affect the data."))))))

(defn- override-node-desc? [node-desc]
  (or (:template-node-child node-desc)
      (str/includes? (:id node-desc) "/")))

(defn- make-displayed-layout->node->data [gui-scene-node-id data-fn]
  {:pre [(g/node-id? gui-scene-node-id)
         (ifn? data-fn)]}
  (let [layout-names (cons "" (g/node-value gui-scene-node-id :layout-names))
        gui-node-name->node-id (g/node-value gui-scene-node-id :node-ids)]
    (coll/transfer layout-names (sorted-map)
      (map (fn [layout-name]
             (with-visible-layout! gui-scene-node-id layout-name
               (g/with-auto-evaluation-context evaluation-context
                 (pair (if (str/blank? layout-name) "Default" layout-name)
                       (coll/transfer gui-node-name->node-id (sorted-map)
                         (keep (fn [[gui-node-name gui-node-id]]
                                 (let [data (data-fn gui-node-id evaluation-context)]
                                   (when (coll/not-empty data)
                                     (pair gui-node-name data))))))))))))))

(defn- make-visibly-overridden-layout->node->props [gui-scene-node-id]
  (make-displayed-layout->node->data
    gui-scene-node-id
    (fn data-fn [gui-node-id evaluation-context]
      (->> (g/valid-node-value gui-node-id :_properties evaluation-context)
           (vector)
           (properties/coalesce)
           (:properties)
           (into
             (if (:outline-overridden? (g/valid-node-value gui-node-id :node-outline evaluation-context))
               #{:node-outline}
               #{})
             (keep (fn [[prop-kw coalesced-property]]
                     (when (properties/overridden? coalesced-property)
                       prop-kw))))))))

(defn- make-displayed-layout->node->prop->value [gui-scene-node-id]
  (make-displayed-layout->node->data
    gui-scene-node-id
    (fn data-fn [gui-node-id evaluation-context]
      (let [node-type (g/node-type* (:basis evaluation-context) gui-node-id)
            prop-labels (g/declared-property-labels node-type)
            prop->default (in/defaults node-type)]
        (coll/transfer prop-labels (sorted-map)
          (remove #{:child-index :custom-type :generated-id :id :layout->prop->override :template :type})
          (keep (fn [prop-label]
                  (let [default-value (prop->default prop-label)
                        prop-value (g/node-value gui-node-id prop-label evaluation-context)]
                    (when (not= default-value prop-value)
                      (pair prop-label prop-value))))))))))

(defn- make-node->field->value [node-descs override-node-desc? node-desc-fn]
  (coll/transfer node-descs (sorted-map)
    (keep (fn [node-desc]
            (when-some [field->value (node-desc-fn node-desc (override-node-desc? node-desc))]
              (pair (:id node-desc) field->value))))))

(defn- make-layout->node->field->value [scene-desc node-desc-fn]
  (coll/transfer
    (:layouts scene-desc)
    (sorted-map "Default" (make-node->field->value (:nodes scene-desc) override-node-desc? node-desc-fn))
    (map (fn [layout-desc]
           (let [layout-name (:name layout-desc)]
             (pair (if (str/blank? layout-name) "Default" layout-name)
                   (make-node->field->value (:nodes layout-desc) fn/constantly-true node-desc-fn)))))))

(defn- make-built-layout->node->field->value [gui-scene-node-id]
  (let [build-targets (g/valid-node-value gui-scene-node-id :build-targets)
        scene-desc (get-in build-targets [0 :user-data :pb])]
    (make-layout->node->field->value
      scene-desc
      (fn node-desc-fn [node-desc _is-override-node-desc]
        (dissoc node-desc :custom-type :id :type)))))

(defn- make-saved-layout->node->field->value [gui-scene-node-id]
  (let [scene-desc (g/valid-node-value gui-scene-node-id :save-value)]
    (make-layout->node->field->value
      scene-desc
      (fn node-desc-fn [node-desc is-override-node-desc]
        (if-not is-override-node-desc
          (dissoc node-desc :custom-type :id :overridden-fields :template-node-child :type)
          (reduce (fn [clean-node-desc overridden-pb-field-index]
                    (let [overridden-pb-field (gui/pb-field-index->pb-field overridden-pb-field-index)]
                      (assoc clean-node-desc
                        overridden-pb-field (get node-desc overridden-pb-field (protobuf/default Gui$NodeDesc overridden-pb-field)))))
                  (select-keys node-desc [:parent])
                  (:overridden-fields node-desc)))))))

(defn has-successor?
  ([source-id+source-label target-id+target-label]
   (has-successor? (g/now) source-id+source-label target-id+target-label))
  ([basis [source-id source-label] [target-id target-label]]
   (let [graph-id (g/node-id->graph-id source-id)
         successor-endpoint-array (get-in basis [:graphs graph-id :successors source-id source-label])
         length (count successor-endpoint-array)
         target-endpoint (g/endpoint target-id target-label)]
     (loop [index 0]
       (cond
         (= index length) false
         (= target-endpoint (aget successor-endpoint-array index)) true
         :else (recur (unchecked-inc-int index)))))))

(deftest template-layout-visible-layout-property-successors-test
  (test-util/with-loaded-project "test/resources/gui_project"
    (let [referencing-scene (project/get-resource-node project "/gui/template_layout/panel_l_button_l.gui")
          referenced-scene (project/get-resource-node project "/gui/template_layout/button_l.gui")]

      ;; Add a new node to the referenced scene.
      (let [referenced-scene-node-tree (g/node-value referenced-scene :node-tree)
            added-text-props {:id "added" :font "font" :text "button default"}
            referenced-scene-text (get (scene-gui-node-map referenced-scene) "text")
            referenced-scene-added-text (gui/add-gui-node-with-props! referenced-scene referenced-scene-node-tree :type-text 0 added-text-props nil)]

        ;; Override the text property on the added node for the Landscape layout in the referenced scene.
        (with-visible-layout! referenced-scene "Landscape"
          (test-util/prop! referenced-scene-added-text :text "button landscape"))

        (testing "Current layout is reflected across referenced scenes."
          (let [referencing-scene-node-map (scene-gui-node-map referencing-scene)
                referencing-scene-button (get referencing-scene-node-map "button")
                referencing-scene-node-tree (g/node-value referencing-scene :node-tree)
                referencing-scene-referenced-scene (g/node-feeding-into referencing-scene-button :template-resource)
                referencing-scene-referenced-scene-node-tree (g/node-value referencing-scene-referenced-scene :node-tree)
                referencing-scene-referenced-scene-text (get referencing-scene-node-map "button/text")
                referencing-scene-referenced-scene-added-text (get referencing-scene-node-map "button/added")]

            (testing "Successors in referenced scene."
              (is (has-successor? [referenced-scene :visible-layout] [referenced-scene :trivial-gui-scene-info]))
              (is (has-successor? [referenced-scene :trivial-gui-scene-info] [referenced-scene-node-tree :trivial-gui-scene-info]))
              (is (has-successor? [referenced-scene-node-tree :trivial-gui-scene-info] [referenced-scene-text :trivial-gui-scene-info]))
              (is (has-successor? [referenced-scene-node-tree :trivial-gui-scene-info] [referenced-scene-added-text :trivial-gui-scene-info]))
              (is (has-successor? [referenced-scene-text :trivial-gui-scene-info] [referenced-scene-text :prop->value]))
              (is (has-successor? [referenced-scene-added-text :trivial-gui-scene-info] [referenced-scene-added-text :prop->value])))

            (testing "Successors in referencing scene."
              (is (has-successor? [referencing-scene :visible-layout] [referencing-scene :trivial-gui-scene-info]))
              (is (has-successor? [referencing-scene :trivial-gui-scene-info] [referencing-scene-node-tree :trivial-gui-scene-info]))
              (is (has-successor? [referencing-scene-node-tree :trivial-gui-scene-info] [referencing-scene-button :trivial-gui-scene-info]))
              (is (has-successor? [referencing-scene-button :template-trivial-gui-scene-info] [referencing-scene-referenced-scene :trivial-gui-scene-info]))
              (is (has-successor? [referencing-scene-referenced-scene :trivial-gui-scene-info] [referencing-scene-referenced-scene-node-tree :trivial-gui-scene-info]))
              (is (has-successor? [referencing-scene-referenced-scene-node-tree :trivial-gui-scene-info] [referencing-scene-referenced-scene-text :trivial-gui-scene-info]))
              (is (has-successor? [referencing-scene-referenced-scene-node-tree :trivial-gui-scene-info] [referencing-scene-referenced-scene-added-text :trivial-gui-scene-info]))
              (is (has-successor? [referencing-scene-referenced-scene-text :trivial-gui-scene-info] [referencing-scene-referenced-scene-text :prop->value]))
              (is (has-successor? [referencing-scene-referenced-scene-added-text :trivial-gui-scene-info] [referencing-scene-referenced-scene-added-text :prop->value])))

            (testing "Visible layout selected for referencing scene is reflected in imported nodes."
              (set-visible-layout! referencing-scene "")
              (is (= "panel default" (g/node-value referencing-scene-referenced-scene-text :text)))
              (is (= "button default" (g/node-value referencing-scene-referenced-scene-added-text :text)))

              (set-visible-layout! referencing-scene "Landscape")
              (is (= "panel landscape" (g/node-value referencing-scene-referenced-scene-text :text)))
              (is (= "button landscape" (g/node-value referencing-scene-referenced-scene-added-text :text))))))))))

(deftest template-layout-data-test
  (test-util/with-loaded-project "test/resources/gui_project"
    (let [make-restore-point! #(test-util/make-graph-reverter (project/graph project))]

      (let [proj-path "/gui/template_layout/button.gui"]
        (testing proj-path
          (with-open [_ (make-restore-point!)]
            (let [scene (project/get-resource-node project proj-path)]

              (testing "Before modifications."
                (is (= {"Default" {}}
                       (make-visibly-overridden-layout->node->props scene)))
                (is (= {"Default" {"text" {:font "font"
                                           :text "button default"}}}
                       (make-displayed-layout->node->prop->value scene)))
                (is (= {"Default" {"text" {:font "font"
                                           :text "button default"}}}
                       (make-built-layout->node->field->value scene)))
                (is (= {"Default" {"text" {:font "font"
                                           :text "button default"}}}
                       (make-saved-layout->node->field->value scene))))))))

      (let [proj-path "/gui/template_layout/button_l.gui"]
        (testing proj-path
          (with-open [_ (make-restore-point!)]
            (let [scene (project/get-resource-node project proj-path)
                  text (get (scene-gui-node-map scene) "text")]

              (testing "Before modifications."
                (is (= {"Default" {}
                        "Landscape" {"text" #{:node-outline :text}}}
                       (make-visibly-overridden-layout->node->props scene)))
                (is (= {"Default" {"text" {:font "font"
                                           :text "button default"}}
                        "Landscape" {"text" {:font "font"
                                             :text "button landscape"}}}
                       (make-displayed-layout->node->prop->value scene)))
                (is (= {"Default" {"text" {:font "font"
                                           :text "button default"}}
                        "Landscape" {"text" {:font "font"
                                             :text "button landscape"}}}
                       (make-built-layout->node->field->value scene)))
                (is (= {"Default" {"text" {:font "font"
                                           :text "button default"}}
                        "Landscape" {"text" {:text "button landscape"}}}
                       (make-saved-layout->node->field->value scene))))

              (testing "After clearing Landscape layout override."
                (with-visible-layout! scene "Landscape"
                  (test-util/prop-clear! text :text))
                (is (= {"Default" {}
                        "Landscape" {}}
                       (make-visibly-overridden-layout->node->props scene)))
                (is (= {"Default" {"text" {:font "font"
                                           :text "button default"}}
                        "Landscape" {"text" {:font "font"
                                             :text "button default"}}}
                       (make-displayed-layout->node->prop->value scene)))
                (is (= {"Default" {"text" {:font "font"
                                           :text "button default"}}
                        "Landscape" {}}
                       (make-built-layout->node->field->value scene)))
                (is (= {"Default" {"text" {:font "font"
                                           :text "button default"}}
                        "Landscape" {}}
                       (make-saved-layout->node->field->value scene))))))))

      (let [proj-path "/gui/template_layout/button_lp.gui"]
        (testing proj-path
          (with-open [_ (make-restore-point!)]
            (let [scene (project/get-resource-node project proj-path)
                  text (get (scene-gui-node-map scene) "text")]

              (testing "Before modifications."
                (is (= {"Default" {}
                        "Landscape" {"text" #{:node-outline :text}}
                        "Portrait" {"text" #{:node-outline :text}}}
                       (make-visibly-overridden-layout->node->props scene)))
                (is (= {"Default" {"text" {:font "font"
                                           :text "button default"}}
                        "Landscape" {"text" {:font "font"
                                             :text "button landscape"}}
                        "Portrait" {"text" {:font "font"
                                            :text "button portrait"}}}
                       (make-displayed-layout->node->prop->value scene)))
                (is (= {"Default" {"text" {:font "font"
                                           :text "button default"}}
                        "Landscape" {"text" {:font "font"
                                             :text "button landscape"}}
                        "Portrait" {"text" {:font "font"
                                            :text "button portrait"}}}
                       (make-built-layout->node->field->value scene)))
                (is (= {"Default" {"text" {:font "font"
                                           :text "button default"}}
                        "Landscape" {"text" {:text "button landscape"}}
                        "Portrait" {"text" {:text "button portrait"}}}
                       (make-saved-layout->node->field->value scene))))

              (testing "After clearing Landscape layout override."
                (with-visible-layout! scene "Landscape"
                  (test-util/prop-clear! text :text))
                (is (= {"Default" {}
                        "Landscape" {}
                        "Portrait" {"text" #{:node-outline :text}}}
                       (make-visibly-overridden-layout->node->props scene)))
                (is (= {"Default" {"text" {:font "font"
                                           :text "button default"}}
                        "Landscape" {"text" {:font "font"
                                             :text "button default"}}
                        "Portrait" {"text" {:font "font"
                                            :text "button portrait"}}}
                       (make-displayed-layout->node->prop->value scene)))
                (is (= {"Default" {"text" {:font "font"
                                           :text "button default"}}
                        "Landscape" {}
                        "Portrait" {"text" {:font "font"
                                            :text "button portrait"}}}
                       (make-built-layout->node->field->value scene)))
                (is (= {"Default" {"text" {:font "font"
                                           :text "button default"}}
                        "Landscape" {}
                        "Portrait" {"text" {:text "button portrait"}}}
                       (make-saved-layout->node->field->value scene))))))))

      (let [proj-path "/gui/template_layout/panel_button.gui"]
        (testing proj-path
          (with-open [_ (make-restore-point!)]
            (let [scene (project/get-resource-node project proj-path)
                  text (get (scene-gui-node-map scene) "button/text")]

              (testing "Before modifications."
                (is (= {"Default" {"button/text" #{:node-outline :text}}}
                       (make-visibly-overridden-layout->node->props scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}}
                       (make-displayed-layout->node->prop->value scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}}
                       (make-built-layout->node->field->value scene)))
                (is (= {"Default" {"button" {:template "/gui/template_layout/button.gui"}
                                   "button/text" {:parent "button"
                                                  :text "panel default"}}}
                       (make-saved-layout->node->field->value scene))))

              (testing "After clearing Default layout override."
                (with-visible-layout! scene ""
                  (test-util/prop-clear! text :text))
                (is (= {"Default" {}}
                       (make-visibly-overridden-layout->node->props scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "button default"}}}
                       (make-displayed-layout->node->prop->value scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "button default"}}}
                       (make-built-layout->node->field->value scene)))
                (is (= {"Default" {"button" {:template "/gui/template_layout/button.gui"}
                                   "button/text" {:parent "button"}}}
                       (make-saved-layout->node->field->value scene))))))))

      (let [proj-path "/gui/template_layout/panel_button_l.gui"]
        (testing proj-path
          (with-open [_ (make-restore-point!)]
            (let [scene (project/get-resource-node project proj-path)
                  text (get (scene-gui-node-map scene) "button/text")]

              (testing "Before modifications."
                (is (= {"Default" {"button/text" #{:node-outline :text}}}
                       (make-visibly-overridden-layout->node->props scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}}
                       (make-displayed-layout->node->prop->value scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}}
                       (make-built-layout->node->field->value scene)))
                (is (= {"Default" {"button" {:template "/gui/template_layout/button_l.gui"}
                                   "button/text" {:parent "button"
                                                  :text "panel default"}}}
                       (make-saved-layout->node->field->value scene))))

              (testing "After clearing Default layout override."
                (with-visible-layout! scene ""
                  (test-util/prop-clear! text :text))
                (is (= {"Default" {}}
                       (make-visibly-overridden-layout->node->props scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "button default"}}}
                       (make-displayed-layout->node->prop->value scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "button default"}}}
                       (make-built-layout->node->field->value scene)))
                (is (= {"Default" {"button" {:template "/gui/template_layout/button_l.gui"}
                                   "button/text" {:parent "button"}}}
                       (make-saved-layout->node->field->value scene))))))))

      (let [proj-path "/gui/template_layout/panel_button_lp.gui"]
        (testing proj-path
          (with-open [_ (make-restore-point!)]
            (let [scene (project/get-resource-node project proj-path)
                  text (get (scene-gui-node-map scene) "button/text")]

              (testing "Before modifications."
                (is (= {"Default" {"button/text" #{:node-outline :text}}}
                       (make-visibly-overridden-layout->node->props scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}}
                       (make-displayed-layout->node->prop->value scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}}
                       (make-built-layout->node->field->value scene)))
                (is (= {"Default" {"button" {:template "/gui/template_layout/button_lp.gui"}
                                   "button/text" {:parent "button"
                                                  :text "panel default"}}}
                       (make-saved-layout->node->field->value scene))))

              (testing "After clearing Default layout override."
                (with-visible-layout! scene ""
                  (test-util/prop-clear! text :text))
                (is (= {"Default" {}}
                       (make-visibly-overridden-layout->node->props scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "button default"}}}
                       (make-displayed-layout->node->prop->value scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "button default"}}}
                       (make-built-layout->node->field->value scene)))
                (is (= {"Default" {"button" {:template "/gui/template_layout/button_lp.gui"}
                                   "button/text" {:parent "button"}}}
                       (make-saved-layout->node->field->value scene))))))))

      (let [proj-path "/gui/template_layout/panel_l_button.gui"]
        (testing proj-path
          (with-open [_ (make-restore-point!)]
            (let [scene (project/get-resource-node project proj-path)
                  text (get (scene-gui-node-map scene) "button/text")]

              (testing "Before modifications."
                (is (= {"Default" {"button/text" #{:node-outline :text}}
                        "Landscape" {"button/text" #{:node-outline :text}}}
                       (make-visibly-overridden-layout->node->props scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "panel landscape"}}}
                       (make-displayed-layout->node->prop->value scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "panel landscape"}}}
                       (make-built-layout->node->field->value scene)))
                (is (= {"Default" {"button" {:template "/gui/template_layout/button.gui"}
                                   "button/text" {:parent "button"
                                                  :text "panel default"}}
                        "Landscape" {"button/text" {:parent "button"
                                                    :text "panel landscape"}}}
                       (make-saved-layout->node->field->value scene))))

              (testing "After clearing Landscape layout override."
                (with-visible-layout! scene "Landscape"
                  (test-util/prop-clear! text :text))
                (is (= {"Default" {"button/text" #{:node-outline :text}}
                        "Landscape" {}}
                       (make-visibly-overridden-layout->node->props scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "panel default"}}}
                       (make-displayed-layout->node->prop->value scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}
                        "Landscape" {}}
                       (make-built-layout->node->field->value scene)))
                (is (= {"Default" {"button" {:template "/gui/template_layout/button.gui"}
                                   "button/text" {:parent "button"
                                                  :text "panel default"}}
                        "Landscape" {}}
                       (make-saved-layout->node->field->value scene))))

              (testing "After clearing Landscape and Default layout overrides."
                (with-visible-layout! scene ""
                  (test-util/prop-clear! text :text))
                (is (= {"Default" {}
                        "Landscape" {}}
                       (make-visibly-overridden-layout->node->props scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "button default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "button default"}}}
                       (make-displayed-layout->node->prop->value scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "button default"}}
                        "Landscape" {}}
                       (make-built-layout->node->field->value scene)))
                (is (= {"Default" {"button" {:template "/gui/template_layout/button.gui"}
                                   "button/text" {:parent "button"}}
                        "Landscape" {}}
                       (make-saved-layout->node->field->value scene))))))))

      (let [proj-path "/gui/template_layout/panel_l_button_l.gui"]
        (testing proj-path
          (with-open [_ (make-restore-point!)]
            (let [scene (project/get-resource-node project proj-path)
                  text (get (scene-gui-node-map scene) "button/text")]

              (testing "Before modifications."
                (is (= {"Default" {"button/text" #{:node-outline :text}}
                        "Landscape" {"button/text" #{:node-outline :text}}}
                       (make-visibly-overridden-layout->node->props scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "panel landscape"}}}
                       (make-displayed-layout->node->prop->value scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "panel landscape"}}}
                       (make-built-layout->node->field->value scene)))
                (is (= {"Default" {"button" {:template "/gui/template_layout/button_l.gui"}
                                   "button/text" {:parent "button"
                                                  :text "panel default"}}
                        "Landscape" {"button/text" {:parent "button"
                                                    :text "panel landscape"}}}
                       (make-saved-layout->node->field->value scene))))

              (testing "After clearing Landscape layout override."
                (with-visible-layout! scene "Landscape"
                  (test-util/prop-clear! text :text))
                (is (= {"Default" {"button/text" #{:node-outline :text}}
                        "Landscape" {}}
                       (make-visibly-overridden-layout->node->props scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "button landscape"}}}
                       (make-displayed-layout->node->prop->value scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "button landscape"}}}
                       (make-built-layout->node->field->value scene)))
                (is (= {"Default" {"button" {:template "/gui/template_layout/button_l.gui"}
                                   "button/text" {:parent "button"
                                                  :text "panel default"}}
                        "Landscape" {}}
                       (make-saved-layout->node->field->value scene))))

              (testing "After clearing Landscape and Default layout overrides."
                (with-visible-layout! scene ""
                  (test-util/prop-clear! text :text))
                (is (= {"Default" {}
                        "Landscape" {}}
                       (make-visibly-overridden-layout->node->props scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "button default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "button landscape"}}}
                       (make-displayed-layout->node->prop->value scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "button default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "button landscape"}}}
                       (make-built-layout->node->field->value scene)))
                (is (= {"Default" {"button" {:template "/gui/template_layout/button_l.gui"}
                                   "button/text" {:parent "button"}}
                        "Landscape" {}}
                       (make-saved-layout->node->field->value scene))))))))

      (let [proj-path "/gui/template_layout/panel_l_button_lp.gui"]
        (testing proj-path
          (with-open [_ (make-restore-point!)]
            (let [scene (project/get-resource-node project proj-path)
                  text (get (scene-gui-node-map scene) "button/text")]

              (testing "Before modifications."
                (is (= {"Default" {"button/text" #{:node-outline :text}}
                        "Landscape" {"button/text" #{:node-outline :text}}}
                       (make-visibly-overridden-layout->node->props scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "panel landscape"}}}
                       (make-displayed-layout->node->prop->value scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "panel landscape"}}}
                       (make-built-layout->node->field->value scene)))
                (is (= {"Default" {"button" {:template "/gui/template_layout/button_lp.gui"}
                                   "button/text" {:parent "button"
                                                  :text "panel default"}}
                        "Landscape" {"button/text" {:parent "button"
                                                    :text "panel landscape"}}}
                       (make-saved-layout->node->field->value scene))))

              (testing "After clearing Landscape layout override."
                (with-visible-layout! scene "Landscape"
                  (test-util/prop-clear! text :text))
                (is (= {"Default" {"button/text" #{:node-outline :text}}
                        "Landscape" {}}
                       (make-visibly-overridden-layout->node->props scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "button landscape"}}}
                       (make-displayed-layout->node->prop->value scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "button landscape"}}}
                       (make-built-layout->node->field->value scene)))
                (is (= {"Default" {"button" {:template "/gui/template_layout/button_lp.gui"}
                                   "button/text" {:parent "button"
                                                  :text "panel default"}}
                        "Landscape" {}}
                       (make-saved-layout->node->field->value scene))))

              (testing "After clearing Landscape and Default layout overrides."
                (with-visible-layout! scene ""
                  (test-util/prop-clear! text :text))
                (is (= {"Default" {}
                        "Landscape" {}}
                       (make-visibly-overridden-layout->node->props scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "button default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "button landscape"}}}
                       (make-displayed-layout->node->prop->value scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "button default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "button landscape"}}}
                       (make-built-layout->node->field->value scene)))
                (is (= {"Default" {"button" {:template "/gui/template_layout/button_lp.gui"}
                                   "button/text" {:parent "button"}}
                        "Landscape" {}}
                       (make-saved-layout->node->field->value scene))))))))

      (let [proj-path "/gui/template_layout/panel_lp_button.gui"]
        (testing proj-path
          (with-open [_ (make-restore-point!)]
            (let [scene (project/get-resource-node project proj-path)
                  text (get (scene-gui-node-map scene) "button/text")]

              (testing "Before modifications."
                (is (= {"Default" {"button/text" #{:node-outline :text}}
                        "Landscape" {"button/text" #{:node-outline :text}}
                        "Portrait" {"button/text" #{:node-outline :text}}}
                       (make-visibly-overridden-layout->node->props scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "panel landscape"}}
                        "Portrait" {"button/text" {:font "font"
                                                   :text "panel portrait"}}}
                       (make-displayed-layout->node->prop->value scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "panel landscape"}}
                        "Portrait" {"button/text" {:font "font"
                                                   :text "panel portrait"}}}
                       (make-built-layout->node->field->value scene)))
                (is (= {"Default" {"button" {:template "/gui/template_layout/button.gui"}
                                   "button/text" {:parent "button"
                                                  :text "panel default"}}
                        "Landscape" {"button/text" {:parent "button"
                                                    :text "panel landscape"}}
                        "Portrait" {"button/text" {:parent "button"
                                                   :text "panel portrait"}}}
                       (make-saved-layout->node->field->value scene))))

              (testing "After clearing Landscape layout override."
                (with-visible-layout! scene "Landscape"
                  (test-util/prop-clear! text :text))
                (is (= {"Default" {"button/text" #{:node-outline :text}}
                        "Landscape" {}
                        "Portrait" {"button/text" #{:node-outline :text}}}
                       (make-visibly-overridden-layout->node->props scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "panel default"}}
                        "Portrait" {"button/text" {:font "font"
                                                   :text "panel portrait"}}}
                       (make-displayed-layout->node->prop->value scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}
                        "Landscape" {}
                        "Portrait" {"button/text" {:font "font"
                                                   :text "panel portrait"}}}
                       (make-built-layout->node->field->value scene)))
                (is (= {"Default" {"button" {:template "/gui/template_layout/button.gui"}
                                   "button/text" {:parent "button"
                                                  :text "panel default"}}
                        "Landscape" {}
                        "Portrait" {"button/text" {:parent "button"
                                                   :text "panel portrait"}}}
                       (make-saved-layout->node->field->value scene))))

              (testing "After clearing Landscape and Default layout overrides."
                (with-visible-layout! scene ""
                  (test-util/prop-clear! text :text))
                (is (= {"Default" {}
                        "Landscape" {}
                        "Portrait" {"button/text" #{:node-outline :text}}}
                       (make-visibly-overridden-layout->node->props scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "button default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "button default"}}
                        "Portrait" {"button/text" {:font "font"
                                                   :text "panel portrait"}}}
                       (make-displayed-layout->node->prop->value scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "button default"}}
                        "Landscape" {}
                        "Portrait" {"button/text" {:font "font"
                                                   :text "panel portrait"}}}
                       (make-built-layout->node->field->value scene)))
                (is (= {"Default" {"button" {:template "/gui/template_layout/button.gui"}
                                   "button/text" {:parent "button"}}
                        "Landscape" {}
                        "Portrait" {"button/text" {:parent "button"
                                                   :text "panel portrait"}}}
                       (make-saved-layout->node->field->value scene))))))))

      (let [proj-path "/gui/template_layout/panel_lp_button_l.gui"]
        (testing proj-path
          (with-open [_ (make-restore-point!)]
            (let [scene (project/get-resource-node project proj-path)
                  text (get (scene-gui-node-map scene) "button/text")]

              (testing "Before modifications."
                (is (= {"Default" {"button/text" #{:node-outline :text}}
                        "Landscape" {"button/text" #{:node-outline :text}}
                        "Portrait" {"button/text" #{:node-outline :text}}}
                       (make-visibly-overridden-layout->node->props scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "panel landscape"}}
                        "Portrait" {"button/text" {:font "font"
                                                   :text "panel portrait"}}}
                       (make-displayed-layout->node->prop->value scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "panel landscape"}}
                        "Portrait" {"button/text" {:font "font"
                                                   :text "panel portrait"}}}
                       (make-built-layout->node->field->value scene)))
                (is (= {"Default" {"button" {:template "/gui/template_layout/button_l.gui"}
                                   "button/text" {:parent "button"
                                                  :text "panel default"}}
                        "Landscape" {"button/text" {:parent "button"
                                                    :text "panel landscape"}}
                        "Portrait" {"button/text" {:parent "button"
                                                   :text "panel portrait"}}}
                       (make-saved-layout->node->field->value scene))))

              (testing "After clearing Landscape layout override."
                (with-visible-layout! scene "Landscape"
                  (test-util/prop-clear! text :text))
                (is (= {"Default" {"button/text" #{:node-outline :text}}
                        "Landscape" {}
                        "Portrait" {"button/text" #{:node-outline :text}}}
                       (make-visibly-overridden-layout->node->props scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "button landscape"}}
                        "Portrait" {"button/text" {:font "font"
                                                   :text "panel portrait"}}}
                       (make-displayed-layout->node->prop->value scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "button landscape"}}
                        "Portrait" {"button/text" {:font "font"
                                                   :text "panel portrait"}}}
                       (make-built-layout->node->field->value scene)))
                (is (= {"Default" {"button" {:template "/gui/template_layout/button_l.gui"}
                                   "button/text" {:parent "button"
                                                  :text "panel default"}}
                        "Landscape" {}
                        "Portrait" {"button/text" {:parent "button"
                                                   :text "panel portrait"}}}
                       (make-saved-layout->node->field->value scene))))

              (testing "After clearing Landscape and Default layout overrides."
                (with-visible-layout! scene ""
                  (test-util/prop-clear! text :text))
                (is (= {"Default" {}
                        "Landscape" {}
                        "Portrait" {"button/text" #{:node-outline :text}}}
                       (make-visibly-overridden-layout->node->props scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "button default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "button landscape"}}
                        "Portrait" {"button/text" {:font "font"
                                                   :text "panel portrait"}}}
                       (make-displayed-layout->node->prop->value scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "button default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "button landscape"}}
                        "Portrait" {"button/text" {:font "font"
                                                   :text "panel portrait"}}}
                       (make-built-layout->node->field->value scene)))
                (is (= {"Default" {"button" {:template "/gui/template_layout/button_l.gui"}
                                   "button/text" {:parent "button"}}
                        "Landscape" {}
                        "Portrait" {"button/text" {:parent "button"
                                                   :text "panel portrait"}}}
                       (make-saved-layout->node->field->value scene))))))))

      (let [proj-path "/gui/template_layout/panel_lp_button_lp.gui"]
        (testing proj-path
          (with-open [_ (make-restore-point!)]
            (let [scene (project/get-resource-node project proj-path)
                  text (get (scene-gui-node-map scene) "button/text")]

              (testing "Before modifications."
                (is (= {"Default" {"button/text" #{:node-outline :text}}
                        "Landscape" {"button/text" #{:node-outline :text}}
                        "Portrait" {"button/text" #{:node-outline :text}}}
                       (make-visibly-overridden-layout->node->props scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "panel landscape"}}
                        "Portrait" {"button/text" {:font "font"
                                                   :text "panel portrait"}}}
                       (make-displayed-layout->node->prop->value scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "panel landscape"}}
                        "Portrait" {"button/text" {:font "font"
                                                   :text "panel portrait"}}}
                       (make-built-layout->node->field->value scene)))
                (is (= {"Default" {"button" {:template "/gui/template_layout/button_lp.gui"}
                                   "button/text" {:parent "button"
                                                  :text "panel default"}}
                        "Landscape" {"button/text" {:parent "button"
                                                    :text "panel landscape"}}
                        "Portrait" {"button/text" {:parent "button"
                                                   :text "panel portrait"}}}
                       (make-saved-layout->node->field->value scene))))

              (testing "After clearing Landscape layout override."
                (with-visible-layout! scene "Landscape"
                  (test-util/prop-clear! text :text))
                (is (= {"Default" {"button/text" #{:node-outline :text}}
                        "Landscape" {}
                        "Portrait" {"button/text" #{:node-outline :text}}}
                       (make-visibly-overridden-layout->node->props scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "button landscape"}}
                        "Portrait" {"button/text" {:font "font"
                                                   :text "panel portrait"}}}
                       (make-displayed-layout->node->prop->value scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "panel default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "button landscape"}}
                        "Portrait" {"button/text" {:font "font"
                                                   :text "panel portrait"}}}
                       (make-built-layout->node->field->value scene)))
                (is (= {"Default" {"button" {:template "/gui/template_layout/button_lp.gui"}
                                   "button/text" {:parent "button"
                                                  :text "panel default"}}
                        "Landscape" {}
                        "Portrait" {"button/text" {:parent "button"
                                                   :text "panel portrait"}}}
                       (make-saved-layout->node->field->value scene))))

              (testing "After clearing Landscape and Default layout overrides."
                (with-visible-layout! scene ""
                  (test-util/prop-clear! text :text))
                (is (= {"Default" {}
                        "Landscape" {}
                        "Portrait" {"button/text" #{:node-outline :text}}}
                       (make-visibly-overridden-layout->node->props scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "button default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "button landscape"}}
                        "Portrait" {"button/text" {:font "font"
                                                   :text "panel portrait"}}}
                       (make-displayed-layout->node->prop->value scene)))
                (is (= {"Default" {"button/text" {:font "font"
                                                  :text "button default"}}
                        "Landscape" {"button/text" {:font "font"
                                                    :text "button landscape"}}
                        "Portrait" {"button/text" {:font "font"
                                                   :text "panel portrait"}}}
                       (make-built-layout->node->field->value scene)))
                (is (= {"Default" {"button" {:template "/gui/template_layout/button_lp.gui"}
                                   "button/text" {:parent "button"}}
                        "Landscape" {}
                        "Portrait" {"button/text" {:parent "button"
                                                   :text "panel portrait"}}}
                       (make-saved-layout->node->field->value scene)))))))))))

(deftest template-layout-add-referenced-layout-to-referencing-scene-test
  (test-util/with-loaded-project "test/resources/gui_project"
    (let [make-restore-point! #(test-util/make-graph-reverter (project/graph project))]
      (with-open [_ (make-restore-point!)]
        (let [referencing-scene (project/get-resource-node project "/gui/template_layout/panel_button.gui")
              referenced-scene (project/get-resource-node project "/gui/template_layout/button.gui")
              referenced-scene-text (get (scene-gui-node-map referenced-scene) "text")]

          ;; Add the Landscape layout to the referenced scene.
          (add-layout! project app-view referenced-scene "Landscape")

          (testing "After adding Landscape layout to the referenced scene, but not yet to the referencing scene."
            (testing "Referenced scene."
              (is (= {"Default" {}
                      "Landscape" {}}
                     (make-visibly-overridden-layout->node->props referenced-scene)))
              (is (= {"Default" {"text" {:font "font"
                                         :text "button default"}}
                      "Landscape" {"text" {:font "font"
                                           :text "button default"}}}
                     (make-displayed-layout->node->prop->value referenced-scene)))
              (is (= {"Default" {"text" {:font "font"
                                         :text "button default"}}
                      "Landscape" {}}
                     (make-built-layout->node->field->value referenced-scene)))
              (is (= {"Default" {"text" {:font "font"
                                         :text "button default"}}
                      "Landscape" {}}
                     (make-saved-layout->node->field->value referenced-scene))))

            (testing "Referencing scene."
              (is (= {"Default" {"button/text" #{:node-outline :text}}}
                     (make-visibly-overridden-layout->node->props referencing-scene)))
              (is (= {"Default" {"button/text" {:font "font"
                                                :text "panel default"}}}
                     (make-displayed-layout->node->prop->value referencing-scene)))
              (is (= {"Default" {"button/text" {:font "font"
                                                :text "panel default"}}}
                     (make-built-layout->node->field->value referencing-scene)))
              (is (= {"Default" {"button" {:template "/gui/template_layout/button.gui"}
                                 "button/text" {:parent "button"
                                                :text "panel default"}}}
                     (make-saved-layout->node->field->value referencing-scene)))))

          ;; Add the Landscape layout to the referencing scene as well. Before,
          ;; it was only present in the referenced scene.
          (add-layout! project app-view referencing-scene "Landscape")

          (testing "After adding Landscape layout to the referencing scene, and now to the referenced scene as well."
            (testing "Referenced scene."
              (is (= {"Default" {}
                      "Landscape" {}}
                     (make-visibly-overridden-layout->node->props referenced-scene)))
              (is (= {"Default" {"text" {:font "font"
                                         :text "button default"}}
                      "Landscape" {"text" {:font "font"
                                           :text "button default"}}}
                     (make-displayed-layout->node->prop->value referenced-scene)))
              (is (= {"Default" {"text" {:font "font"
                                         :text "button default"}}
                      "Landscape" {}}
                     (make-built-layout->node->field->value referenced-scene)))
              (is (= {"Default" {"text" {:font "font"
                                         :text "button default"}}
                      "Landscape" {}}
                     (make-saved-layout->node->field->value referenced-scene))))

            (testing "Referencing scene."
              (is (= {"Default" {"button/text" #{:node-outline :text}}
                      "Landscape" {}}
                     (make-visibly-overridden-layout->node->props referencing-scene)))
              (is (= {"Default" {"button/text" {:font "font"
                                                :text "panel default"}}
                      "Landscape" {"button/text" {:font "font"
                                                  :text "button default"}}}
                     (make-displayed-layout->node->prop->value referencing-scene)))
              (is (= {"Default" {"button/text" {:font "font"
                                                :text "panel default"}}
                      "Landscape" {"button/text" {:font "font"
                                                  :text "button default"}}}
                     (make-built-layout->node->field->value referencing-scene)))
              (is (= {"Default" {"button" {:template "/gui/template_layout/button.gui"}
                                 "button/text" {:parent "button"
                                                :text "panel default"}}
                      "Landscape" {}}
                     (make-saved-layout->node->field->value referencing-scene)))))

          ;; Override the text property for the Landscape layout in the referenced scene.
          (with-visible-layout! referenced-scene "Landscape"
            (test-util/prop! referenced-scene-text :text "button landscape"))

          (testing "After overriding the text property for the Landscape layout in the referenced scene."
            (testing "Referenced scene."
              (is (= {"Default" {}
                      "Landscape" {"text" #{:node-outline :text}}}
                     (make-visibly-overridden-layout->node->props referenced-scene)))
              (is (= {"Default" {"text" {:font "font"
                                         :text "button default"}}
                      "Landscape" {"text" {:font "font"
                                           :text "button landscape"}}}
                     (make-displayed-layout->node->prop->value referenced-scene)))
              (is (= {"Default" {"text" {:font "font"
                                         :text "button default"}}
                      "Landscape" {"text" {:font "font"
                                           :text "button landscape"}}}
                     (make-built-layout->node->field->value referenced-scene)))
              (is (= {"Default" {"text" {:font "font"
                                         :text "button default"}}
                      "Landscape" {"text" {:text "button landscape"}}}
                     (make-saved-layout->node->field->value referenced-scene))))

            (testing "Referencing scene."
              (is (= {"Default" {"button/text" #{:node-outline :text}}
                      "Landscape" {}}
                     (make-visibly-overridden-layout->node->props referencing-scene)))
              (is (= {"Default" {"button/text" {:font "font"
                                                :text "panel default"}}
                      "Landscape" {"button/text" {:font "font"
                                                  :text "button landscape"}}}
                     (make-displayed-layout->node->prop->value referencing-scene)))
              (is (= {"Default" {"button/text" {:font "font"
                                                :text "panel default"}}
                      "Landscape" {"button/text" {:font "font"
                                                  :text "button landscape"}}}
                     (make-built-layout->node->field->value referencing-scene)))
              (is (= {"Default" {"button" {:template "/gui/template_layout/button.gui"}
                                 "button/text" {:parent "button"
                                                :text "panel default"}}
                      "Landscape" {}}
                     (make-saved-layout->node->field->value referencing-scene))))))))))

(deftest template-layout-add-referencing-layout-to-referenced-scene-test
  (test-util/with-loaded-project "test/resources/gui_project"
    (let [make-restore-point! #(test-util/make-graph-reverter (project/graph project))]
      (with-open [_ (make-restore-point!)]
        (let [referencing-scene (project/get-resource-node project "/gui/template_layout/panel_button.gui")
              referenced-scene (project/get-resource-node project "/gui/template_layout/button.gui")
              referenced-scene-text (get (scene-gui-node-map referenced-scene) "text")]

          ;; Add the Landscape layout to the referencing scene.
          (add-layout! project app-view referencing-scene "Landscape")

          (testing "After adding Landscape layout to the referencing scene, but not yet to the referenced scene."
            (testing "Referenced scene."
              (is (= {"Default" {}}
                     (make-visibly-overridden-layout->node->props referenced-scene)))
              (is (= {"Default" {"text" {:font "font"
                                         :text "button default"}}}
                     (make-displayed-layout->node->prop->value referenced-scene)))
              (is (= {"Default" {"text" {:font "font"
                                         :text "button default"}}}
                     (make-built-layout->node->field->value referenced-scene)))
              (is (= {"Default" {"text" {:font "font"
                                         :text "button default"}}}
                     (make-saved-layout->node->field->value referenced-scene))))

            (testing "Referencing scene."
              (is (= {"Default" {"button/text" #{:node-outline :text}}
                      "Landscape" {}}
                     (make-visibly-overridden-layout->node->props referencing-scene)))
              (is (= {"Default" {"button/text" {:font "font"
                                                :text "panel default"}}
                      "Landscape" {"button/text" {:font "font"
                                                  :text "panel default"}}}
                     (make-displayed-layout->node->prop->value referencing-scene)))
              (is (= {"Default" {"button/text" {:font "font"
                                                :text "panel default"}}
                      "Landscape" {}}
                     (make-built-layout->node->field->value referencing-scene)))
              (is (= {"Default" {"button" {:template "/gui/template_layout/button.gui"}
                                 "button/text" {:parent "button"
                                                :text "panel default"}}
                      "Landscape" {}}
                     (make-saved-layout->node->field->value referencing-scene)))))

          ;; Add the Landscape layout to the referenced scene as well. Before,
          ;; it was only present in the referencing scene.
          (add-layout! project app-view referenced-scene "Landscape")

          (testing "After adding Landscape layout to the referencing scene, and now to the referenced scene as well."
            (testing "Referenced scene."
              (is (= {"Default" {}
                      "Landscape" {}}
                     (make-visibly-overridden-layout->node->props referenced-scene)))
              (is (= {"Default" {"text" {:font "font"
                                         :text "button default"}}
                      "Landscape" {"text" {:font "font"
                                           :text "button default"}}}
                     (make-displayed-layout->node->prop->value referenced-scene)))
              (is (= {"Default" {"text" {:font "font"
                                         :text "button default"}}
                      "Landscape" {}}
                     (make-built-layout->node->field->value referenced-scene)))
              (is (= {"Default" {"text" {:font "font"
                                         :text "button default"}}
                      "Landscape" {}}
                     (make-saved-layout->node->field->value referenced-scene))))

            (testing "Referencing scene."
              (is (= {"Default" {"button/text" #{:node-outline :text}}
                      "Landscape" {}}
                     (make-visibly-overridden-layout->node->props referencing-scene)))
              (is (= {"Default" {"button/text" {:font "font"
                                                :text "panel default"}}
                      "Landscape" {"button/text" {:font "font"
                                                  :text "button default"}}}
                     (make-displayed-layout->node->prop->value referencing-scene)))
              (is (= {"Default" {"button/text" {:font "font"
                                                :text "panel default"}}
                      "Landscape" {"button/text" {:font "font"
                                                  :text "button default"}}}
                     (make-built-layout->node->field->value referencing-scene)))
              (is (= {"Default" {"button" {:template "/gui/template_layout/button.gui"}
                                 "button/text" {:parent "button"
                                                :text "panel default"}}
                      "Landscape" {}}
                     (make-saved-layout->node->field->value referencing-scene)))))

          ;; Override the text property for the Landscape layout in the referenced scene.
          (with-visible-layout! referenced-scene "Landscape"
            (test-util/prop! referenced-scene-text :text "button landscape"))

          (testing "After overriding the text property for the Landscape layout in the referenced scene."
            (testing "Referenced scene."
              (is (= {"Default" {}
                      "Landscape" {"text" #{:node-outline :text}}}
                     (make-visibly-overridden-layout->node->props referenced-scene)))
              (is (= {"Default" {"text" {:font "font"
                                         :text "button default"}}
                      "Landscape" {"text" {:font "font"
                                           :text "button landscape"}}}
                     (make-displayed-layout->node->prop->value referenced-scene)))
              (is (= {"Default" {"text" {:font "font"
                                         :text "button default"}}
                      "Landscape" {"text" {:font "font"
                                           :text "button landscape"}}}
                     (make-built-layout->node->field->value referenced-scene)))
              (is (= {"Default" {"text" {:font "font"
                                         :text "button default"}}
                      "Landscape" {"text" {:text "button landscape"}}}
                     (make-saved-layout->node->field->value referenced-scene))))

            (testing "Referencing scene."
              (is (= {"Default" {"button/text" #{:node-outline :text}}
                      "Landscape" {}}
                     (make-visibly-overridden-layout->node->props referencing-scene)))
              (is (= {"Default" {"button/text" {:font "font"
                                                :text "panel default"}}
                      "Landscape" {"button/text" {:font "font"
                                                  :text "button landscape"}}}
                     (make-displayed-layout->node->prop->value referencing-scene)))
              (is (= {"Default" {"button/text" {:font "font"
                                                :text "panel default"}}
                      "Landscape" {"button/text" {:font "font"
                                                  :text "button landscape"}}}
                     (make-built-layout->node->field->value referencing-scene)))
              (is (= {"Default" {"button" {:template "/gui/template_layout/button.gui"}
                                 "button/text" {:parent "button"
                                                :text "panel default"}}
                      "Landscape" {}}
                     (make-saved-layout->node->field->value referencing-scene)))))

          ;; Add a new node to the referenced scene.
          (let [referenced-scene-node-tree (g/node-value referenced-scene :node-tree)
                added-text-props {:id "added" :font "font" :text "button default"}]
            (gui/add-gui-node-with-props! referenced-scene referenced-scene-node-tree :type-text 0 added-text-props nil))

          (testing "After adding a new text node to the referenced scene."
            (testing "Referenced scene."
              (is (= {"Default" {}
                      "Landscape" {"text" #{:node-outline :text}}}
                     (make-visibly-overridden-layout->node->props referenced-scene)))
              (is (= {"Default" {"added" {:font "font"
                                          :text "button default"}
                                 "text" {:font "font"
                                         :text "button default"}}
                      "Landscape" {"added" {:font "font"
                                            :text "button default"}
                                   "text" {:font "font"
                                           :text "button landscape"}}}
                     (make-displayed-layout->node->prop->value referenced-scene)))
              (is (= {"Default" {"added" {:font "font"
                                          :text "button default"}
                                 "text" {:font "font"
                                         :text "button default"}}
                      "Landscape" {"text" {:font "font"
                                           :text "button landscape"}}}
                     (make-built-layout->node->field->value referenced-scene)))
              (is (= {"Default" {"added" {:font "font"
                                          :text "button default"}
                                 "text" {:font "font"
                                         :text "button default"}}
                      "Landscape" {"text" {:text "button landscape"}}}
                     (make-saved-layout->node->field->value referenced-scene))))

            (testing "Referencing scene."
              (is (= {"Default" {"button/text" #{:node-outline :text}}
                      "Landscape" {}}
                     (make-visibly-overridden-layout->node->props referencing-scene)))
              (is (= {"Default" {"button/added" {:font "font"
                                                 :text "button default"}
                                 "button/text" {:font "font"
                                                :text "panel default"}}
                      "Landscape" {"button/added" {:font "font"
                                                   :text "button default"}
                                   "button/text" {:font "font"
                                                  :text "button landscape"}}}
                     (make-displayed-layout->node->prop->value referencing-scene)))
              (is (= {"Default" {"button/added" {:font "font"
                                                 :text "button default"}
                                 "button/text" {:font "font"
                                                :text "panel default"}}
                      "Landscape" {"button/text" {:font "font"
                                                  :text "button landscape"}}}
                     (make-built-layout->node->field->value referencing-scene)))
              (is (= {"Default" {"button" {:template "/gui/template_layout/button.gui"}
                                 "button/added" {:parent "button"}
                                 "button/text" {:parent "button"
                                                :text "panel default"}}
                      "Landscape" {}}
                     (make-saved-layout->node->field->value referencing-scene)))))

          (let [referenced-scene-added-text (get (scene-gui-node-map referenced-scene) "added")
                referencing-scene-added-text (get (scene-gui-node-map referencing-scene) "button/added")]

            (testing "Before: Layout overrides are not present in either node (sanity check)."
              (is (= {} (g/node-value referenced-scene-added-text :layout->prop->override)))
              (is (= {} (g/node-value referencing-scene-added-text :layout->prop->override))))

            ;; Override the text property on the added node for the Landscape layout in the referenced scene.
            (with-visible-layout! referenced-scene "Landscape"
              (test-util/prop! referenced-scene-added-text :text "button landscape"))

            (testing "After: Layout override is present only in referenced scene node (sanity check)."
              (is (= {"Landscape" {:text "button landscape"}} (g/node-value referenced-scene-added-text :layout->prop->override)))
              (is (= {} (g/node-value referencing-scene-added-text :layout->prop->override)))))

          (testing "After overriding the text property for the Landscape layout in the referenced scene."
            (testing "Referenced scene."
              (is (= {"Default" {}
                      "Landscape" {"added" #{:node-outline :text}
                                   "text" #{:node-outline :text}}}
                     (make-visibly-overridden-layout->node->props referenced-scene)))
              (is (= {"Default" {"added" {:font "font"
                                          :text "button default"}
                                 "text" {:font "font"
                                         :text "button default"}}
                      "Landscape" {"added" {:font "font"
                                            :text "button landscape"}
                                   "text" {:font "font"
                                           :text "button landscape"}}}
                     (make-displayed-layout->node->prop->value referenced-scene)))
              (is (= {"Default" {"added" {:font "font"
                                          :text "button default"}
                                 "text" {:font "font"
                                         :text "button default"}}
                      "Landscape" {"added" {:font "font"
                                            :text "button landscape"}
                                   "text" {:font "font"
                                           :text "button landscape"}}}
                     (make-built-layout->node->field->value referenced-scene)))
              (is (= {"Default" {"added" {:font "font"
                                          :text "button default"}
                                 "text" {:font "font"
                                         :text "button default"}}
                      "Landscape" {"added" {:text "button landscape"}
                                   "text" {:text "button landscape"}}}
                     (make-saved-layout->node->field->value referenced-scene))))

            (testing "Referencing scene."
              (is (= {"Default" {"button/text" #{:node-outline :text}}
                      "Landscape" {}}
                     (make-visibly-overridden-layout->node->props referencing-scene)))
              (is (= {"Default" {"button/added" {:font "font"
                                                 :text "button default"}
                                 "button/text" {:font "font"
                                                :text "panel default"}}
                      "Landscape" {"button/added" {:font "font"
                                                   :text "button landscape"}
                                   "button/text" {:font "font"
                                                  :text "button landscape"}}}
                     (make-displayed-layout->node->prop->value referencing-scene)))
              (is (= {"Default" {"button/added" {:font "font"
                                                 :text "button default"}
                                 "button/text" {:font "font"
                                                :text "panel default"}}
                      "Landscape" {"button/added" {:font "font"
                                                   :text "button landscape"}
                                   "button/text" {:font "font"
                                                  :text "button landscape"}}}
                     (make-built-layout->node->field->value referencing-scene)))
              (is (= {"Default" {"button" {:template "/gui/template_layout/button.gui"}
                                 "button/added" {:parent "button"}
                                 "button/text" {:parent "button"
                                                :text "panel default"}}
                      "Landscape" {}}
                     (make-saved-layout->node->field->value referencing-scene))))))))))

(deftest template-layout-remove-gui-node-from-referenced-scene-test
  (test-util/with-loaded-project "test/resources/gui_project"
    (let [referencing-scene (project/get-resource-node project "/gui/template_layout/panel_l_button_l.gui")
          referenced-scene (project/get-resource-node project "/gui/template_layout/button_l.gui")
          referenced-scene-text (get (scene-gui-node-map referenced-scene) "text")]

      ;; Delete the text node from the referenced scene.
      (g/delete-node! referenced-scene-text)

      (testing "After deleting the text node from the referenced scene."
        (testing "Referenced scene."
          (is (= {"Default" {}
                  "Landscape" {}}
                 (make-visibly-overridden-layout->node->props referenced-scene)))
          (is (= {"Default" {}
                  "Landscape" {}}
                 (make-displayed-layout->node->prop->value referenced-scene)))
          (is (= {"Default" {}
                  "Landscape" {}}
                 (make-built-layout->node->field->value referenced-scene)))
          (is (= {"Default" {}
                  "Landscape" {}}
                 (make-saved-layout->node->field->value referenced-scene))))

        (testing "Referencing scene."
          (is (= {"Default" {}
                  "Landscape" {}}
                 (make-visibly-overridden-layout->node->props referencing-scene)))
          (is (= {"Default" {}
                  "Landscape" {}}
                 (make-displayed-layout->node->prop->value referencing-scene)))
          (is (= {"Default" {}
                  "Landscape" {}}
                 (make-built-layout->node->field->value referencing-scene)))
          (is (= {"Default" {"button" {:template "/gui/template_layout/button_l.gui"}}
                  "Landscape" {}}
                 (make-saved-layout->node->field->value referencing-scene))))))))

(deftest template-layout-add-gui-node-to-referenced-scene-test
  (test-util/with-loaded-project "test/resources/gui_project"
    (let [referencing-scene (project/get-resource-node project "/gui/template_layout/panel_l_button_l.gui")
          referenced-scene (project/get-resource-node project "/gui/template_layout/button_l.gui")
          referenced-scene-text (get (scene-gui-node-map referenced-scene) "text")
          referenced-scene-node-tree (g/node-value referenced-scene :node-tree)
          added-text-props {:id "added" :font "font" :text "button default"}
          referenced-scene-added-text (gui/add-gui-node-with-props! referenced-scene referenced-scene-node-tree :type-text 0 added-text-props nil)
          referencing-scene-added-text (get (scene-gui-node-map referencing-scene) "button/added")]

      ;; Delete the original text node from the referenced scene in order to
      ;; keep the test data simple.
      (g/delete-node! referenced-scene-text)

      (testing "After adding a new text node to the referenced scene."
        (testing "Referenced scene."
          (is (= {"Default" {}
                  "Landscape" {}}
                 (make-visibly-overridden-layout->node->props referenced-scene)))
          (is (= {"Default" {"added" {:font "font"
                                      :text "button default"}}
                  "Landscape" {"added" {:font "font"
                                        :text "button default"}}}
                 (make-displayed-layout->node->prop->value referenced-scene)))
          (is (= {"Default" {"added" {:font "font"
                                      :text "button default"}}
                  "Landscape" {}}
                 (make-built-layout->node->field->value referenced-scene)))
          (is (= {"Default" {"added" {:font "font"
                                      :text "button default"}}
                  "Landscape" {}}
                 (make-saved-layout->node->field->value referenced-scene))))

        (testing "Referencing scene."
          (is (= {"Default" {}
                  "Landscape" {}}
                 (make-visibly-overridden-layout->node->props referencing-scene)))
          (is (= {"Default" {"button/added" {:font "font"
                                             :text "button default"}}
                  "Landscape" {"button/added" {:font "font"
                                               :text "button default"}}}
                 (make-displayed-layout->node->prop->value referencing-scene)))
          (is (= {"Default" {"button/added" {:font "font"
                                             :text "button default"}}
                  "Landscape" {}}
                 (make-built-layout->node->field->value referencing-scene)))
          (is (= {"Default" {"button" {:template "/gui/template_layout/button_l.gui"}
                             "button/added" {:parent "button"}}
                  "Landscape" {}}
                 (make-saved-layout->node->field->value referencing-scene)))))

      (testing "Before: Layout overrides are not present in either node (sanity check)."
        (is (= {} (g/node-value referenced-scene-added-text :layout->prop->override)))
        (is (= {} (g/node-value referencing-scene-added-text :layout->prop->override))))

      ;; Override the text property for the Landscape layout in the referenced scene.
      (with-visible-layout! referenced-scene "Landscape"
        (test-util/prop! referenced-scene-added-text :text "button landscape"))

      (testing "After: Layout override is present only in referenced scene node (sanity check)."
        (is (= {"Landscape" {:text "button landscape"}} (g/node-value referenced-scene-added-text :layout->prop->override)))
        (is (= {} (g/node-value referencing-scene-added-text :layout->prop->override))))

      (testing "After overriding the text property for the Landscape layout in the referenced scene."
        (testing "Referenced scene."
          (is (= {"Default" {}
                  "Landscape" {"added" #{:node-outline :text}}}
                 (make-visibly-overridden-layout->node->props referenced-scene)))
          (is (= {"Default" {"added" {:font "font"
                                      :text "button default"}}
                  "Landscape" {"added" {:font "font"
                                        :text "button landscape"}}}
                 (make-displayed-layout->node->prop->value referenced-scene)))
          (is (= {"Default" {"added" {:font "font"
                                      :text "button default"}}
                  "Landscape" {"added" {:font "font"
                                        :text "button landscape"}}}
                 (make-built-layout->node->field->value referenced-scene)))
          (is (= {"Default" {"added" {:font "font"
                                      :text "button default"}}
                  "Landscape" {"added" {:text "button landscape"}}}
                 (make-saved-layout->node->field->value referenced-scene))))

        (testing "Referencing scene."
          (is (= {"Default" {}
                  "Landscape" {}}
                 (make-visibly-overridden-layout->node->props referencing-scene)))
          (is (= {"Default" {"button/added" {:font "font"
                                             :text "button default"}}
                  "Landscape" {"button/added" {:font "font"
                                               :text "button landscape"}}}
                 (make-displayed-layout->node->prop->value referencing-scene)))
          (is (= {"Default" {"button/added" {:font "font"
                                             :text "button default"}}
                  "Landscape" {"button/added" {:font "font"
                                               :text "button landscape"}}}
                 (make-built-layout->node->field->value referencing-scene)))
          (is (= {"Default" {"button" {:template "/gui/template_layout/button_l.gui"}
                             "button/added" {:parent "button"}}
                  "Landscape" {}}
                 (make-saved-layout->node->field->value referencing-scene))))))))

(deftest template-layout-resource-rename-test
  (test-util/with-loaded-project "test/resources/gui_project"
    (let [make-restore-point! #(test-util/make-graph-reverter (project/graph project))]
      (let [referencing-scene (project/get-resource-node project "/gui/resources/panel.gui")
            referenced-scene (project/get-resource-node project "/gui/resources/button.gui")

            make-layout->node->resource-field-values
            (fn make-layout->node->resource-field-values [gui-scene-node-id]
              (let [scene-desc (g/valid-node-value gui-scene-node-id :save-value)]
                (make-layout->node->field->value
                  scene-desc
                  (fn node-desc-fn [node-desc _is-override-node-desc]
                    (-> node-desc
                        (select-keys [:font :layer :material :particlefx :spine-scene :texture])
                        (coll/not-empty))))))]

        (testing "Before renaming resources."
          (testing "Referenced scene."
            (is (= {"Default" {"box" {:layer "button_layer"
                                      :material "button_material"
                                      :texture "button_texture/button_striped"}
                               "particlefx" {:layer "button_layer"
                                             :material "button_material"
                                             :particlefx "button_particlefx"}
                               "pie" {:layer "button_layer"
                                      :material "button_material"
                                      :texture "button_texture/button_striped"}
                               "spine" {:layer "button_layer"
                                        :material "button_material"
                                        :spine-scene "button_spinescene"}
                               "text" {:font "button_font"
                                       :layer "button_layer"
                                       :material "button_material"}}
                    "Landscape" {"box" {:layer "button_layer"
                                        :material "button_material"
                                        :texture "button_texture/button_striped"}
                                 "particlefx" {:layer "button_layer"
                                               :material "button_material"
                                               :particlefx "button_particlefx"}
                                 "pie" {:layer "button_layer"
                                        :material "button_material"
                                        :texture "button_texture/button_striped"}
                                 "spine" {:layer "button_layer"
                                          :material "button_material"
                                          :spine-scene "button_spinescene"}
                                 "text" {:font "button_font"
                                         :layer "button_layer"
                                         :material "button_material"}}}
                   (make-layout->node->resource-field-values referenced-scene))))

          (testing "Referencing scene."
            (is (= {"Default" {"button_resources" {:layer "button_layer"}
                               "button_resources/box" {:layer "button_layer"
                                                       :material "button_material"
                                                       :texture "button_texture/button_striped"}
                               "button_resources/particlefx" {:layer "button_layer"
                                                              :material "button_material"
                                                              :particlefx "button_particlefx"}
                               "button_resources/pie" {:layer "button_layer"
                                                       :material "button_material"
                                                       :texture "button_texture/button_striped"}
                               "button_resources/spine" {:layer "button_layer"
                                                         :material "button_material"
                                                         :spine-scene "button_spinescene"}
                               "button_resources/text" {:font "button_font"
                                                        :layer "button_layer"
                                                        :material "button_material"}
                               "panel_resources" {:layer "panel_layer"}
                               "panel_resources/box" {:layer "panel_layer"
                                                      :material "panel_material"
                                                      :texture "panel_texture/button_striped"}
                               "panel_resources/particlefx" {:layer "panel_layer"
                                                             :material "panel_material"
                                                             :particlefx "panel_particlefx"}
                               "panel_resources/pie" {:layer "panel_layer"
                                                      :material "panel_material"
                                                      :texture "panel_texture/button_striped"}
                               "panel_resources/spine" {:layer "panel_layer"
                                                        :material "panel_material"
                                                        :spine-scene "panel_spinescene"}
                               "panel_resources/text" {:font "panel_font"
                                                       :layer "panel_layer"
                                                       :material "panel_material"}}
                    "Landscape" {"button_resources/box" {:layer "button_layer"
                                                         :material "button_material"
                                                         :texture "button_texture/button_striped"}
                                 "button_resources/particlefx" {:layer "button_layer"
                                                                :material "button_material"
                                                                :particlefx "button_particlefx"}
                                 "button_resources/pie" {:layer "button_layer"
                                                         :material "button_material"
                                                         :texture "button_texture/button_striped"}
                                 "button_resources/spine" {:layer "button_layer"
                                                           :material "button_material"
                                                           :spine-scene "button_spinescene"}
                                 "button_resources/text" {:font "button_font"
                                                          :layer "button_layer"
                                                          :material "button_material"}
                                 "panel_resources/box" {:layer "panel_layer"
                                                        :material "panel_material"
                                                        :texture "panel_texture/button_striped"}
                                 "panel_resources/particlefx" {:layer "panel_layer"
                                                               :material "panel_material"
                                                               :particlefx "panel_particlefx"}
                                 "panel_resources/pie" {:layer "panel_layer"
                                                        :material "panel_material"
                                                        :texture "panel_texture/button_striped"}
                                 "panel_resources/spine" {:layer "panel_layer"
                                                          :material "panel_material"
                                                          :spine-scene "panel_spinescene"}
                                 "panel_resources/text" {:font "panel_font"
                                                         :layer "panel_layer"
                                                         :material "panel_material"}}}
                   (make-layout->node->resource-field-values referencing-scene)))))

        (testing "After renaming resources in the referenced scene."
          (with-open [_ (make-restore-point!)]

            ;; Rename all resources in the referenced scene.
            (let [button-font (gui-font referenced-scene "button_font")
                  button-layer (gui-layer referenced-scene "button_layer")
                  button-material (gui-material referenced-scene "button_material")
                  button-particlefx (gui-particlefx-resource referenced-scene "button_particlefx")
                  button-spinescene (gui-spine-scene referenced-scene "button_spinescene")
                  button-texture (gui-texture referenced-scene "button_texture")]
              (test-util/prop! button-font :name "button_font_renamed")
              (test-util/prop! button-layer :name "button_layer_renamed")
              (test-util/prop! button-material :name "button_material_renamed")
              (test-util/prop! button-particlefx :name "button_particlefx_renamed")
              (test-util/prop! button-spinescene :name "button_spinescene_renamed")
              (test-util/prop! button-texture :name "button_texture_renamed"))

            (testing "Referenced scene."
              (is (= {"Default" {"box" {:layer "button_layer_renamed"
                                        :material "button_material_renamed"
                                        :texture "button_texture_renamed/button_striped"}
                                 "particlefx" {:layer "button_layer_renamed"
                                               :material "button_material_renamed"
                                               :particlefx "button_particlefx_renamed"}
                                 "pie" {:layer "button_layer_renamed"
                                        :material "button_material_renamed"
                                        :texture "button_texture_renamed/button_striped"}
                                 "spine" {:layer "button_layer_renamed"
                                          :material "button_material_renamed"
                                          :spine-scene "button_spinescene_renamed"}
                                 "text" {:font "button_font_renamed"
                                         :layer "button_layer_renamed"
                                         :material "button_material_renamed"}}
                      "Landscape" {"box" {:layer "button_layer_renamed"
                                          :material "button_material_renamed"
                                          :texture "button_texture_renamed/button_striped"}
                                   "particlefx" {:layer "button_layer_renamed"
                                                 :material "button_material_renamed"
                                                 :particlefx "button_particlefx_renamed"}
                                   "pie" {:layer "button_layer_renamed"
                                          :material "button_material_renamed"
                                          :texture "button_texture_renamed/button_striped"}
                                   "spine" {:layer "button_layer_renamed"
                                            :material "button_material_renamed"
                                            :spine-scene "button_spinescene_renamed"}
                                   "text" {:font "button_font_renamed"
                                           :layer "button_layer_renamed"
                                           :material "button_material_renamed"}}}
                     (make-layout->node->resource-field-values referenced-scene))))

            (testing "Referencing scene."
              ;; Note: "button_layer" existed in both button.gui and panel.gui
              ;; before the rename. After the rename of "button_layer" to
              ;; "button_layer_renamed" in button.gui, we should not see any
              ;; updated references to it in panel.gui, since it has its own
              ;; "button_layer".
              (is (= {"Default" {"button_resources" {:layer "button_layer"}
                                 "button_resources/box" {:layer "button_layer"
                                                         :material "button_material_renamed"
                                                         :texture "button_texture_renamed/button_striped"}
                                 "button_resources/particlefx" {:layer "button_layer"
                                                                :material "button_material_renamed"
                                                                :particlefx "button_particlefx_renamed"}
                                 "button_resources/pie" {:layer "button_layer"
                                                         :material "button_material_renamed"
                                                         :texture "button_texture_renamed/button_striped"}
                                 "button_resources/spine" {:layer "button_layer"
                                                           :material "button_material_renamed"
                                                           :spine-scene "button_spinescene_renamed"}
                                 "button_resources/text" {:font "button_font_renamed"
                                                          :layer "button_layer"
                                                          :material "button_material_renamed"}
                                 "panel_resources" {:layer "panel_layer"}
                                 "panel_resources/box" {:layer "panel_layer"
                                                        :material "panel_material"
                                                        :texture "panel_texture/button_striped"}
                                 "panel_resources/particlefx" {:layer "panel_layer"
                                                               :material "panel_material"
                                                               :particlefx "panel_particlefx"}
                                 "panel_resources/pie" {:layer "panel_layer"
                                                        :material "panel_material"
                                                        :texture "panel_texture/button_striped"}
                                 "panel_resources/spine" {:layer "panel_layer"
                                                          :material "panel_material"
                                                          :spine-scene "panel_spinescene"}
                                 "panel_resources/text" {:font "panel_font"
                                                         :layer "panel_layer"
                                                         :material "panel_material"}}
                      "Landscape" {"button_resources/box" {:layer "button_layer"
                                                           :material "button_material_renamed"
                                                           :texture "button_texture_renamed/button_striped"}
                                   "button_resources/particlefx" {:layer "button_layer"
                                                                  :material "button_material_renamed"
                                                                  :particlefx "button_particlefx_renamed"}
                                   "button_resources/pie" {:layer "button_layer"
                                                           :material "button_material_renamed"
                                                           :texture "button_texture_renamed/button_striped"}
                                   "button_resources/spine" {:layer "button_layer"
                                                             :material "button_material_renamed"
                                                             :spine-scene "button_spinescene_renamed"}
                                   "button_resources/text" {:font "button_font_renamed"
                                                            :layer "button_layer"
                                                            :material "button_material_renamed"}
                                   "panel_resources/box" {:layer "panel_layer"
                                                          :material "panel_material"
                                                          :texture "panel_texture/button_striped"}
                                   "panel_resources/particlefx" {:layer "panel_layer"
                                                                 :material "panel_material"
                                                                 :particlefx "panel_particlefx"}
                                   "panel_resources/pie" {:layer "panel_layer"
                                                          :material "panel_material"
                                                          :texture "panel_texture/button_striped"}
                                   "panel_resources/spine" {:layer "panel_layer"
                                                            :material "panel_material"
                                                            :spine-scene "panel_spinescene"}
                                   "panel_resources/text" {:font "panel_font"
                                                           :layer "panel_layer"
                                                           :material "panel_material"}}}
                     (make-layout->node->resource-field-values referencing-scene))))))

        (testing "After renaming resources in the referencing scene."
          (with-open [_ (make-restore-point!)]

            ;; Rename all resources in the referencing scene.
            (let [panel-font (gui-font referencing-scene "panel_font")
                  panel-layer (gui-layer referencing-scene "panel_layer")
                  panel-material (gui-material referencing-scene "panel_material")
                  panel-particlefx (gui-particlefx-resource referencing-scene "panel_particlefx")
                  panel-spinescene (gui-spine-scene referencing-scene "panel_spinescene")
                  panel-texture (gui-texture referencing-scene "panel_texture")]
              (test-util/prop! panel-font :name "panel_font_renamed")
              (test-util/prop! panel-layer :name "panel_layer_renamed")
              (test-util/prop! panel-material :name "panel_material_renamed")
              (test-util/prop! panel-particlefx :name "panel_particlefx_renamed")
              (test-util/prop! panel-spinescene :name "panel_spinescene_renamed")
              (test-util/prop! panel-texture :name "panel_texture_renamed"))

            (testing "Referenced scene."
              (is (= {"Default" {"box" {:layer "button_layer"
                                        :material "button_material"
                                        :texture "button_texture/button_striped"}
                                 "particlefx" {:layer "button_layer"
                                               :material "button_material"
                                               :particlefx "button_particlefx"}
                                 "pie" {:layer "button_layer"
                                        :material "button_material"
                                        :texture "button_texture/button_striped"}
                                 "spine" {:layer "button_layer"
                                          :material "button_material"
                                          :spine-scene "button_spinescene"}
                                 "text" {:font "button_font"
                                         :layer "button_layer"
                                         :material "button_material"}}
                      "Landscape" {"box" {:layer "button_layer"
                                          :material "button_material"
                                          :texture "button_texture/button_striped"}
                                   "particlefx" {:layer "button_layer"
                                                 :material "button_material"
                                                 :particlefx "button_particlefx"}
                                   "pie" {:layer "button_layer"
                                          :material "button_material"
                                          :texture "button_texture/button_striped"}
                                   "spine" {:layer "button_layer"
                                            :material "button_material"
                                            :spine-scene "button_spinescene"}
                                   "text" {:font "button_font"
                                           :layer "button_layer"
                                           :material "button_material"}}}
                     (make-layout->node->resource-field-values referenced-scene))))

            (testing "Referencing scene."
              (is (= {"Default" {"button_resources" {:layer "button_layer"}
                                 "button_resources/box" {:layer "button_layer"
                                                         :material "button_material"
                                                         :texture "button_texture/button_striped"}
                                 "button_resources/particlefx" {:layer "button_layer"
                                                                :material "button_material"
                                                                :particlefx "button_particlefx"}
                                 "button_resources/pie" {:layer "button_layer"
                                                         :material "button_material"
                                                         :texture "button_texture/button_striped"}
                                 "button_resources/spine" {:layer "button_layer"
                                                           :material "button_material"
                                                           :spine-scene "button_spinescene"}
                                 "button_resources/text" {:font "button_font"
                                                          :layer "button_layer"
                                                          :material "button_material"}
                                 "panel_resources" {:layer "panel_layer_renamed"}
                                 "panel_resources/box" {:layer "panel_layer_renamed"
                                                        :material "panel_material_renamed"
                                                        :texture "panel_texture_renamed/button_striped"}
                                 "panel_resources/particlefx" {:layer "panel_layer_renamed"
                                                               :material "panel_material_renamed"
                                                               :particlefx "panel_particlefx_renamed"}
                                 "panel_resources/pie" {:layer "panel_layer_renamed"
                                                        :material "panel_material_renamed"
                                                        :texture "panel_texture_renamed/button_striped"}
                                 "panel_resources/spine" {:layer "panel_layer_renamed"
                                                          :material "panel_material_renamed"
                                                          :spine-scene "panel_spinescene_renamed"}
                                 "panel_resources/text" {:font "panel_font_renamed"
                                                         :layer "panel_layer_renamed"
                                                         :material "panel_material_renamed"}}
                      "Landscape" {"button_resources/box" {:layer "button_layer"
                                                           :material "button_material"
                                                           :texture "button_texture/button_striped"}
                                   "button_resources/particlefx" {:layer "button_layer"
                                                                  :material "button_material"
                                                                  :particlefx "button_particlefx"}
                                   "button_resources/pie" {:layer "button_layer"
                                                           :material "button_material"
                                                           :texture "button_texture/button_striped"}
                                   "button_resources/spine" {:layer "button_layer"
                                                             :material "button_material"
                                                             :spine-scene "button_spinescene"}
                                   "button_resources/text" {:font "button_font"
                                                            :layer "button_layer"
                                                            :material "button_material"}
                                   "panel_resources/box" {:layer "panel_layer_renamed"
                                                          :material "panel_material_renamed"
                                                          :texture "panel_texture_renamed/button_striped"}
                                   "panel_resources/particlefx" {:layer "panel_layer_renamed"
                                                                 :material "panel_material_renamed"
                                                                 :particlefx "panel_particlefx_renamed"}
                                   "panel_resources/pie" {:layer "panel_layer_renamed"
                                                          :material "panel_material_renamed"
                                                          :texture "panel_texture_renamed/button_striped"}
                                   "panel_resources/spine" {:layer "panel_layer_renamed"
                                                            :material "panel_material_renamed"
                                                            :spine-scene "panel_spinescene_renamed"}
                                   "panel_resources/text" {:font "panel_font_renamed"
                                                           :layer "panel_layer_renamed"
                                                           :material "panel_material_renamed"}}}
                     (make-layout->node->resource-field-values referencing-scene))))))))))

(deftest template-layout-shadowing-resource-rename-test
  (test-util/with-loaded-project "test/resources/gui_project"
    (let [make-restore-point! #(test-util/make-graph-reverter (project/graph project))]
      (let [referencing-scene (project/get-resource-node project "/gui/resources/shadowing_panel.gui")
            referenced-scene (project/get-resource-node project "/gui/resources/shadowing_button.gui")

            make-layout->node->resource-field-values
            (fn make-layout->node->resource-field-values [gui-scene-node-id]
              (let [scene-desc (g/valid-node-value gui-scene-node-id :save-value)]
                (make-layout->node->field->value
                  scene-desc
                  (fn node-desc-fn [node-desc _is-override-node-desc]
                    (-> node-desc
                        (select-keys [:font :layer :material :particlefx :spine-scene :texture])
                        (coll/not-empty))))))]

        (testing "Before renaming resources."
          (testing "Referenced scene."
            (is (= {"Default" {"box" {:layer "shadowing_layer"
                                      :material "shadowing_material"
                                      :texture "shadowing_texture/button_striped"}
                               "particlefx" {:layer "shadowing_layer"
                                             :material "shadowing_material"
                                             :particlefx "shadowing_particlefx"}
                               "pie" {:layer "shadowing_layer"
                                      :material "shadowing_material"
                                      :texture "shadowing_texture/button_striped"}
                               "spine" {:layer "shadowing_layer"
                                        :material "shadowing_material"
                                        :spine-scene "shadowing_spinescene"}
                               "text" {:font "shadowing_font"
                                       :layer "shadowing_layer"
                                       :material "shadowing_material"}}
                    "Landscape" {"box" {:layer "shadowing_layer"
                                        :material "shadowing_material"
                                        :texture "shadowing_texture/button_striped"}
                                 "particlefx" {:layer "shadowing_layer"
                                               :material "shadowing_material"
                                               :particlefx "shadowing_particlefx"}
                                 "pie" {:layer "shadowing_layer"
                                        :material "shadowing_material"
                                        :texture "shadowing_texture/button_striped"}
                                 "spine" {:layer "shadowing_layer"
                                          :material "shadowing_material"
                                          :spine-scene "shadowing_spinescene"}
                                 "text" {:font "shadowing_font"
                                         :layer "shadowing_layer"
                                         :material "shadowing_material"}}}
                   (make-layout->node->resource-field-values referenced-scene))))

          (testing "Referencing scene."
            (is (= {"Default" {"shadowing_resources" {:layer "shadowing_layer"}
                               "shadowing_resources/box" {:layer "shadowing_layer"
                                                          :material "shadowing_material"
                                                          :texture "shadowing_texture/button_striped"}
                               "shadowing_resources/particlefx" {:layer "shadowing_layer"
                                                                 :material "shadowing_material"
                                                                 :particlefx "shadowing_particlefx"}
                               "shadowing_resources/pie" {:layer "shadowing_layer"
                                                          :material "shadowing_material"
                                                          :texture "shadowing_texture/button_striped"}
                               "shadowing_resources/spine" {:layer "shadowing_layer"
                                                            :material "shadowing_material"
                                                            :spine-scene "shadowing_spinescene"}
                               "shadowing_resources/text" {:font "shadowing_font"
                                                           :layer "shadowing_layer"
                                                           :material "shadowing_material"}}
                    "Landscape" {"shadowing_resources/box" {:layer "shadowing_layer"
                                                            :material "shadowing_material"
                                                            :texture "shadowing_texture/button_striped"}
                                 "shadowing_resources/particlefx" {:layer "shadowing_layer"
                                                                   :material "shadowing_material"
                                                                   :particlefx "shadowing_particlefx"}
                                 "shadowing_resources/pie" {:layer "shadowing_layer"
                                                            :material "shadowing_material"
                                                            :texture "shadowing_texture/button_striped"}
                                 "shadowing_resources/spine" {:layer "shadowing_layer"
                                                              :material "shadowing_material"
                                                              :spine-scene "shadowing_spinescene"}
                                 "shadowing_resources/text" {:font "shadowing_font"
                                                             :layer "shadowing_layer"
                                                             :material "shadowing_material"}}}
                   (make-layout->node->resource-field-values referencing-scene)))))

        (testing "After renaming resources in the referenced scene."
          (with-open [_ (make-restore-point!)]

            ;; Rename all resources in the referenced scene.
            (let [button-font (gui-font referenced-scene "shadowing_font")
                  button-layer (gui-layer referenced-scene "shadowing_layer")
                  button-material (gui-material referenced-scene "shadowing_material")
                  button-particlefx (gui-particlefx-resource referenced-scene "shadowing_particlefx")
                  button-spinescene (gui-spine-scene referenced-scene "shadowing_spinescene")
                  button-texture (gui-texture referenced-scene "shadowing_texture")]
              (test-util/prop! button-font :name "shadowing_font_renamed")
              (test-util/prop! button-layer :name "shadowing_layer_renamed")
              (test-util/prop! button-material :name "shadowing_material_renamed")
              (test-util/prop! button-particlefx :name "shadowing_particlefx_renamed")
              (test-util/prop! button-spinescene :name "shadowing_spinescene_renamed")
              (test-util/prop! button-texture :name "shadowing_texture_renamed"))

            (testing "Referenced scene."
              (is (= {"Default" {"box" {:layer "shadowing_layer_renamed"
                                        :material "shadowing_material_renamed"
                                        :texture "shadowing_texture_renamed/button_striped"}
                                 "particlefx" {:layer "shadowing_layer_renamed"
                                               :material "shadowing_material_renamed"
                                               :particlefx "shadowing_particlefx_renamed"}
                                 "pie" {:layer "shadowing_layer_renamed"
                                        :material "shadowing_material_renamed"
                                        :texture "shadowing_texture_renamed/button_striped"}
                                 "spine" {:layer "shadowing_layer_renamed"
                                          :material "shadowing_material_renamed"
                                          :spine-scene "shadowing_spinescene_renamed"}
                                 "text" {:font "shadowing_font_renamed"
                                         :layer "shadowing_layer_renamed"
                                         :material "shadowing_material_renamed"}}
                      "Landscape" {"box" {:layer "shadowing_layer_renamed"
                                          :material "shadowing_material_renamed"
                                          :texture "shadowing_texture_renamed/button_striped"}
                                   "particlefx" {:layer "shadowing_layer_renamed"
                                                 :material "shadowing_material_renamed"
                                                 :particlefx "shadowing_particlefx_renamed"}
                                   "pie" {:layer "shadowing_layer_renamed"
                                          :material "shadowing_material_renamed"
                                          :texture "shadowing_texture_renamed/button_striped"}
                                   "spine" {:layer "shadowing_layer_renamed"
                                            :material "shadowing_material_renamed"
                                            :spine-scene "shadowing_spinescene_renamed"}
                                   "text" {:font "shadowing_font_renamed"
                                           :layer "shadowing_layer_renamed"
                                           :material "shadowing_material_renamed"}}}
                     (make-layout->node->resource-field-values referenced-scene))))

            (testing "Referencing scene."
              ;; After the renames in button.gui, we should not see any updated
              ;; references in panel.gui, since it has its own resources with
              ;; identical names shadowing the resources in button.gui.
              (is (= {"Default" {"shadowing_resources" {:layer "shadowing_layer"}
                                 "shadowing_resources/box" {:layer "shadowing_layer"
                                                            :material "shadowing_material"
                                                            :texture "shadowing_texture/button_striped"}
                                 "shadowing_resources/particlefx" {:layer "shadowing_layer"
                                                                   :material "shadowing_material"
                                                                   :particlefx "shadowing_particlefx"}
                                 "shadowing_resources/pie" {:layer "shadowing_layer"
                                                            :material "shadowing_material"
                                                            :texture "shadowing_texture/button_striped"}
                                 "shadowing_resources/spine" {:layer "shadowing_layer"
                                                              :material "shadowing_material"
                                                              :spine-scene "shadowing_spinescene"}
                                 "shadowing_resources/text" {:font "shadowing_font"
                                                             :layer "shadowing_layer"
                                                             :material "shadowing_material"}}
                      "Landscape" {"shadowing_resources/box" {:layer "shadowing_layer"
                                                              :material "shadowing_material"
                                                              :texture "shadowing_texture/button_striped"}
                                   "shadowing_resources/particlefx" {:layer "shadowing_layer"
                                                                     :material "shadowing_material"
                                                                     :particlefx "shadowing_particlefx"}
                                   "shadowing_resources/pie" {:layer "shadowing_layer"
                                                              :material "shadowing_material"
                                                              :texture "shadowing_texture/button_striped"}
                                   "shadowing_resources/spine" {:layer "shadowing_layer"
                                                                :material "shadowing_material"
                                                                :spine-scene "shadowing_spinescene"}
                                   "shadowing_resources/text" {:font "shadowing_font"
                                                               :layer "shadowing_layer"
                                                               :material "shadowing_material"}}}
                     (make-layout->node->resource-field-values referencing-scene))))))

        (testing "After renaming resources in the referencing scene."
          (with-open [_ (make-restore-point!)]

            ;; Rename all resources in the referencing scene.
            (let [panel-font (gui-font referencing-scene "shadowing_font")
                  panel-layer (gui-layer referencing-scene "shadowing_layer")
                  panel-material (gui-material referencing-scene "shadowing_material")
                  panel-particlefx (gui-particlefx-resource referencing-scene "shadowing_particlefx")
                  panel-spinescene (gui-spine-scene referencing-scene "shadowing_spinescene")
                  panel-texture (gui-texture referencing-scene "shadowing_texture")]
              (test-util/prop! panel-font :name "shadowing_font_renamed")
              (test-util/prop! panel-layer :name "shadowing_layer_renamed")
              (test-util/prop! panel-material :name "shadowing_material_renamed")
              (test-util/prop! panel-particlefx :name "shadowing_particlefx_renamed")
              (test-util/prop! panel-spinescene :name "shadowing_spinescene_renamed")
              (test-util/prop! panel-texture :name "shadowing_texture_renamed"))

            (testing "Referenced scene."
              (is (= {"Default" {"box" {:layer "shadowing_layer"
                                        :material "shadowing_material"
                                        :texture "shadowing_texture/button_striped"}
                                 "particlefx" {:layer "shadowing_layer"
                                               :material "shadowing_material"
                                               :particlefx "shadowing_particlefx"}
                                 "pie" {:layer "shadowing_layer"
                                        :material "shadowing_material"
                                        :texture "shadowing_texture/button_striped"}
                                 "spine" {:layer "shadowing_layer"
                                          :material "shadowing_material"
                                          :spine-scene "shadowing_spinescene"}
                                 "text" {:font "shadowing_font"
                                         :layer "shadowing_layer"
                                         :material "shadowing_material"}}
                      "Landscape" {"box" {:layer "shadowing_layer"
                                          :material "shadowing_material"
                                          :texture "shadowing_texture/button_striped"}
                                   "particlefx" {:layer "shadowing_layer"
                                                 :material "shadowing_material"
                                                 :particlefx "shadowing_particlefx"}
                                   "pie" {:layer "shadowing_layer"
                                          :material "shadowing_material"
                                          :texture "shadowing_texture/button_striped"}
                                   "spine" {:layer "shadowing_layer"
                                            :material "shadowing_material"
                                            :spine-scene "shadowing_spinescene"}
                                   "text" {:font "shadowing_font"
                                           :layer "shadowing_layer"
                                           :material "shadowing_material"}}}
                     (make-layout->node->resource-field-values referenced-scene))))

            (testing "Referencing scene."
              (is (= {"Default" {"shadowing_resources" {:layer "shadowing_layer_renamed"}
                                 "shadowing_resources/box" {:layer "shadowing_layer_renamed"
                                                            :material "shadowing_material_renamed"
                                                            :texture "shadowing_texture_renamed/button_striped"}
                                 "shadowing_resources/particlefx" {:layer "shadowing_layer_renamed"
                                                                   :material "shadowing_material_renamed"
                                                                   :particlefx "shadowing_particlefx_renamed"}
                                 "shadowing_resources/pie" {:layer "shadowing_layer_renamed"
                                                            :material "shadowing_material_renamed"
                                                            :texture "shadowing_texture_renamed/button_striped"}
                                 "shadowing_resources/spine" {:layer "shadowing_layer_renamed"
                                                              :material "shadowing_material_renamed"
                                                              :spine-scene "shadowing_spinescene_renamed"}
                                 "shadowing_resources/text" {:font "shadowing_font_renamed"
                                                             :layer "shadowing_layer_renamed"
                                                             :material "shadowing_material_renamed"}}
                      "Landscape" {"shadowing_resources/box" {:layer "shadowing_layer_renamed"
                                                              :material "shadowing_material_renamed"
                                                              :texture "shadowing_texture_renamed/button_striped"}
                                   "shadowing_resources/particlefx" {:layer "shadowing_layer_renamed"
                                                                     :material "shadowing_material_renamed"
                                                                     :particlefx "shadowing_particlefx_renamed"}
                                   "shadowing_resources/pie" {:layer "shadowing_layer_renamed"
                                                              :material "shadowing_material_renamed"
                                                              :texture "shadowing_texture_renamed/button_striped"}
                                   "shadowing_resources/spine" {:layer "shadowing_layer_renamed"
                                                                :material "shadowing_material_renamed"
                                                                :spine-scene "shadowing_spinescene_renamed"}
                                   "shadowing_resources/text" {:font "shadowing_font_renamed"
                                                               :layer "shadowing_layer_renamed"
                                                               :material "shadowing_material_renamed"}}}
                     (make-layout->node->resource-field-values referencing-scene))))))))))

(defn- round-num
  ^double [^double num]
  (math/round-with-precision num 0.001))

(defn- round-vec [double-vec]
  (into (empty double-vec)
        (map round-num)
        double-vec))

(defn- round-layout->node->field->value [layout->node->field->value]
  (coll/update-vals
    layout->node->field->value
    coll/update-vals
    coll/update-vals-kv
    (fn [field value]
      (case field
        (:alpha) (round-num value)
        (:position :rotation :scale) (round-vec value)
        value))))

(def ^:private gui-alpha-pb-field-index (gui/prop-key->pb-field-index :alpha))
(def ^:private gui-enabled-pb-field-index (gui/prop-key->pb-field-index :enabled))
(def ^:private gui-inherit-alpha-pb-field-index (gui/prop-key->pb-field-index :inherit-alpha))
(def ^:private gui-layer-pb-field-index (gui/prop-key->pb-field-index :layer))
(def ^:private gui-position-pb-field-index (gui/prop-key->pb-field-index :position))
(def ^:private gui-rotation-pb-field-index (gui/prop-key->pb-field-index :rotation))
(def ^:private gui-scale-pb-field-index (gui/prop-key->pb-field-index :scale))

(deftest template-props-baked-into-imported-node-descs-test
  (testing "Template children are reparented to the template node's parent."
    (test-util/with-temp-project-content
      {"/imported.gui"
       {:nodes
        [{:type :type-box
          :id "box"}
         {:type :type-box
          :id "box_child"
          :parent "box"}]}

       "/importing.gui"
       {:nodes
        [{:type :type-box
          :id "imported_parent"}
         {:type :type-template
          :id "imported"
          :parent "imported_parent"
          :template "/imported.gui"}
         {:type :type-box
          :template-node-child true
          :id "imported/box"
          :parent "imported"}
         {:type :type-box
          :template-node-child true
          :id "imported/box_child"
          :parent "imported/box"}]

        :layouts
        [{:name "Unaltered"}]}}

      (is (= {"Default" {"imported_parent" {}
                         "imported/box" {:parent "imported_parent"}
                         "imported/box_child" {:parent "imported/box"}}
              "Unaltered" {"imported_parent" {}
                           "imported/box" {:parent "imported_parent"}
                           "imported/box_child" {:parent "imported/box"}}}
             (-> (project/get-resource-node project "/importing.gui")
                 (make-built-layout->node->field->value))))))

  (testing "Template may disable imported children."
    (test-util/with-temp-project-content
      {"/off.gui"
       {:nodes
        [{:type :type-box
          :id "box"
          :enabled false}]}

       "/on.gui"
       {:nodes
        [{:type :type-box
          :id "box"}]}

       "/importing.gui"
       {:nodes
        [{:type :type-template
          :id "disabled_off"
          :template "/off.gui"
          :enabled false}
         {:type :type-box
          :template-node-child true
          :id "disabled_off/box"
          :parent "disabled_off"}
         {:type :type-template
          :id "enabled_off"
          :template "/off.gui"}
         {:type :type-box
          :template-node-child true
          :id "enabled_off/box"
          :parent "enabled_off"}
         {:type :type-template
          :id "disabled_on"
          :template "/on.gui"
          :enabled false}
         {:type :type-box
          :template-node-child true
          :id "disabled_on/box"
          :parent "disabled_on"}
         {:type :type-template
          :id "enabled_on"
          :template "/on.gui"}
         {:type :type-box
          :template-node-child true
          :id "enabled_on/box"
          :parent "enabled_on"}]

        :layouts
        [{:name "Unaltered"}
         {:name "Altered Parent"
          :nodes
          [{:type :type-template
            :id "disabled_off"
            :template "/off.gui"
            :overridden-fields [gui-enabled-pb-field-index]}
           {:type :type-template
            :id "enabled_off"
            :template "/off.gui"
            :enabled false
            :overridden-fields [gui-enabled-pb-field-index]}
           {:type :type-template
            :id "disabled_on"
            :template "/on.gui"
            :overridden-fields [gui-enabled-pb-field-index]}
           {:type :type-template
            :id "enabled_on"
            :template "/on.gui"
            :enabled false
            :overridden-fields [gui-enabled-pb-field-index]}]}
         {:name "Altered Child"
          :nodes
          [{:type :type-box
            :template-node-child true
            :id "disabled_off/box"
            :parent "disabled_off"
            :overridden-fields [gui-enabled-pb-field-index]}
           {:type :type-box
            :template-node-child true
            :id "enabled_off/box"
            :parent "enabled_off"
            :overridden-fields [gui-enabled-pb-field-index]}
           {:type :type-box
            :template-node-child true
            :id "disabled_on/box"
            :parent "disabled_on"
            :enabled false
            :overridden-fields [gui-enabled-pb-field-index]}
           {:type :type-box
            :template-node-child true
            :id "enabled_on/box"
            :parent "enabled_on"
            :enabled false
            :overridden-fields [gui-enabled-pb-field-index]}]}]}}

      (is (= {"Default" {"disabled_off/box" {:enabled false}
                         "enabled_off/box" {:enabled false}
                         "disabled_on/box" {:enabled false}
                         "enabled_on/box" {}}
              "Unaltered" {"disabled_off/box" {:enabled false}
                           "enabled_off/box" {:enabled false}
                           "disabled_on/box" {:enabled false}
                           "enabled_on/box" {}}
              "Altered Parent" {"disabled_off/box" {:enabled false}
                                "enabled_off/box" {:enabled false}
                                "disabled_on/box" {}
                                "enabled_on/box" {:enabled false}}
              "Altered Child" {"disabled_off/box" {:enabled false}
                               "enabled_off/box" {}
                               "disabled_on/box" {:enabled false}
                               "enabled_on/box" {:enabled false}}}
             (-> (project/get-resource-node project "/importing.gui")
                 (make-built-layout->node->field->value))))))

  (testing "Template layer is applied to imported nodes with no layer specified."
    (test-util/with-temp-project-content
      {"/unlayered.gui"
       {:nodes
        [{:type :type-box
          :id "box"}]}

       "/layered.gui"
       {:layers
        [{:name "imported_layer"}]

        :nodes
        [{:type :type-box
          :id "box"
          :layer "imported_layer"}]}

       "/importing.gui"
       {:layers
        [{:name "importing_layer"}
         {:name "other_importing_layer"}]

        :nodes
        [{:type :type-template
          :id "unlayered"
          :template "/unlayered.gui"
          :layer "importing_layer"}
         {:type :type-box
          :template-node-child true
          :id "unlayered/box"
          :parent "unlayered"}
         {:type :type-template
          :id "layered"
          :template "/layered.gui"
          :layer "importing_layer"}
         {:type :type-box
          :template-node-child true
          :id "layered/box"
          :parent "layered"}]

        :layouts
        [{:name "Unaltered"}
         {:name "Altered Parent"
          :nodes
          [{:type :type-template
            :id "unlayered"
            :template "/unlayered.gui"
            :layer "other_importing_layer"
            :overridden-fields [gui-layer-pb-field-index]}
           {:type :type-template
            :id "layered"
            :template "/layered.gui"
            :layer "other_importing_layer"
            :overridden-fields [gui-layer-pb-field-index]}]}
         {:name "Altered Child"
          :nodes
          [{:type :type-box
            :template-node-child true
            :id "unlayered/box"
            :parent "unlayered"
            :layer "other_importing_layer"
            :overridden-fields [gui-layer-pb-field-index]}
           {:type :type-box
            :template-node-child true
            :id "layered/box"
            :parent "layered"
            :layer "other_importing_layer"
            :overridden-fields [gui-layer-pb-field-index]}]}]}}

      (is (= {"Default" {"unlayered/box" {:layer "importing_layer"}
                         "layered/box" {:layer "imported_layer"}}
              "Unaltered" {"unlayered/box" {:layer "importing_layer"}
                           "layered/box" {:layer "imported_layer"}}
              "Altered Parent" {"unlayered/box" {:layer "other_importing_layer"}
                                "layered/box" {:layer "imported_layer"}}
              "Altered Child" {"unlayered/box" {:layer "other_importing_layer"}
                               "layered/box" {:layer "other_importing_layer"}}}
             (-> (project/get-resource-node project "/importing.gui")
                 (make-built-layout->node->field->value))))))

  (testing "Template alpha is applied to imported nodes."
    (test-util/with-temp-project-content
      {"/forsaking.gui"
       {:nodes
        [{:type :type-box
          :id "box"
          :alpha 0.5
          :inherit-alpha false}]}

       "/inheriting.gui"
       {:nodes
        [{:type :type-box
          :id "box"
          :alpha 0.5
          :inherit-alpha true}]}

       "/importing.gui"
       {:nodes
        [{:type :type-template
          :id "forsaking"
          :template "/forsaking.gui"
          :alpha 0.5}
         {:type :type-box
          :template-node-child true
          :id "forsaking/box"
          :parent "forsaking"}
         {:type :type-template
          :id "inheriting"
          :template "/inheriting.gui"
          :alpha 0.5}
         {:type :type-box
          :template-node-child true
          :id "inheriting/box"
          :parent "inheriting"}]

        :layouts
        [{:name "Unaltered"}
         {:name "Altered Parent"
          :nodes
          [{:type :type-template
            :id "forsaking"
            :template "/forsaking.gui"
            :alpha 0.75
            :overridden-fields [gui-alpha-pb-field-index]}
           {:type :type-template
            :id "inheriting"
            :template "/inheriting.gui"
            :alpha 0.75
            :overridden-fields [gui-alpha-pb-field-index]}]}
         {:name "Altered Child"
          :nodes
          [{:type :type-box
            :template-node-child true
            :id "forsaking/box"
            :parent "forsaking"
            :alpha 0.75
            :overridden-fields [gui-alpha-pb-field-index]}
           {:type :type-box
            :template-node-child true
            :id "inheriting/box"
            :parent "inheriting"
            :alpha 0.75
            :overridden-fields [gui-alpha-pb-field-index]}]}]}}

      (is (= {"Default" {"inheriting/box" {:alpha 0.25}
                         "forsaking/box" {:alpha 0.5}}
              "Unaltered" {"inheriting/box" {:alpha 0.25}
                           "forsaking/box" {:alpha 0.5}}
              "Altered Parent" {"inheriting/box" {:alpha 0.375}
                                "forsaking/box" {:alpha 0.5}}
              "Altered Child" {"inheriting/box" {:alpha 0.375}
                               "forsaking/box" {:alpha 0.75}}}
             (-> (project/get-resource-node project "/importing.gui")
                 (make-built-layout->node->field->value)
                 (round-layout->node->field->value))))))

  (testing "Overriding inherit-alpha."
    (test-util/with-temp-project-content
      {"/forsaking.gui"
       {:nodes
        [{:type :type-box
          :id "box"
          :inherit-alpha false}]}

       "/inheriting.gui"
       {:nodes
        [{:type :type-box
          :id "box"
          :inherit-alpha true}]}

       "/importing.gui"
       {:nodes
        [{:type :type-template
          :id "forsaking"
          :template "/forsaking.gui"
          :alpha 0.5}
         {:type :type-box
          :template-node-child true
          :id "forsaking/box"
          :parent "forsaking"}
         {:type :type-template
          :id "not_forsaking"
          :template "/forsaking.gui"
          :alpha 0.5}
         {:type :type-box
          :template-node-child true
          :id "not_forsaking/box"
          :parent "not_forsaking"
          :inherit-alpha true
          :overridden-fields [gui-inherit-alpha-pb-field-index]}
         {:type :type-template
          :id "inheriting"
          :template "/inheriting.gui"
          :alpha 0.5}
         {:type :type-box
          :template-node-child true
          :id "inheriting/box"
          :parent "inheriting"}
         {:type :type-template
          :id "not_inheriting"
          :template "/inheriting.gui"
          :alpha 0.5}
         {:type :type-box
          :template-node-child true
          :id "not_inheriting/box"
          :parent "not_inheriting"
          :inherit-alpha false
          :overridden-fields [gui-inherit-alpha-pb-field-index]}]

        :layouts
        [{:name "Unaltered"}
         {:name "Altered Child"
          :nodes
          [{:type :type-box
            :template-node-child true
            :id "forsaking/box"
            :parent "forsaking"
            :inherit-alpha true
            :overridden-fields [gui-inherit-alpha-pb-field-index]}
           {:type :type-box
            :template-node-child true
            :id "not_forsaking/box"
            :parent "not_forsaking"
            :inherit-alpha false
            :overridden-fields [gui-inherit-alpha-pb-field-index]}
           {:type :type-box
            :template-node-child true
            :id "inheriting/box"
            :parent "inheriting"
            :inherit-alpha false
            :overridden-fields [gui-inherit-alpha-pb-field-index]}
           {:type :type-box
            :template-node-child true
            :id "not_inheriting/box"
            :parent "not_inheriting"
            :inherit-alpha true
            :overridden-fields [gui-inherit-alpha-pb-field-index]}]}]}}

      (is (= {"Default" {"inheriting/box" {:alpha 0.5}
                         "not_inheriting/box" {}
                         "forsaking/box" {}
                         "not_forsaking/box" {:alpha 0.5}}
              "Unaltered" {"inheriting/box" {:alpha 0.5}
                           "not_inheriting/box" {}
                           "forsaking/box" {}
                           "not_forsaking/box" {:alpha 0.5}}
              "Altered Child" {"inheriting/box" {}
                               "not_inheriting/box" {:alpha 0.5}
                               "forsaking/box" {:alpha 0.5}
                               "not_forsaking/box" {}}}
             (-> (project/get-resource-node project "/importing.gui")
                 (make-built-layout->node->field->value)
                 (round-layout->node->field->value))))))

  (testing "Template transform is applied to imported node transforms."
    (test-util/with-temp-project-content
      {"/untransformed.gui"
       {:nodes
        [{:type :type-box
          :id "box"}]}

       "/transformed.gui"
       {:nodes
        [{:type :type-box
          :id "box"
          :position [10.0 0.0 0.0 1.0]
          :rotation [0.0 0.0 90.0 0.0]
          :scale [2.0 2.0 1.0 1.0]}]}

       "/importing.gui"
       {:nodes
        [{:type :type-template
          :id "untransformed"
          :template "/untransformed.gui"
          :position [10.0 0.0 0.0 1.0]
          :rotation [0.0 0.0 90.0 0.0]
          :scale [2.0 2.0 1.0 1.0]}
         {:type :type-box
          :template-node-child true
          :id "untransformed/box"
          :parent "untransformed"}
         {:type :type-template
          :id "transformed"
          :template "/transformed.gui"
          :position [10.0 0.0 0.0 1.0]
          :rotation [0.0 0.0 90.0 0.0]
          :scale [2.0 2.0 1.0 1.0]}
         {:type :type-box
          :template-node-child true
          :id "transformed/box"
          :parent "transformed"}]

        :layouts
        [{:name "Unaltered"}
         {:name "Altered Parent"
          :nodes
          [{:type :type-template
            :id "untransformed"
            :template "/untransformed.gui"
            :position [20.0 0.0 0.0 1.0]
            :rotation [0.0 0.0 180.0 0.0]
            :scale [3.0 3.0 1.0 1.0]
            :overridden-fields [gui-position-pb-field-index
                                gui-rotation-pb-field-index
                                gui-scale-pb-field-index]}
           {:type :type-template
            :id "transformed"
            :template "/transformed.gui"
            :position [20.0 0.0 0.0 1.0]
            :rotation [0.0 0.0 180.0 0.0]
            :scale [3.0 3.0 1.0 1.0]
            :overridden-fields [gui-position-pb-field-index
                                gui-rotation-pb-field-index
                                gui-scale-pb-field-index]}]}
         {:name "Altered Child"
          :nodes
          [{:type :type-box
            :template-node-child true
            :id "untransformed/box"
            :parent "untransformed"
            :position [20.0 0.0 0.0 1.0]
            :rotation [0.0 0.0 180.0 0.0]
            :scale [3.0 3.0 1.0 1.0]
            :overridden-fields [gui-position-pb-field-index
                                gui-rotation-pb-field-index
                                gui-scale-pb-field-index]}
           {:type :type-box
            :template-node-child true
            :id "transformed/box"
            :parent "transformed"
            :position [20.0 0.0 0.0 1.0]
            :rotation [0.0 0.0 180.0 0.0]
            :scale [3.0 3.0 1.0 1.0]
            :overridden-fields [gui-position-pb-field-index
                                gui-rotation-pb-field-index
                                gui-scale-pb-field-index]}]}]}}

      (is (= {"Default" {"transformed/box" {:position [10.0 20.0 0.0 1.0]
                                            :rotation [0.0 0.0 180.0 0.0]
                                            :scale [4.0 4.0 1.0 1.0]}
                         "untransformed/box" {:position [10.0 0.0 0.0 1.0]
                                              :rotation [0.0 0.0 90.0 0.0]
                                              :scale [2.0 2.0 1.0 1.0]}}
              "Unaltered" {"transformed/box" {:position [10.0 20.0 0.0 1.0]
                                              :rotation [0.0 0.0 180.0 0.0]
                                              :scale [4.0 4.0 1.0 1.0]}
                           "untransformed/box" {:position [10.0 0.0 0.0 1.0]
                                                :rotation [0.0 0.0 90.0 0.0]
                                                :scale [2.0 2.0 1.0 1.0]}}
              "Altered Parent" {"transformed/box" {:position [-10.0 0.0 0.0 1.0]
                                                   :rotation [0.0 0.0 270.0 0.0]
                                                   :scale [6.0 6.0 1.0 1.0]}
                                "untransformed/box" {:position [20.0 0.0 0.0 1.0]
                                                     :rotation [0.0 0.0 180.0 0.0]
                                                     :scale [3.0 3.0 1.0 1.0]}}
              "Altered Child" {"transformed/box" {:position [10.0 40.0 0.0 1.0]
                                                  :rotation [0.0 0.0 270.0 0.0]
                                                  :scale [6.0 6.0 1.0 1.0]}
                               "untransformed/box" {:position [10.0 40.0 0.0 1.0]
                                                    :rotation [0.0 0.0 270.0 0.0]
                                                    :scale [6.0 6.0 1.0 1.0]}}}
             (-> (project/get-resource-node project "/importing.gui")
                 (make-built-layout->node->field->value)
                 (round-layout->node->field->value)))))))
