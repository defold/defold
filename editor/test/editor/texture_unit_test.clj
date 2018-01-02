(ns editor.texture-unit-test
  (:require [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.graph-util :as gu]
            [editor.resource :as resource]
            [editor.texture-unit :as texture-unit]
            [integration.test-util :as test-util]))

(g/defnode SinkNode
  (input dep-build-targets g/Any :array)
  (input texture-resources resource/Resource :array)
  (input texture-build-targets g/Any :array)
  (property textures resource/ResourceVec
            (value (gu/passthrough texture-resources))
            (set (fn [_evaluation-context self old-value new-value]
                   (concat
                     (texture-unit/connect-resources self old-value new-value [[:texture-build-target :dep-build-targets]])
                     (texture-unit/connect-resources-preserving-index self old-value new-value [[:resource :texture-resources]
                                                                                                [:texture-build-target :texture-build-targets]]))))))

(deftest connect-resources-test
  (test-util/with-loaded-project
    (let [project-graph (project/graph project)
          make-restore-point! (partial test-util/make-graph-reverter project-graph)
          atlas-resource (test-util/resource workspace "/graphics/atlas.atlas")
          image-resource (test-util/resource workspace "/graphics/ball.png")
          atlas (test-util/resource-node project "/graphics/atlas.atlas")
          image (test-util/resource-node project "/graphics/ball.png")
          atlas-texture-build-target (g/node-value atlas :texture-build-target)
          image-texture-build-target (g/node-value image :texture-build-target)
          sink (g/make-node! project-graph SinkNode)]

      (is (empty? (g/node-value sink :texture-resources)))
      (is (string/ends-with? (resource/proj-path (:resource atlas-texture-build-target)) ".texturec"))
      (is (string/ends-with? (resource/proj-path (:resource image-texture-build-target)) ".texturec"))

      (testing "Single texture"
        (with-open [_ (make-restore-point!)]
          (g/transact
            (texture-unit/set-array-index sink :textures 0 atlas-resource))
          (is (= [atlas-resource] (g/node-value sink :texture-resources)))
          (is (= [atlas-texture-build-target] (g/node-value sink :texture-build-targets)))
          (is (= [atlas-texture-build-target] (g/node-value sink :dep-build-targets)))))

      (testing "Textures at specific slots"
        (with-open [_ (make-restore-point!)]

          (testing "Assign"
            (g/transact
              (concat
                (texture-unit/set-array-index sink :textures 1 atlas-resource)
                (texture-unit/set-array-index sink :textures 3 image-resource)))
            (is (= [nil atlas-resource nil image-resource] (g/node-value sink :texture-resources)))
            (is (= [nil atlas-texture-build-target nil image-texture-build-target] (g/node-value sink :texture-build-targets)))
            (is (= [atlas-texture-build-target image-texture-build-target] (g/node-value sink :dep-build-targets))))

          (testing "Clear"
            (g/transact
              (concat
                (texture-unit/set-array-index sink :textures 1 nil)
                (texture-unit/set-array-index sink :textures 3 nil)))
            (is (= [] (g/node-value sink :texture-resources)))
            (is (= [] (g/node-value sink :texture-build-targets)))
            (is (= [] (g/node-value sink :dep-build-targets)))))))))
