(ns integration.atlas-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.collection :as collection]
            [editor.factory :as factory]
            [editor.handler :as handler]
            [editor.defold-project :as project]
            [editor.types :as types]
            [editor.properties :as properties]
            [integration.test-util :as test-util]))

(deftest valid-fps
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/graphics/atlas.atlas")
          anim (:node-id (test-util/outline node-id [2]))]
      (is (nil? (test-util/prop-error anim :fps)))
      (test-util/prop! anim :fps -1)
      (is (g/error? (test-util/prop-error anim :fps))))))

(deftest img-not-found
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/graphics/img_not_found.atlas")
          img (:node-id (test-util/outline node-id [0]))]
      (is (g/error? (g/node-value img :animation))))))
