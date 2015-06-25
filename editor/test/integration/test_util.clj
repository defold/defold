(ns integration.test-util
  (:require [dynamo.graph :as g]
            [editor.atlas :as atlas]
            [editor.collection :as collection]
            [editor.cubemap :as cubemap]
            [editor.game-object :as game-object]
            [editor.game-project :as game-project]
            [editor.image :as image]
            [editor.platformer :as platformer]
            [editor.project :as project]
            [editor.scene :as scene]
            [editor.sprite :as sprite]
            [editor.switcher :as switcher]
            [editor.font :as font]
            [editor.protobuf-types :as protobuf-types]
            [editor.workspace :as workspace])
  (:import [java.io File]
           [javax.imageio ImageIO]))

(def project-path "resources/test_project")

(defn setup-workspace!
  ([graph]
    (setup-workspace! graph project-path))
  ([graph project-path]
    (let [workspace (workspace/make-workspace graph project-path)]
      (g/transact
        (concat
          (scene/register-view-types workspace)))
      (let [workspace (g/refresh workspace)]
        (g/transact
          (concat
            (collection/register-resource-types workspace)
            (font/register-resource-types workspace)
            (game-object/register-resource-types workspace)
            (game-project/register-resource-types workspace)
            (cubemap/register-resource-types workspace)
            (image/register-resource-types workspace)
            (atlas/register-resource-types workspace)
            (platformer/register-resource-types workspace)
            (protobuf-types/register-resource-types workspace)
            (switcher/register-resource-types workspace)
            (sprite/register-resource-types workspace))))
      (g/refresh workspace))))

(defn setup-project!
  [workspace]
  (let [proj-graph (g/make-graph! :history true :volatility 1)
        project (project/make-project proj-graph workspace)
        project (project/load-project project)]
    (g/reset-undo! proj-graph)
    project))

(defn resource-node [project path]
  (let [workspace (:workspace project)]
    (project/get-resource-node project (workspace/file-resource workspace path))))

(defn empty-selection? [project]
  (let [sel (g/node-value project :selected-node-ids)]
    (empty? sel)))

(defn selected? [project tgt-node]
  (let [sel (g/node-value project :selected-node-ids)]
    (not (nil? (some #{(g/node-id tgt-node)} sel)))))

(g/defnode DummyAppView
  (property active-tool g/Keyword))

(defn setup-app-view! []
  (let [view-graph (g/make-graph! :history false :volatility 2)
        app-view (first (g/tx-nodes-added (g/transact (g/make-node view-graph DummyAppView :active-tool :move))))]
    app-view))

(defn set-active-tool! [app-view tool]
  (g/transact (g/set-property app-view :active-tool tool)))

(defn open-scene-view! [project app-view resource-node width height]
  (let [view-graph (g/make-graph! :history false :volatility 2)
        view (scene/make-preview view-graph resource-node {:app-view app-view :project project} width height)]
    (g/refresh view)))

(defn- fake-input!
  ([view type x y]
    (fake-input! view type x y []))
  ([view type x y modifiers]
    (let [pos [x y 0.0]]
      (g/transact (g/set-property view :picking-rect (scene/calc-picking-rect pos pos))))
    (let [handlers (g/sources-of view :input-handlers)
          user-data (g/node-value view :selected-tool-renderables)
          action (reduce #(assoc %1 %2 true) {:type type :x x :y y} modifiers)
          action (scene/augment-action view action)]
      (doseq [[node label] handlers]
        (let [handler-fn (g/node-value node label)]
          (handler-fn node action user-data))))))

(defn mouse-press!
  ([view x y]
    (fake-input! view :mouse-pressed x y))
  ([view x y modifiers]
    (fake-input! view :mouse-pressed x y modifiers)))

(defn mouse-move! [view x y]
  (fake-input! view :mouse-moved x y))

(defn mouse-release! [view x y]
  (fake-input! view :mouse-released x y))

(defn mouse-click!
  ([view x y]
    (mouse-click! view x y []))
  ([view x y modifiers]
    (mouse-press! view x y modifiers)
    (mouse-release! view x y)))

(defn mouse-drag! [view x0 y0 x1 y1]
  (mouse-press! view x0 y0)
  (mouse-move! view x1 y1)
  (mouse-release! view x1 y1))

(defn dump-frame! [view path]
  (let [image (g/node-value view :frame)]
    (let [file (File. path)]
      (ImageIO/write image "png" file))))
