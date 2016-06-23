(ns integration.curve-view-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [editor.curve-view :as curve-view]
            [editor.defold-project :as project]
            [integration.test-util :as test-util]))

(deftest selection
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          curve-view (curve-view/make-view! project (test-util/make-view-graph!) nil {} false)
          node-id (test-util/resource-node project "/particlefx/fireworks_big.particlefx")]
      (project/select project [(:node-id (test-util/outline node-id [2]))])
      
      #_(prn curve-view node-id))))

(selection)