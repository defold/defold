(ns editor.gui
  (:require [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.gui.fonts :as fonts]
            [editor.gui.gui-scene-node :as gsn]
            [editor.gui.layers :as layers]
            [editor.gui.layouts :as layouts]
            [editor.gui.loader :as loader]
            [editor.gui.nodes :as nodes]
            [editor.gui.shapes :as shapes]
            [editor.gui.templates :as templates]
            [editor.gui.text :as text]
            [editor.gui.textures :as textures]
            [editor.gui.util :as gutil]
            [editor.handler :as handler]
            [editor.outline :as outline]
            [editor.protobuf :as protobuf]
            [editor.gui.icons :as icons])
  (:import [com.dynamo.gui.proto Gui$NodeDesc$Type]))

(set! *warn-on-reflection* true)

(defn add-gui-node! [project scene parent node-type]
  (let [index (inc (reduce max 0 (g/node-value parent :child-indices)))
        id (outline/resolve-id (subs (name node-type) 5) (keys (g/node-value scene :node-ids)))
        def-node-type (case node-type
                        :type-box shapes/BoxNode
                        :type-pie shapes/PieNode
                        :type-text text/TextNode
                        :type-template templates/TemplateNode
                        nodes/GuiNode)]
    (g/transact
      (concat
        (g/operation-label "Add Gui Node")
        (g/make-nodes (g/node-id->graph-id scene) [gui-node [def-node-type :id id :index index :type node-type :size [200.0 100.0 0.0]]]
          (gutil/attach-gui-node scene parent gui-node node-type)
          (if (= node-type :type-text)
            (g/set-property gui-node :text "<text>" :font "")
            [])
          (project/select project [gui-node]))))))

(defn- add-gui-node-handler [project {:keys [scene parent node-type]}]
  (add-gui-node! project scene parent node-type))

(defn- make-add-handler [scene parent label icon handler-fn user-data]
  {:label label :icon icon :command :add
   :user-data (merge {:handler-fn handler-fn :scene scene :parent parent} user-data)})

(defn- add-handler-options [node]
  (let [types (protobuf/enum-values Gui$NodeDesc$Type)
        scene (nodes/node->gui-scene node)
        node-options (if (some #(g/node-instance? % node) [gsn/GuiSceneNode nodes/GuiNode nodes/NodesNode])
                       (let [parent (if (= node scene)
                                      (g/node-value scene :nodes-node)
                                      node)]
                         (mapv (fn [[type info]]
                                 (make-add-handler scene parent (:display-name info) (get icons/node-icons type)
                                                   add-gui-node-handler {:node-type type}))
                           types))
                       [])
        texture-option (if (some #(g/node-instance? % node) [gsn/GuiSceneNode textures/TexturesNode])
                         (let [parent (if (= node scene)
                                        (g/node-value scene :textures-node)
                                        node)]
                           (make-add-handler scene parent "Texture" icons/texture-icon textures/add-texture-handler {})))
        font-option (if (some #(g/node-instance? % node) [gsn/GuiSceneNode fonts/FontsNode])
                      (let [parent (if (= node scene)
                                     (g/node-value scene :fonts-node)
                                     node)]
                        (make-add-handler scene parent "Font" icons/font-icon fonts/add-font-handler {})))
        layer-option (if (some #(g/node-instance? % node) [gsn/GuiSceneNode layers/LayersNode])
                       (let [parent (if (= node scene)
                                      (g/node-value scene :layers-node)
                                      node)]
                         (make-add-handler scene parent "Layer" icons/layer-icon layers/add-layer-handler {})))
        layout-option (if (some #(g/node-instance? % node) [gsn/GuiSceneNode layouts/LayoutsNode])
                        (let [parent (if (= node scene)
                                       (g/node-value scene :layouts-node)
                                       node)]
                          (make-add-handler scene parent "Layout" icons/layout-icon layouts/add-layout-handler {})))]
    (filter some? (conj node-options texture-option font-option layer-option layout-option))))

(handler/defhandler :add :global
  (active? [selection] (and (= 1 (count selection))
                         (not-empty (add-handler-options (first selection)))))
  (run [project user-data] (when user-data ((:handler-fn user-data) project user-data)))
  (options [selection user-data]
    (when (not user-data)
      (add-handler-options (first selection)))))

(defn register-resource-types [workspace]
  (loader/register workspace gutil/pb-def))

(defn- outline-parent [child]
  (first (filter some? (map (fn [[_ output target input]]
                              (when (= output :node-outline) [target input]))
                            (g/outputs child)))))

(defn- outline-move! [outline node-id offset]
  (let [outline (sort-by :index outline)
        new-order (sort-by second (map-indexed (fn [i entry]
                                                 (let [nid (:node-id entry)
                                                       new-index (+ (* 2 i) (if (= node-id nid) (* offset 3) 0))]
                                                   [nid new-index]))
                                               outline))
        packed-order (map-indexed (fn [i v] [(first v) i]) new-order)]
    (g/transact
      (for [[node-id index] packed-order]
        (g/set-property node-id :index index)))))

(defn- single-gui-node? [selection]
  (handler/single-selection? selection nodes/GuiNode))

(defn- single-layer-node? [selection]
  (handler/single-selection? selection layers/LayerNode))

(handler/defhandler :move-up :global
  (active? [selection] (or (single-gui-node? selection) (single-layer-node? selection)))
  (run [selection] (let [selected (first selection)
                         [target input] (outline-parent selected)]
                     (outline-move! (g/node-value target input) selected -1))))

(handler/defhandler :move-down :global
  (active? [selection] (or (single-gui-node? selection) (single-layer-node? selection)))
  (run [selection] (let [selected (first selection)
                         [target input] (outline-parent selected)]
                     (outline-move! (g/node-value target input) selected 1))))
