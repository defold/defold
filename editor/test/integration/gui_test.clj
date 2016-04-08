(ns integration.gui-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system tx-nodes]]
            [integration.test-util :as test-util]
            [editor.workspace :as workspace]
            [editor.defold-project :as project]
            [editor.gui :as gui]
            [editor.gl.pass :as pass]
            [editor.handler :as handler]
            [editor.types :as types])
  (:import [java.io File]
           [java.nio.file Files attribute.FileAttribute]
           [javax.vecmath Point3d Matrix4d Vector3d]
           [org.apache.commons.io FilenameUtils FileUtils]))

(defn- prop [node-id label]
  (test-util/prop node-id label))

(defn- prop! [node-id label val]
  (test-util/prop! node-id label val))

(defn- gui-node [scene id]
  (let [id->node (->> (get-in (g/node-value scene :node-outline) [:children 0])
                   (tree-seq (constantly true) :children)
                   (map :node-id)
                   (map (fn [node-id] [(g/node-value node-id :id) node-id]))
                   (into {}))]
    (id->node id)))

(defn- gui-layer [scene id]
  (get (g/node-value scene :layer-ids) id))

(deftest load-gui
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project   (test-util/setup-project! workspace)
          node-id   (test-util/resource-node project "/logic/main.gui")
          gui-node (ffirst (g/sources-of node-id :node-outlines))])))

(deftest gui-scene-generation
 (with-clean-system
   (let [workspace (test-util/setup-workspace! world)
         project   (test-util/setup-project! workspace)
         node-id   (test-util/resource-node project "/logic/main.gui")
         scene (g/node-value node-id :scene)]
     (is (= 0.25 (get-in scene [:children 0 :children 2 :children 0 :renderable :user-data :color 3]))))))

(deftest gui-scene-pie
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project   (test-util/setup-project! workspace)
          node-id   (test-util/resource-node project "/logic/main.gui")
          scene (g/node-value node-id :scene)]
      (is (> (count (get-in scene [:children 0 :children 3 :renderable :user-data :line-data])) 0)))))

(deftest gui-textures
  (with-clean-system
   (let [workspace (test-util/setup-workspace! world)
         project   (test-util/setup-project! workspace)
         node-id   (test-util/resource-node project "/logic/main.gui")
         outline (g/node-value node-id :node-outline)
         png-node (get-in outline [:children 0 :children 1 :node-id])
         png-tex (get-in outline [:children 1 :children 0 :node-id])]
     (is (some? png-tex))
     (is (= "png_texture" (prop png-node :texture)))
     (prop! png-tex :name "new-name")
     (is (= "new-name" (prop png-node :texture))))))

(deftest gui-atlas
  (with-clean-system
   (let [workspace (test-util/setup-workspace! world)
         project   (test-util/setup-project! workspace)
         node-id   (test-util/resource-node project "/logic/main.gui")
         outline (g/node-value node-id :node-outline)
         box (get-in outline [:children 0 :children 2 :node-id])
         atlas-tex (get-in outline [:children 1 :children 1 :node-id])]
     (is (some? atlas-tex))
     (is (= "atlas_texture/anim" (prop box :texture)))
     (prop! atlas-tex :name "new-name")
     (is (= "new-name/anim" (prop box :texture))))))

(deftest gui-shaders
  (with-clean-system
   (let [workspace (test-util/setup-workspace! world)
         project   (test-util/setup-project! workspace)
         node-id   (test-util/resource-node project "/logic/main.gui")]
     (is (some? (g/node-value node-id :material-shader))))))

(defn- font-resource-node [project gui-font-node]
  (project/get-resource-node project (g/node-value gui-font-node :font)))

(defn- build-targets-deps [gui-scene-node]
  (map :node-id (:deps (first (g/node-value gui-scene-node :build-targets)))))

