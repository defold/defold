(ns integration.label-test
  (:require [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.defold-project :as project]
            [editor.gl.pass :as pass]
            [editor.label :as label]
            [editor.math :as math]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]))

(deftest label-validation
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

(deftest label-aabb
  (test-util/with-loaded-project
    (let [node-id (project/get-resource-node project "/label/test.label")]
      (let [aabb (g/node-value node-id :aabb)
            [x y z] (mapv - (math/vecmath->clj (:max aabb)) (math/vecmath->clj (:min aabb)))]
        (is (< 0.0 x))
        (is (< 0.0 y))
        (is (= 0.0 z))))))

(deftest label-scene
  (test-util/with-loaded-project
    (let [node-id (project/get-resource-node project "/label/test.label")]
      (let [scene (g/node-value node-id :scene)
            aabb (g/node-value node-id :aabb)]
        (is (= aabb (:aabb scene)))
        (is (= node-id (:node-id scene)))
        (is (= node-id (some-> scene :renderable :select-batch-key)))
        (is (= :blend-mode-alpha (some-> scene :renderable :batch-key :blend-mode)))
        (is (= "Label" (some-> scene :renderable :user-data :text-data :text-layout :lines first)))
        (is (string/includes? (some-> scene :renderable :user-data :material-shader :verts) "gl_Position"))
        (is (string/includes? (some-> scene :renderable :user-data :material-shader :frags) "gl_FragColor"))))))

(defn- get-render-calls-by-pass
  [scene camera selection key-fn]
  (let [render-data (scene/produce-render-data scene selection [] camera)
        renderables (:renderables render-data)]
    (into {}
          (keep (fn [pass]
                  (let [calls (test-util/with-logged-calls [label/render-lines label/render-tris]
                                (scene/batch-render nil {:pass pass} (get renderables pass) false key-fn))]
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

(deftest label-batch-render
  (test-util/with-loaded-project
    (let [make-restore-point! #(test-util/make-graph-reverter (project/graph project))
          add-label-component! (partial test-util/add-embedded-component! app-view (fn [node-ids] (app-view/select app-view node-ids)) (workspace/get-resource-type workspace "label"))
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
          (is (= {pass/outline {label/render-lines 2}
                  pass/transparent {label/render-tris 2}}
                 (render-call-counts #{} :batch-key)))))

      (testing "Font differs"
        (with-open [_ (make-restore-point!)]
          (test-util/prop! (add-label-component! go) :font (workspace/find-resource workspace "/fonts/active_menu_item.font"))
          (test-util/prop! (add-label-component! go) :font (workspace/find-resource workspace "/fonts/active_menu_item.font"))
          (test-util/prop! (add-label-component! go) :font (workspace/find-resource workspace "/fonts/big_score.font"))
          (test-util/prop! (add-label-component! go) :font (workspace/find-resource workspace "/fonts/big_score.font"))
          (is (= {pass/outline {label/render-lines 2}
                  pass/transparent {label/render-tris 2}}
                 (render-call-counts #{} :batch-key)))))

      (testing "Material differs"
        (with-open [_ (make-restore-point!)]
          (test-util/prop! (add-label-component! go) :material (workspace/find-resource workspace "/fonts/active_menu_item.material"))
          (test-util/prop! (add-label-component! go) :material (workspace/find-resource workspace "/fonts/active_menu_item.material"))
          (test-util/prop! (add-label-component! go) :material (workspace/find-resource workspace "/fonts/big_score_font.material"))
          (test-util/prop! (add-label-component! go) :material (workspace/find-resource workspace "/fonts/big_score_font.material"))
          (is (= {pass/outline {label/render-lines 2}
                  pass/transparent {label/render-tris 2}}
                 (render-call-counts #{} :batch-key))))))))

(deftest label-scene
  (test-util/with-loaded-project
    (let [node-id (project/get-resource-node project "/label/test.label")]
      (test-util/test-uses-assigned-material workspace project node-id
                                             :material
                                             [:renderable :user-data :material-shader]
                                             [:renderable :user-data :gpu-texture]))))
