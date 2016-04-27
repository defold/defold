(ns editor.gui.loader
  (:require [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.gui.fonts :as fonts]
            [editor.gui.gui-scene-node :as gsn]
            [editor.gui.layers :as layers]
            [editor.gui.layouts :as layouts]
            [editor.gui.nodes :as nodes]
            [editor.gui.shapes :as shapes]
            [editor.gui.templates :as templates]
            [editor.gui.text :as text]
            [editor.gui.textures :as textures]
            [editor.gui.util :as gutil]
            [editor.progress :as progress]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.workspace :as workspace])
  (:import [com.dynamo.gui.proto Gui$NodeDesc]))

(defn- v4->v3 [v4]
  (subvec v4 0 3))

(defn- color-alpha [node-desc color-field alpha-field]
  (let [color (get node-desc color-field)
        alpha (if (protobuf/field-set? node-desc alpha-field) (get node-desc alpha-field) (get color 3))]
    (conj (subvec color 0 3) alpha)))

(defn- extract-overrides [node-desc]
  (select-keys node-desc (map (protobuf/fields-by-indices Gui$NodeDesc) (:overridden-fields node-desc))))

(def node-property-fns (-> {}
                         (into (map (fn [label] [label [label (comp v4->v3 label)]]) [:position :rotation :scale :size]))
                         (into (map (fn [[label alpha]] [label [label (fn [n] (color-alpha n label alpha))]])
                                    [[:color :alpha]
                                     [:shadow :shadow-alpha]
                                     [:outline :outline-alpha]]))
                         (into (map (fn [[ddf-label label]] [ddf-label [label ddf-label]]) [[:xanchor :x-anchor]
                                                                                            [:yanchor :y-anchor]]))))


(defn- convert-node-desc [node-desc]
  (into {} (map (fn [[key val]] (let [[new-key f] (get node-property-fns key [key key])]
                                  [new-key (f node-desc)])) node-desc)))

(defn- tx-create-node? [tx-entry]
  (= :create-node (:type tx-entry)))

(defn- tx-node-id [tx-entry]
  (get-in tx-entry [:node :_node-id]))