(deftest gui-fonts
  (with-clean-system
   (let [workspace (test-util/setup-workspace! world)
         project   (test-util/setup-project! workspace)
         gui-scene-node   (test-util/resource-node project "/logic/main.gui")
         outline (g/node-value gui-scene-node :node-outline)
         gui-font-node (get-in outline [:children 2 :children 0 :node-id])
         old-font (font-resource-node project gui-font-node)
         new-font (project/get-resource-node project "/fonts/big_score.font")]
     (is (some? (g/node-value gui-font-node :font-map)))
     (is (some #{old-font} (build-targets-deps gui-scene-node)))
     (g/transact (g/set-property gui-font-node :font (g/node-value new-font :resource)))
     (is (not (some #{old-font} (build-targets-deps gui-scene-node))))
     (is (some #{new-font} (build-targets-deps gui-scene-node))))))

(deftest gui-text-node
  (with-clean-system
   (let [workspace (test-util/setup-workspace! world)
         project   (test-util/setup-project! workspace)
         node-id   (test-util/resource-node project "/logic/main.gui")
         outline (g/node-value node-id :node-outline)
         nodes (into {} (map (fn [item] [(:label item) (:node-id item)]) (get-in outline [:children 0 :children])))
         text-node (get nodes "hexagon_text")]
     (is (= false (g/node-value text-node :line-break))))))

(defn- render-order [view]
  (let [renderables (g/node-value view :renderables)]
    (->> (get renderables pass/transparent)
      (map :node-id)
      (filter #(and (some? %) (g/node-instance? gui/GuiNode %)))
      (map #(g/node-value % :id))
      vec)))

(deftest gui-layers
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          app-view (test-util/setup-app-view!)
          node-id (test-util/resource-node project "/gui/layers.gui")
          view (test-util/open-scene-view! project app-view node-id 16 16)]
      (is (= ["box" "pie" "box1" "text"] (render-order view)))
      (g/set-property! (gui-node node-id "box") :layer "layer1")
      (is (= ["pie" "box1" "box" "text"] (render-order view)))
      (g/set-property! (gui-node node-id "box") :layer "")
      (is (= ["box" "pie" "box1" "text"] (render-order view))))))

;; Templates

(deftest gui-templates
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          app-view (test-util/setup-app-view!)
          node-id (test-util/resource-node project "/gui/scene.gui")
          original-template (test-util/resource-node project "/gui/sub_scene.gui")
          tmpl-node (gui-node node-id "sub_scene")
          path [:children 0 :children 0 :node-id]]
      (is (not= (get-in (g/node-value tmpl-node :scene) path)
                (get-in (g/node-value original-template :scene) path))))))

(deftest gui-template-ids
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          app-view (test-util/setup-app-view!)
          node-id (test-util/resource-node project "/gui/scene.gui")
          original-template (test-util/resource-node project "/gui/sub_scene.gui")
          tmpl-node (gui-node node-id "sub_scene")
          old-name "sub_scene/sub_box"
          new-name "sub_scene2/sub_box"]
      (is (not (nil? (gui-node node-id old-name))))
      (is (nil? (gui-node node-id new-name)))
      (g/transact (g/set-property tmpl-node :id "sub_scene2"))
      (is (not (nil? (gui-node node-id new-name))))
      (is (nil? (gui-node node-id old-name)))
      (is (false? (get-in (g/node-value tmpl-node :_declared-properties) [:properties :id :read-only?])))
      (let [sub-node (gui-node node-id new-name)
            props (get (g/node-value sub-node :_declared-properties) :properties)]
        (is (= new-name (prop sub-node :id)))
        (is (= (g/node-value sub-node :id)
               (get-in props [:id :value])))
        (is (true? (get-in props [:id :read-only?])))))))

(deftest gui-templates-complex-property
 (with-clean-system
   (let [workspace (test-util/setup-workspace! world)
         project (test-util/setup-project! workspace)
         app-view (test-util/setup-app-view!)
         node-id (test-util/resource-node project "/gui/scene.gui")
         sub-node (gui-node node-id "sub_scene/sub_box")]
     (let [color (prop sub-node :color)
           alpha (prop sub-node :alpha)]
       (g/transact (g/set-property sub-node :alpha (* 0.5 alpha)))
       (is (not= color (prop sub-node :color)))
       (is (not= alpha (prop sub-node :alpha)))
       (g/transact (g/clear-property sub-node :alpha))
       (is (= color (prop sub-node :color)))
       (is (= alpha (prop sub-node :alpha)))))))

(deftest gui-template-hierarchy
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          app-view (test-util/setup-app-view!)
          node-id (test-util/resource-node project "/gui/super_scene.gui")
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
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          app-view (test-util/setup-app-view!)
          node-id (test-util/resource-node project "/gui/super_scene.gui")
          tmpl-node (gui-node node-id "scene/sub_scene")]
      (project/select! project [tmpl-node])
      (let [props (g/node-value project :selected-node-properties)]
        (is (not (empty? (keys props))))))))

(deftest gui-template-set-leak
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          app-view (test-util/setup-app-view!)
          node-id (test-util/resource-node project "/gui/scene.gui")
          sub-node (test-util/resource-node project "/gui/sub_scene.gui")
          tmpl-node (gui-node node-id "sub_scene")]
      (is (= 1 (count (g/overrides sub-node))))
      (g/transact (g/set-property tmpl-node :template {:resource (workspace/find-resource workspace "/gui/layers.gui") :overrides {}}))
      (is (= 0 (count (g/overrides sub-node)))))))

(defn- options [node-id prop]
  (vec (vals (get-in (g/node-value node-id :_properties) [:properties prop :edit-type :options]))))

(deftest gui-template-dynamics
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          app-view (test-util/setup-app-view!)
          node-id (test-util/resource-node project "/gui/super_scene.gui")
          box (gui-node node-id "scene/box")
          text (gui-node node-id "scene/text")]
      (is (= ["" "main_super/particle_blob"] (options box :texture)))
      (is (= ["" "layer"] (options text :layer)))
      (is (= ["" "system_font_super"] (options text :font)))
      (g/transact (g/set-property text :layer "layer"))
      (let [l (gui-layer node-id "layer")]
        (g/transact (g/set-property l :name "new-name"))
        (is (= "new-name" (prop text :layer)))))))

