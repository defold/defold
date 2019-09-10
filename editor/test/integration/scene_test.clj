(ns integration.scene-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.camera :as camera]
            [editor.geom :as geom]
            [editor.gl.pass :as pass]
            [editor.scene :as scene]
            [editor.system :as system]
            [editor.types :as types]
            [editor.math :as math]
            [integration.test-util :as test-util])
  (:import [editor.types AABB]
           [javax.vecmath Point3d Matrix4d Quat4d Vector3d]))

(defn- apply-scene-transforms-to-aabbs
  ([scene] (apply-scene-transforms-to-aabbs geom/Identity4d scene))
  ([parent-world-transform scene]
   (let [local-transform (or (:transform scene) geom/Identity4d)
         world-transform (doto (Matrix4d. parent-world-transform) (.mul local-transform))]
     (cond-> (update scene :aabb geom/aabb-transform world-transform)
             (seq (:children scene)) (update :children #(mapv (partial apply-scene-transforms-to-aabbs world-transform) %))))))

(defn- scene-union-aabb [scene]
  (reduce geom/aabb-union geom/null-aabb (map :aabb (tree-seq :children :children scene))))

(defn- node-union-aabb [node-id]
  (scene-union-aabb (apply-scene-transforms-to-aabbs (g/node-value node-id :scene))))

(deftest gen-scene
  (testing "Scene generation"
           (let [cases {"/logic/atlas_sprite.collection"
                        (fn [node-id]
                          (let [go (ffirst (g/sources-of node-id :child-scenes))]
                            (is (= (node-union-aabb node-id) (geom/coords->aabb [-101 -97 0] [101 97 0])))
                            (g/transact (g/set-property go :position [10 0 0]))
                            (is (= (node-union-aabb node-id) (geom/coords->aabb [-91 -97 0] [111 97 0])))))
                        "/logic/atlas_sprite.go"
                        (fn [node-id]
                          (let [component (ffirst (g/sources-of node-id :child-scenes))]
                            (is (= (node-union-aabb node-id) (geom/coords->aabb [-101 -97] [101 97])))
                            (g/transact (g/set-property component :position [10 0 0]))
                            (is (= (node-union-aabb node-id) (geom/coords->aabb [-91 -97] [111 97])))))
                        "/sprite/atlas.sprite"
                        (fn [node-id]
                          (is (= (node-union-aabb node-id) (geom/coords->aabb [-101 -97] [101 97]))))
                        "/car/env/env.cubemap"
                        (fn [node-id]
                          (is (= (node-union-aabb node-id) (geom/coords->aabb [-1 -1 -1] [1 1 1]))))
                        "/switcher/switcher.atlas"
                        (fn [node-id]
                          (is (= (node-union-aabb node-id) (geom/coords->aabb [0 0] [2048 1024]))))}]
             (test-util/with-loaded-project
               (doseq [[path test-fn] cases]
                 (let [[node view] (test-util/open-scene-view! project app-view path 128 128)]
                   (is (not (nil? node)) (format "Could not find '%s'" path))
                   (test-fn node)))))))

(deftest gen-renderables
  (testing "Renderables generation"
           (test-util/with-loaded-project
             (let [path          "/sprite/small_atlas.sprite"
                   [resource-node view] (test-util/open-scene-view! project app-view path 128 128)
                   renderables   (g/node-value view :all-renderables)]
               (is (reduce #(and %1 %2) (map #(contains? renderables %) [pass/transparent pass/selection])))))))

(deftest scene-selection
  (testing "Scene selection"
           (test-util/with-loaded-project
             (let [path          "/logic/atlas_sprite.collection"
                   [resource-node view] (test-util/open-scene-view! project app-view path 128 128)
                   go-node       (ffirst (g/sources-of resource-node :child-scenes))]
               (is (test-util/selected? app-view resource-node))
               ;; Press
               (test-util/mouse-press! view 32 32)
               (is (test-util/selected? app-view go-node))
               ;; Click
               (test-util/mouse-release! view 32 32)
               (is (test-util/selected? app-view go-node))
               ;; Drag
               (test-util/mouse-drag! view 32 32 32 36)
               (is (test-util/selected? app-view go-node))
               ;; Deselect - default to "root" node
               (test-util/mouse-press! view 0 0)
               (is (test-util/selected? app-view resource-node))
               ;; Toggling
               (let [modifiers (if system/mac? [:meta] [:shift])]
                 (test-util/mouse-click! view 32 32)
                 (is (test-util/selected? app-view go-node))
                 (test-util/mouse-click! view 32 32 modifiers)
                 (is (test-util/selected? app-view resource-node)))))))

(deftest scene-multi-selection
  (testing "Scene multi selection"
           (test-util/with-loaded-project
             (let [path          "/logic/two_atlas_sprites.collection"
                   [resource-node view] (test-util/open-scene-view! project app-view path 128 128)
                   go-nodes      (map first (g/sources-of resource-node :child-scenes))]
               (is (test-util/selected? app-view resource-node))
               ;; Drag entire screen
               (test-util/mouse-drag! view 0 0 128 128)
               (is (every? #(test-util/selected? app-view %) go-nodes))))))

(defn- pos [node]
  (doto (Vector3d.) (math/clj->vecmath (g/node-value node :position))))

(defn- rot [node]
  (doto (Quat4d.) (math/clj->vecmath (g/node-value node :rotation))))

(defn- scale [node]
  (doto (Vector3d.) (math/clj->vecmath (g/node-value node :scale))))

(deftest transform-tools
  (testing "Transform tools and manipulator interactions"
           (test-util/with-loaded-project
             (let [project-graph (g/node-id->graph-id project)
                   path          "/logic/atlas_sprite.collection"
                   [resource-node view] (test-util/open-scene-view! project app-view path 128 128)
                   go-node       (ffirst (g/sources-of resource-node :child-scenes))]
               (is (test-util/selected? app-view resource-node))
               ;; Initial selection
               (test-util/mouse-click! view 64 64)
               (is (test-util/selected? app-view go-node))
               ;; Move tool
               (test-util/set-active-tool! app-view :move)
               (is (= 0.0 (.x (pos go-node))))
               (test-util/mouse-drag! view 64 64 68 64)
               (is (not= 0.0 (.x (pos go-node))))
               (g/undo! project-graph)
               ;; Rotate tool
               (test-util/set-active-tool! app-view :rotate)
               (is (= 0.0 (.x (rot go-node))))
               ;; begin drag at y = 80 to hit y axis (for x rotation)
               (test-util/mouse-drag! view 64 80 64 84)
               (is (not= 0.0 (.x (rot go-node))))
               (g/undo! project-graph)
               ;; Scale tool
               (test-util/set-active-tool! app-view :scale)
               (is (= 1.0 (.x (scale go-node))))
               (test-util/mouse-drag! view 64 64 68 64)
               (is (not= 1.0 (.x (scale go-node))))))))

(deftest delete-undo-delete-selection
  (testing "Scene generation"
           (test-util/with-loaded-project
             (let [project-graph (g/node-id->graph-id project)
                   path          "/logic/atlas_sprite.collection"
                   [resource-node view] (test-util/open-scene-view! project app-view path 128 128)
                   go-node       (ffirst (g/sources-of resource-node :child-scenes))]
               (is (test-util/selected? app-view resource-node))
               ;; Click
               (test-util/mouse-click! view 32 32)
               (is (test-util/selected? app-view go-node))
               ;; Delete
               (g/transact (g/delete-node go-node))
               (is (test-util/empty-selection? app-view))
               ;; Undo
               (g/undo! project-graph)
               (is (test-util/selected? app-view go-node))
               ;; Select again
               (test-util/mouse-click! view 32 32)
               (is (test-util/selected? app-view go-node))
               ;; Delete again
               (g/transact (g/delete-node go-node))
               (is (test-util/empty-selection? app-view))
               ;; Select again
               (test-util/mouse-click! view 32 32)
               (is (test-util/selected? app-view resource-node))))))

(deftest transform-tools-empty-go
  (testing "Transform tools and manipulator interactions"
           (test-util/with-loaded-project
             (let [path          "/collection/empty_go.collection"
                   [resource-node view] (test-util/open-scene-view! project app-view path 128 128)
                   go-node       (ffirst (g/sources-of resource-node :child-scenes))]
               (is (test-util/selected? app-view resource-node))
               ;; Initial selection (empty go's are not selectable in the view)
               (app-view/select! app-view [go-node])
               (is (test-util/selected? app-view go-node))
               ;; Move tool
               (test-util/set-active-tool! app-view :move)
               (is (= 0.0 (.x (pos go-node))))
               (test-util/mouse-drag! view 64 64 100 64)
               (is (not= 0.0 (.x (pos go-node))))))))

(deftest select-component-part-in-collection
  (testing "Transform tools and manipulator interactions"
           (test-util/with-loaded-project
             (let [path "/collection/go_pfx.collection"
                   [resource-node view]          (test-util/open-scene-view! project app-view path 128 128)
                   emitter (:node-id (test-util/outline resource-node [0 0 0]))]
               (is (not (seq (g/node-value view :selected-renderables))))
               (app-view/select! app-view [emitter])
               (is (seq (g/node-value view :selected-renderables)))))))

(deftest claim-scene-test
  (let [scene {:node-id :scene-node-id
               :children [{:node-id :scene-node-id
                           :children [{:node-id :scene-node-id}]}
                          {:node-id :tree-node-id
                           :node-outline-key "tree-node-outline-key"
                           :children [{:node-id :apple-node-id
                                       :node-outline-key "apple-node-outline-key"}]}
                          {:node-id :house-node-id
                           :node-outline-key "house-node-outline-key"
                           :children [{:node-id :door-node-id
                                       :node-outline-key "door-node-outline-key"
                                       :children [{:node-id :door-handle-node-id
                                                   :node-outline-key "door-handle-node-outline-key"}]}]}]}]
    (is (= {:node-id :new-node-id
            :node-outline-key "new-node-outline-key"
            :children [{:node-id :new-node-id
                        :node-outline-key "new-node-outline-key"
                        :children [{:node-id :new-node-id
                                    :node-outline-key "new-node-outline-key"}]}
                       {:node-id :tree-node-id
                        :node-outline-key "tree-node-outline-key"
                        :picking-node-id :new-node-id
                        :children [{:node-id :apple-node-id
                                    :node-outline-key "apple-node-outline-key"
                                    :picking-node-id :new-node-id}]}
                       {:node-id :house-node-id
                        :node-outline-key "house-node-outline-key"
                        :picking-node-id :new-node-id
                        :children [{:node-id :door-node-id
                                    :node-outline-key "door-node-outline-key"
                                    :picking-node-id :new-node-id
                                    :children [{:node-id :door-handle-node-id
                                                :node-outline-key "door-handle-node-outline-key"
                                                :picking-node-id :new-node-id}]}]}]}
           (scene/claim-scene scene :new-node-id "new-node-outline-key")))))

(defn- render-pass? [pass]
  (satisfies? types/Pass pass))

(defn- output-renderable? [renderable]
  (is (map? renderable))
  (is (= #{:aabb
           :batch-key
           :node-id
           :node-id-path
           :node-outline-key
           :node-outline-key-path
           :picking-id
           :picking-node-id
           :parent-world-transform
           :render-fn
           :render-key
           :selected
           :tags
           :user-data
           :world-rotation
           :world-scale
           :world-transform
           :world-translation} (set (keys renderable))))
  (is (instance? AABB (:aabb renderable)))
  (is (some? (:node-id renderable)))
  (is (vector? (:node-id-path renderable)))
  (is (every? some? (:node-id-path renderable)))
  (is (vector? (:node-outline-key-path renderable)))
  (is keyword? (first (:node-outline-key-path renderable)))
  (is (every? string? (rest (:node-outline-key-path renderable))))
  (is (or (nil? (:picking-node-id renderable)) (= (type (:node-id renderable)) (type (:picking-node-id renderable)))))
  (is (instance? Matrix4d (:parent-world-transform renderable)))
  (is (some? (:render-fn renderable)))
  (is (instance? Comparable (:render-key renderable)))
  (is (or (true? (:selected renderable)) (false? (:selected renderable))))
  (is (instance? Matrix4d (:world-transform renderable))))

(defn- output-renderable-vector? [coll]
  (and (vector? coll)
       (every? output-renderable? coll)))

(defn- produce-render-data [scene selection aux-renderables camera]
  (let [scene-render-data (scene/produce-scene-render-data {:scene scene :selection selection :hidden-renderable-args [] :hidden-node-outline-key-paths [] :camera camera})
        aux-render-data (scene/produce-aux-render-data {:aux-renderables aux-renderables :internal-renderables {} :hidden-renderable-tags []})]
    (scene/merge-render-datas aux-render-data {} scene-render-data)))

(deftest produce-render-data-test
  (let [passes [pass/transparent pass/selection pass/outline]
        camera (camera/make-camera)
        scene {:node-id :scene-node-id
               :node-outline-key "scene-node-outline-key"
               :renderable {:render-fn :scene-render-fn
                            :passes passes}
               :children [{:node-id :scene-node-id
                           :node-outline-key "scene-node-outline-key"
                           :renderable {:render-fn :scene-render-fn-2
                                        :passes passes}}
                          {:node-id :tree-node-id
                           :node-outline-key "tree-node-outline-key"
                           :renderable {:render-fn :tree-render-fn
                                        :passes passes}
                           :children [{:node-id :apple-node-id
                                       :node-outline-key "apple-node-outline-key"
                                       :renderable {:render-fn :apple-render-fn
                                                    :passes passes}
                                       :children [{:node-id :apple-node-id
                                                   :node-outline-key "apple-node-outline-key"
                                                   :renderable {:render-fn :apple-render-fn-2
                                                                :passes passes}}]}]}
                          {:node-id :house-node-id
                           :node-outline-key "house-node-outline-key"
                           :renderable {:render-fn :house-render-fn
                                        :passes passes}
                           :children [{:node-id :door-node-id
                                       :node-outline-key "door-node-outline-key"
                                       :renderable {:render-fn :door-render-fn
                                                    :passes passes}
                                       :children [{:node-id :door-handle-node-id
                                                   :node-outline-key "door-handle-node-outline-key"
                                                   :renderable {:render-fn :door-handle-render-fn
                                                                :passes passes}}]}]}
                          {:node-id :well-node-id
                           :node-outline-key "well-node-outline-key"
                           :renderable {:render-fn :well-render-fn
                                        :passes passes}
                           :children [{:node-id :rope-node-id
                                       :node-outline-key "rope-node-outline-key"
                                       :picking-id :dont-care
                                       :picking-node-id :well-node-id
                                       :renderable {:render-fn :rope-render-fn
                                                    :passes passes}
                                       :children [{:node-id :bucket-node-id
                                                   :node-outline-key "bucket-node-outline-key"
                                                   :picking-id :dont-care
                                                   :picking-node-id :well-node-id
                                                   :renderable {:render-fn :bucket-render-fn
                                                                :passes passes}}]}]}]}]
    (testing "Output is well-formed"
      (let [render-data (produce-render-data scene [] [] camera)]
        (is (= [:renderables :selected-renderables] (keys render-data)))
        (is (every? render-pass? (keys (:renderables render-data))))
        (is (every? output-renderable-vector? (vals (:renderables render-data))))
        (is (output-renderable-vector? (:selected-renderables render-data)))))

    (testing "Aux renderables are included unaltered"
      (let [background-renderable {:batch-key [false 0 0] :render-fn :background-render-fn}
            aux-renderables [{pass/background [background-renderable]}]
            render-data (produce-render-data scene [] aux-renderables camera)
            background-renderables (-> render-data :renderables (get pass/background))]
        (is (some? (some #(= background-renderable %) background-renderables)))))

    (testing "Node paths are relative to scene"
      (let [render-data (produce-render-data scene [] [] camera)
            selection-renderables (-> render-data :renderables (get pass/selection))]
        (are [render-fn node-id-path]
          (= [node-id-path] (into []
                                  (comp (filter (fn [renderable]
                                                  (= render-fn (:render-fn renderable))))
                                        (map :node-id-path))
                                  selection-renderables))
          :scene-render-fn       []
          :scene-render-fn-2     []
          :tree-render-fn        [:tree-node-id]
          :apple-render-fn       [:tree-node-id :apple-node-id]
          :apple-render-fn-2     [:tree-node-id :apple-node-id]
          :house-render-fn       [:house-node-id]
          :door-render-fn        [:house-node-id :door-node-id]
          :door-handle-render-fn [:house-node-id :door-node-id :door-handle-node-id])))

    (testing "Picking node ids are assigned correctly"
      (let [render-data (produce-render-data scene [] [] camera)
            selection-renderables (-> render-data :renderables (get pass/selection))
            picking-node-ids-by-node-id (into {}
                                              (map (fn [[node-id renderables]]
                                                     [node-id (mapv :picking-node-id renderables)]))
                                              (group-by :node-id selection-renderables))]
        (are [node-id picking-node-ids]
          (= picking-node-ids (get picking-node-ids-by-node-id node-id ::missing))

          :scene-node-id       [nil nil]
          :tree-node-id        [:tree-node-id]
          :apple-node-id       [:apple-node-id :apple-node-id]
          :house-node-id       [:house-node-id]
          :door-node-id        [:door-node-id]
          :door-handle-node-id [:door-handle-node-id]
          :well-node-id        [:well-node-id]
          :rope-node-id        [:well-node-id]
          :bucket-node-id      [:well-node-id])))

    (testing "Node key paths are assigned correctly"
      (let [render-data (produce-render-data scene [] [] camera)
            selection-renderables (-> render-data :renderables (get pass/selection))
            node-outline-key-paths-by-node-id (into {}
                                                    (map (fn [[node-id renderables]]
                                                           [node-id (mapv :node-outline-key-path renderables)]))
                                                    (group-by :node-id selection-renderables))]
        (are [node-id node-outline-key-paths]
          (= node-outline-key-paths (get node-outline-key-paths-by-node-id node-id ::missing))

          :scene-node-id       [[:scene-node-id]
                                [:scene-node-id]]
          :tree-node-id        [[:scene-node-id "tree-node-outline-key"]]
          :apple-node-id       [[:scene-node-id "tree-node-outline-key" "apple-node-outline-key"]
                                [:scene-node-id "tree-node-outline-key" "apple-node-outline-key"]]
          :house-node-id       [[:scene-node-id "house-node-outline-key"]]
          :door-node-id        [[:scene-node-id "house-node-outline-key" "door-node-outline-key"]]
          :door-handle-node-id [[:scene-node-id "house-node-outline-key" "door-node-outline-key" "door-handle-node-outline-key"]]
          :well-node-id        [[:scene-node-id "well-node-outline-key"]]
          :rope-node-id        [[:scene-node-id "well-node-outline-key" "rope-node-outline-key"]]
          :bucket-node-id      [[:scene-node-id "well-node-outline-key" "rope-node-outline-key" "bucket-node-outline-key"]])))

    (testing "Selection"
      (are [selection appears-selected]
        (let [render-data (produce-render-data scene selection [] camera)
              outline-renderables (-> render-data :renderables (get pass/outline))
              selected-renderables (:selected-renderables render-data)]
          (is (= selection (mapv :node-id selected-renderables)))
          (is (= appears-selected (mapv :node-id (filter :selected outline-renderables)))))

        []
        []

        [:apple-node-id]
        [:apple-node-id :apple-node-id]

        [:tree-node-id]
        [:tree-node-id :apple-node-id :apple-node-id]

        [:door-node-id]
        [:door-node-id :door-handle-node-id]

        [:house-node-id]
        [:house-node-id :door-node-id :door-handle-node-id]

        [:house-node-id :door-handle-node-id]
        [:house-node-id :door-node-id :door-handle-node-id]

        [:bucket-node-id]
        [:bucket-node-id]

        [:rope-node-id]
        [:rope-node-id :bucket-node-id]

        [:well-node-id]
        [:well-node-id :rope-node-id :bucket-node-id]

        [:well-node-id :rope-node-id]
        [:well-node-id :rope-node-id :bucket-node-id]))

    (testing "Selected renderables are ordered"
      (are [selection]
        (let [selected-renderables (:selected-renderables (produce-render-data scene selection [] camera))]
          (is (= selection (mapv :node-id selected-renderables))))
        [:house-node-id :door-node-id :door-handle-node-id]
        [:door-handle-node-id :house-node-id :door-node-id]
        [:door-node-id :door-handle-node-id :house-node-id]
        [:door-handle-node-id :door-node-id :house-node-id]
        [:house-node-id :door-handle-node-id :door-node-id]
        [:door-node-id :house-node-id :door-handle-node-id]))))
