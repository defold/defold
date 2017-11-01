(ns integration.material-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [integration.test-util :as test-util]
            [editor.workspace :as workspace]
            [editor.defold-project :as project]))

(defn- prop [node-id label]
  (get-in (g/node-value node-id :_properties) [:properties label :value]))

(defn- prop! [node-id label val]
  (g/transact (g/set-property node-id label val)))

(deftest load-material-render-data
  (test-util/with-loaded-project
    (let [node-id   (test-util/resource-node project "/materials/test.material")
          samplers (g/node-value node-id :samplers)]
      (is (some? (g/node-value node-id :shader)))
      (is (= 1 (count samplers))))))

(deftest material-validation
  (test-util/with-loaded-project
    (let [node-id   (test-util/resource-node project "/materials/test.material")]
      (is (not (g/error? (g/node-value node-id :shader))))
      (is (not (g/error? (g/node-value node-id :build-targets))))
      (doseq [v [nil (workspace/resolve-workspace-resource workspace "/not_found.vp")]]
        (test-util/with-prop [node-id :vertex-program v]
          (is (g/error? (g/node-value node-id :shader)))
          (is (g/error? (g/node-value node-id :build-targets)))))
      (is (not (g/error? (g/node-value node-id :shader))))
      (is (not (g/error? (g/node-value node-id :build-targets))))
      (doseq [v [nil (workspace/resolve-workspace-resource workspace "/not_found.fp")]]
        (test-util/with-prop [node-id :fragment-program v]
          (is (g/error? (g/node-value node-id :shader)))
          (is (g/error? (g/node-value node-id :build-targets))))))))