(defn- strip-scene [scene]
  (-> scene
    (select-keys [:node-id :children :renderable])
    (update :children (fn [c] (mapv #(strip-scene %) c)))
    (update-in [:renderable :user-data] select-keys [:color])
    (update :renderable select-keys [:user-data])))

(defn- scene-by-nid [root-id node-id]
  (let [scene (g/node-value root-id :scene)
        scenes (into {} (map (fn [s] [(:node-id s) s]) (tree-seq (constantly true) :children (strip-scene scene))))]
    (scenes node-id)))

(deftest gui-template-alpha
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project   (test-util/setup-project! workspace)
          node-id   (test-util/resource-node project "/gui/super_scene.gui")
          scene-fn (comp (partial scene-by-nid node-id) (partial gui-node node-id))]
      (is (= 1.0 (get-in (scene-fn "scene/box") [:renderable :user-data :color 3])))
      (g/transact
        (concat
          (g/set-property (gui-node node-id "scene") :alpha 0.5)))
      (is (= 0.5 (get-in (scene-fn "scene/box") [:renderable :user-data :color 3]))))))

(deftest gui-template-reload
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          node-id (test-util/resource-node project "/gui/super_scene.gui")
          template (gui-node node-id "scene")
          box (gui-node node-id "scene/box")]
      (g/transact (g/set-property box :position [-100.0 0.0 0.0]))
      (is (= -100.0 (get-in (g/node-value template :_properties) [:properties :template :value :overrides "box" :position 0])))
      (use 'editor.gui :reload)
      (is (= -100.0 (get-in (g/node-value template :_properties) [:properties :template :value :overrides "box" :position 0]))))))

(deftest gui-template-add
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          node-id (test-util/resource-node project "/gui/scene.gui")
          super (test-util/resource-node project "/gui/super_scene.gui")
          parent (:node-id (test-util/outline node-id [0]))
          [new-tmpl] (g/tx-nodes-added (gui/add-gui-node! project node-id parent :type-template))
          super-template (gui-node super "scene")]
      (is (= new-tmpl (gui-node node-id "template")))
      (is (not (contains? (:overrides (prop super-template :template)) "template/sub_box")))
      (prop! new-tmpl :template {:resource (workspace/resolve-workspace-resource workspace "/gui/sub_scene.gui") :overrides {}})
      (is (contains? (:overrides (prop super-template :template)) "template/sub_box")))))

(deftest gui-layout
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          node-id (test-util/resource-node project "/gui/scene.gui")]
      (is (= ["Landscape"] (map :name (:layouts (g/node-value node-id :pb-msg))))))))

(defn- max-x [scene]
  (.getX ^Point3d (:max (:aabb scene))))

(defn- add-layout! [project scene name]
  (let [parent (g/node-value scene :layouts-node)
        user-data {:scene scene :parent parent :display-profile name :handler-fn gui/add-layout-handler}]
    (handler/run :add [{:name :global :env {:selection [parent] :project project :user-data user-data}}] user-data)))

(defn- set-visible-layout! [scene layout]
  (g/transact (g/set-property scene :visible-layout layout)))

(deftest gui-layout-active
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          node-id (test-util/resource-node project "/gui/layouts.gui")
          box (gui-node node-id "box")
          dims (g/node-value node-id :scene-dims)
          scene (g/node-value node-id :scene)]
      (set-visible-layout! node-id "Landscape")
      (let [new-box (gui-node node-id "box")]
        (is (and new-box (not= box new-box))))
      (let [new-dims (g/node-value node-id :scene-dims)]
        (is (and new-dims (not= dims new-dims))))
      (let [new-scene (g/node-value node-id :scene)]
        (is (not= (max-x scene) (max-x new-scene)))))))

(deftest gui-layout-add
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          node-id (test-util/resource-node project "/gui/layouts.gui")
          box (gui-node node-id "box")]
      (add-layout! project node-id "Portrait")
      (set-visible-layout! node-id "Portrait")
      (is (not= box (gui-node node-id "box"))))))

(defn- gui-text [scene id]
  (-> (gui-node scene id)
    (g/node-value :text)))

(defn- trans-x [root-id target-id]
  (let [s (tree-seq (constantly true) :children (g/node-value root-id :scene))]
    (when-let [^Matrix4d m (some #(and (= (:node-id %) target-id) (:transform %)) s)]
      (let [t (Vector3d.)]
        (.get m t)
        (.getX t)))))

(deftest gui-layout-template
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          node-id (test-util/resource-node project "/gui/super_scene.gui")]
      (testing "regular layout override, through templates"
               (is (= "Test" (gui-text node-id "scene/text")))
               (set-visible-layout! node-id "Landscape")
               (is (= "Testing Text" (gui-text node-id "scene/text"))))
      (testing "scene generation"
               (is (= 1280.0 (max-x (g/node-value node-id :scene))))
               (set-visible-layout! node-id "Portrait")
               (is (= 720.0 (max-x (g/node-value node-id :scene))))))))