(defn load-gui-scene [project self input]
  (let [def                gutil/pb-def
        scene              (protobuf/read-text (:pb-class def) input)
        resource           (g/node-value self :resource)
        resolve-fn         (partial workspace/resolve-resource resource)
        workspace          (project/workspace project)
        graph-id           (g/node-id->graph-id self)
        node-descs         (map convert-node-desc (:nodes scene))
        tmpl-node-descs    (into {} (map (fn [n] [(:id n) {:template (:parent n) :data (extract-overrides n)}])
                                         (filter :template-node-child node-descs)))
        tmpl-node-descs    (into {} (map (fn [[id data]]
                                           [id (update data :template
                                                       (fn [parent] (if (contains? tmpl-node-descs parent)
                                                                      (recur (:template (get tmpl-node-descs parent)))
                                                                      parent)))])
                                         tmpl-node-descs))
        node-descs         (filter (complement :template-node-child) node-descs)
        tmpl-children      (group-by (comp :template second) tmpl-node-descs)
        tmpl-roots         (filter (complement tmpl-node-descs) (map first tmpl-children))
        template-data      (into {} (map (fn [r] [r (into {} (map (fn [[id tmpl]]
                                                                    [(subs id (inc (count r))) (:data tmpl)])
                                                                  (rest (tree-seq (constantly true)
                                                                                  (comp tmpl-children first)
                                                                                  [r nil]))))])
                                         tmpl-roots))
        template-resources (map (comp resolve-fn :template) (filter #(= :type-template (:type %)) node-descs))
        texture-resources  (map (comp resolve-fn :texture) (:textures scene))
        scene-load-data    (project/load-resource-nodes project
                                                        (->> (concat template-resources texture-resources)
                                                             (map #(project/get-resource-node project %))
                                                             (remove nil?))
                                                        progress/null-render-progress!)]
    (concat
      scene-load-data
      (g/set-property self :script (workspace/resolve-resource resource (:script scene)))
      (g/set-property self :material (workspace/resolve-resource resource (:material scene)))
      (g/set-property self :adjust-reference (:adjust-reference scene))
      (g/set-property self :pb scene)
      (g/set-property self :def def)
      (g/set-property self :background-color (:background-color scene))
      (g/connect project :settings self :project-settings)
      (g/make-nodes graph-id [fonts-node fonts/FontsNode]
                    (g/connect fonts-node :_node-id self :fonts-node)
                    (g/connect fonts-node :_node-id self :nodes)
                    (g/connect fonts-node :node-outline self :child-outlines)
                    (g/make-nodes graph-id [font [fonts/FontNode
                                                  :name ""
                                                  :font (workspace/resolve-resource resource "/builtins/fonts/system_font.font")]]
                                  (fonts/attach-font self fonts-node font true))
                    (for [font-desc (:fonts scene)]
                      (g/make-nodes graph-id [font [fonts/FontNode
                                                    :name (:name font-desc)
                                                    :font (workspace/resolve-resource resource (:font font-desc))]]
                                    (fonts/attach-font self fonts-node font))))
      (g/make-nodes graph-id [textures-node textures/TexturesNode]
                    (g/connect textures-node :_node-id self :textures-node)
                    (g/connect textures-node :_node-id self :nodes)
                    (g/connect textures-node :node-outline self :child-outlines)
                    (for [texture-desc (:textures scene)]
                      (let [resource (workspace/resolve-resource resource (:texture texture-desc))
                            type (resource/resource-type resource)
                            outputs (g/output-labels (:node-type type))]
                        ;; Messy because we need to deal with legacy standalone image files
                        (if (outputs :anim-data)
                          ;; TODO: have no tests for this
                          (g/make-nodes graph-id [texture [textures/TextureNode :name (:name texture-desc) :texture resource]]
                                        (textures/attach-texture self textures-node texture))
                          (g/make-nodes graph-id [img-texture [textures/ImageTextureNode]
                                                  texture [textures/TextureNode :name (:name texture-desc)]]
                                        (g/connect img-texture :_node-id texture :image-texture)
                                        (g/connect img-texture :packed-image texture :image)
                                        (g/connect img-texture :anim-data texture :anim-data)
                                        (project/connect-resource-node project resource img-texture [[:content :image]])
                                        (project/connect-resource-node project resource texture [[:resource :texture-resource]
                                                                                                 [:build-targets :dep-build-targets]])
                                        (textures/attach-texture self textures-node texture))))))
      (g/make-nodes graph-id [layers-node layers/LayersNode]
                    (g/connect layers-node :_node-id self :layers-node)
                    (g/connect layers-node :_node-id self :nodes)
                    (g/connect layers-node :node-outline self :child-outlines)
                    (loop [layer-descs (:layers scene)
                           tx-data []
                           index 0]
                      (if-let [layer-desc (first layer-descs)]
                        (let [tx-data (conj tx-data (g/make-nodes graph-id [layer [layers/LayerNode
                                                                                :name (:name layer-desc)
                                                                                :index index]]
                                                               (layers/attach-layer self layers-node layer)))]
                          (recur (rest layer-descs) tx-data (inc index)))
                        tx-data)))
      (g/make-nodes graph-id [layouts-node layouts/LayoutsNode]
                   (g/connect layouts-node :_node-id self :layouts-node)
                   (g/connect layouts-node :_node-id self :nodes)
                   (g/connect layouts-node :node-outline self :child-outlines)
                   (for [layout-desc (:layouts scene)]
                     (g/make-nodes graph-id [layout [layouts/LayoutNode
                                                     :name (:name layout-desc)
                                                     :nodes (:nodes layout-desc)]]
                       (layouts/attach-layout self layouts-node layout))))
      (g/make-nodes graph-id [nodes-node nodes/NodesNode]
        (g/connect nodes-node :_node-id self :nodes-node)
        (g/connect nodes-node :_node-id self :nodes)
        (g/connect nodes-node :node-outline self :child-outlines)
        (g/connect nodes-node :scene self :child-scenes)
        (loop [node-descs node-descs
               id->node {}
               all-tx-data []
               index 0]
          (if-let [node-desc (first node-descs)]
            (let [node-type (case (:type node-desc)
                              :type-box shapes/BoxNode
                              :type-pie shapes/PieNode
                              :type-text text/TextNode
                              :type-template templates/TemplateNode
                              nodes/GuiNode)
                  props (-> node-desc
                          (assoc :index index)
                          (select-keys (filter (complement #{:template :texture :font :layer})
                                               (keys (g/public-properties node-type)))))
                  tx-data (g/make-nodes graph-id [gui-node [node-type props]]
                                        (if (= :type-template (:type node-desc))
                                          (g/set-property gui-node :template {:resource (workspace/resolve-resource resource (:template node-desc))
                                                                              :overrides (get template-data (:id node-desc) {})})
                                          [])
                                        (let [parent (if (empty? (:parent node-desc)) nodes-node (id->node (:parent node-desc)))]
                                          (gutil/attach-gui-node self parent gui-node (:type node-desc)))
                                        ; Needs to be done after attaching so textures etc can be fetched
                                        (g/set-property gui-node :layer (:layer node-desc))
                                        (case (:type node-desc)
                                          (:type-box :type-pie) (g/set-property gui-node :texture (:texture node-desc))
                                          :type-text (g/set-property gui-node :font (:font node-desc))
                                          []))
                 node-id (first (map tx-node-id (filter tx-create-node? tx-data)))]
             (recur (rest node-descs) (assoc id->node (:id node-desc) node-id) (into all-tx-data tx-data) (inc index)))
            all-tx-data))))))


(defn register [workspace def]
  (let [ext (:ext def)
        exts (if (vector? ext) ext [ext])]
    (for [ext exts]
      (workspace/register-resource-type workspace
                                     :ext ext
                                     :label (:label def)
                                     :build-ext (:build-ext def)
                                     :node-type gsn/GuiSceneNode
                                     :load-fn load-gui-scene
                                     :icon (:icon def)
                                     :tags (:tags def)
                                     :template (:template def)
                                     :view-types [:scene :text]
                                     :view-opts {:scene {:grid true}}))))
