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

(defn- make-aabb [min max]
  (reduce geom/aabb-incorporate (geom/null-aabb) (map #(Point3d. (double-array (conj % 0))) [min max])))

(deftest gen-scene
  (testing "Scene generation"
           (let [cases {"/logic/atlas_sprite.collection"
                        (fn [node-id]
                          (let [go (ffirst (g/sources-of node-id :child-scenes))]
                            (is (= (:aabb (g/node-value node-id :scene)) (make-aabb [-101 -97] [101 97])))
                            (g/transact (g/set-property go :position [10 0 0]))
                            (is (= (:aabb (g/node-value node-id :scene)) (make-aabb [-91 -97] [111 97])))))
                        "/logic/atlas_sprite.go"
                        (fn [node-id]
                          (let [component (ffirst (g/sources-of node-id :child-scenes))]
                            (is (= (:aabb (g/node-value node-id :scene)) (make-aabb [-101 -97] [101 97])))
                            (g/transact (g/set-property component :position [10 0 0]))
                            (is (= (:aabb (g/node-value node-id :scene)) (make-aabb [-91 -97] [111 97])))))
                        "/sprite/atlas.sprite"
                        (fn [node-id]
                          (let [scene (g/node-value node-id :scene)
                                aabb (make-aabb [-101 -97] [101 97])]
                            (is (= (:aabb scene) aabb))))
                        "/car/env/env.cubemap"
                        (fn [node-id]
                          (let [scene (g/node-value node-id :scene)]
                            (is (= (:aabb scene) geom/unit-bounding-box))))
                        "/switcher/switcher.atlas"
                        (fn [node-id]
                          (let [scene (g/node-value node-id :scene)
                                aabb (make-aabb [0 0] [2048 1024])]
                            (is (= (:aabb scene) aabb))))
                        }]
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
                   renderables   (g/node-value view :renderables)]
               (is (reduce #(and %1 %2) (map #(contains? renderables %) [pass/transparent pass/selection])))))))

(deftest scene-selection
  (testing "Scene selection"
           (test-util/with-loaded-project
             (let [path          "/logic/atlas_sprite.collection"
                   [resource-node view] (test-util/open-scene-view! project app-view path 128 128)
                   go-node       (ffirst (g/sources-of resource-node :child-scenes))]
               (is (test-util/selected? app-view resource-node))
               ; Press
               (test-util/mouse-press! view 32 32)
               (is (test-util/selected? app-view go-node))
               ; Click
               (test-util/mouse-release! view 32 32)
               (is (test-util/selected? app-view go-node))
               ; Drag
               (test-util/mouse-drag! view 32 32 32 36)
               (is (test-util/selected? app-view go-node))
               ; Deselect - default to "root" node
               (test-util/mouse-press! view 0 0)
               (is (test-util/selected? app-view resource-node))
               ; Toggling
               (let [modifiers (if system/mac? [:meta] [:control])]
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
               ; Drag entire screen
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
               ; Initial selection
               (test-util/mouse-click! view 64 64)
               (is (test-util/selected? app-view go-node))
               ; Move tool
               (test-util/set-active-tool! app-view :move)
               (is (= 0.0 (.x (pos go-node))))
               (test-util/mouse-drag! view 64 64 68 64)
               (is (not= 0.0 (.x (pos go-node))))
               (g/undo! project-graph)
               ; Rotate tool
               (test-util/set-active-tool! app-view :rotate)
               (is (= 0.0 (.x (rot go-node))))
               ;; begin drag at y = 80 to hit y axis (for x rotation)
               (test-util/mouse-drag! view 64 80 64 84)
               (is (not= 0.0 (.x (rot go-node))))
               (g/undo! project-graph)
               ; Scale tool
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
               ; Click
               (test-util/mouse-click! view 32 32)
               (is (test-util/selected? app-view go-node))
               ; Delete
               (g/transact (g/delete-node go-node))
               (is (test-util/empty-selection? app-view))
               ; Undo
               (g/undo! project-graph)
               (is (test-util/selected? app-view go-node))
               ; Select again
               (test-util/mouse-click! view 32 32)
               (is (test-util/selected? app-view go-node))
               ; Delete again
               (g/transact (g/delete-node go-node))
               (is (test-util/empty-selection? app-view))
               ;Select again
               (test-util/mouse-click! view 32 32)
               (is (test-util/selected? app-view resource-node))))))

(deftest transform-tools-empty-go
  (testing "Transform tools and manipulator interactions"
           (test-util/with-loaded-project
             (let [path          "/collection/empty_go.collection"
                   [resource-node view] (test-util/open-scene-view! project app-view path 128 128)
                   go-node       (ffirst (g/sources-of resource-node :child-scenes))]
               (is (test-util/selected? app-view resource-node))
               ; Initial selection (empty go's are not selectable in the view)
               (app-view/select! app-view [go-node])
               (is (test-util/selected? app-view go-node))
               ; Move tool
               (test-util/set-active-tool! app-view :move)
               (is (= 0.0 (.x (pos go-node))))
               (test-util/mouse-drag! view 64 64 68 64)
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
               :children [{:node-id :tree-node-id
                           :children [{:node-id :apple-node-id}]}
                          {:node-id :house-node-id
                           :children [{:node-id :door-node-id
                                       :children [{:node-id :door-handle-node-id}]}]}]}]
    (is (= {:node-id :new-node-id
            :children [{:node-id :tree-node-id
                        :picking-id :new-node-id
                        :children [{:node-id :apple-node-id
                                    :picking-id :new-node-id}]}
                       {:node-id :house-node-id
                        :picking-id :new-node-id
                        :children [{:node-id :door-node-id
                                    :picking-id :new-node-id
                                    :children [{:node-id :door-handle-node-id
                                                :picking-id :new-node-id}]}]}]}
           (scene/claim-scene scene :new-node-id)))))

(defn- render-pass? [pass]
  (satisfies? types/Pass pass))

(defn- output-renderable? [renderable]
  (is (map? renderable))
  (is (= #{:aabb
           :batch-key
           :node-id
           :node-path
           :picking-id
           :parent-world-transform
           :render-fn
           :render-key
           :selected
           :user-data
           :world-transform} (set (keys renderable))))
  (is (instance? AABB (:aabb renderable)))
  (is (some? (:node-id renderable)))
  (is (vector? (:node-path renderable)))
  (is (every? some? (:node-path renderable)))
  (is (or (nil? (:picking-id renderable)) (= (type (:node-id renderable)) (type (:picking-id renderable)))))
  (is (instance? Matrix4d (:parent-world-transform renderable)))
  (is (some? (:render-fn renderable)))
  (is (instance? Comparable (:render-key renderable)))
  (is (or (true? (:selected renderable)) (false? (:selected renderable))))
  (is (instance? Matrix4d (:world-transform renderable))))

(defn- output-renderable-vector? [coll]
  (and (vector? coll)
       (every? output-renderable? coll)))

(deftest produce-render-data-test
  (let [passes [pass/transparent pass/selection pass/outline]
        camera (camera/make-camera)
        scene {:node-id :scene-node-id
               :renderable {:render-fn :scene-render-fn
                            :passes passes}
               :children [{:node-id :tree-node-id
                           :renderable {:render-fn :tree-render-fn
                                        :passes passes}
                           :children [{:node-id :apple-node-id
                                       :renderable {:render-fn :apple-render-fn
                                                    :passes passes}}]}
                          {:node-id :house-node-id
                           :renderable {:render-fn :house-render-fn
                                       :passes passes}
                           :children [{:node-id :door-node-id
                                       :renderable {:render-fn :door-render-fn
                                                    :passes passes}
                                       :children [{:node-id :door-handle-node-id
                                                   :renderable {:render-fn :door-handle-render-fn
                                                                :passes passes}}]}]}
                          {:node-id :well-node-id
                           :renderable {:render-fn :well-render-fn
                                        :passes passes}
                           :children [{:node-id :rope-node-id
                                       :picking-id :well-node-id
                                       :renderable {:render-fn :rope-render-fn
                                                    :passes passes}
                                       :children [{:node-id :bucket-node-id
                                                   :picking-id :well-node-id
                                                   :renderable {:render-fn :bucket-render-fn
                                                                :passes passes}}]}]}]}]
    (testing "Output is well-formed"
      (let [render-data (scene/produce-render-data scene [] [] camera)]
        (is (= [:renderables :selected-renderables] (keys render-data)))
        (is (every? render-pass? (keys (:renderables render-data))))
        (is (every? output-renderable-vector? (vals (:renderables render-data))))
        (is (output-renderable-vector? (:selected-renderables render-data)))))

    (testing "Aux renderables are included unaltered"
      (let [background-renderable {:batch-key [false 0 0] :render-fn :background-render-fn}
            aux-renderables [{pass/background [background-renderable]}]
            render-data (scene/produce-render-data scene [] aux-renderables camera)
            background-renderables (-> render-data :renderables (get pass/background))]
        (is (some? (some #(= background-renderable %) background-renderables)))))

    (testing "Node paths are relative to scene"
      (let [render-data (scene/produce-render-data scene [] [] camera)
            selection-renderables (-> render-data :renderables (get pass/selection))]
        (are [render-fn node-path]
          (= [node-path] (into []
                               (comp (filter (fn [renderable]
                                               (= render-fn (:render-fn renderable))))
                                     (map :node-path))
                               selection-renderables))
          :scene-render-fn       []
          :tree-render-fn        [:tree-node-id]
          :apple-render-fn       [:tree-node-id :apple-node-id]
          :house-render-fn       [:house-node-id]
          :door-render-fn        [:house-node-id :door-node-id]
          :door-handle-render-fn [:house-node-id :door-node-id :door-handle-node-id])))

    (testing "Picking ids are assigned correctly"
      (let [render-data (scene/produce-render-data scene [] [] camera)
            selection-renderables (-> render-data :renderables (get pass/selection))
            picking-ids-by-node-id (into {}
                                         (map (juxt :node-id :picking-id))
                                         selection-renderables)]
        (are [node-id picking-id]
          (= picking-id (get picking-ids-by-node-id node-id ::missing))

          :scene-node-id       nil
          :tree-node-id        :tree-node-id
          :apple-node-id       :apple-node-id
          :house-node-id       :house-node-id
          :door-node-id        :door-node-id
          :door-handle-node-id :door-handle-node-id
          :well-node-id        :well-node-id
          :rope-node-id        :well-node-id
          :bucket-node-id      :well-node-id)))

    (testing "Selection"
      (are [selection appears-selected]
        (let [render-data (scene/produce-render-data scene selection [] camera)
              outline-renderables (-> render-data :renderables (get pass/outline))
              selected-renderables (:selected-renderables render-data)]
          (is (= selection (mapv :node-id selected-renderables)))
          (is (= appears-selected (mapv :node-id (filter :selected outline-renderables)))))

        []
        []

        [:apple-node-id]
        [:apple-node-id]

        [:tree-node-id]
        [:tree-node-id :apple-node-id]

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
        (let [selected-renderables (:selected-renderables (scene/produce-render-data scene selection [] camera))]
          (is (= selection (mapv :node-id selected-renderables))))
        [:house-node-id :door-node-id :door-handle-node-id]
        [:door-handle-node-id :house-node-id :door-node-id]
        [:door-node-id :door-handle-node-id :house-node-id]
        [:door-handle-node-id :door-node-id :house-node-id]
        [:house-node-id :door-handle-node-id :door-node-id]
        [:door-node-id :house-node-id :door-handle-node-id]))))
