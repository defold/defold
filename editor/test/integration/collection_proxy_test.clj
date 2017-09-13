(ns integration.collection-proxy-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.collection :as collection]
            [editor.handler :as handler]
            [editor.defold-project :as project]
            [editor.workspace :as workspace]
            [editor.types :as types]
            [editor.properties :as properties]
            [editor.resource :as resource]
            [integration.test-util :as test-util]))

(deftest new-collection-proxy
  (testing "A new collection proxy"
    (test-util/with-loaded-project
      (let [node-id   (test-util/resource-node project "/collection_proxy/new.collectionproxy")
            outline   (g/node-value node-id :node-outline)
            form-data (g/node-value node-id :form-data)]
        (is (= nil (g/node-value node-id :collection)))
        (is (= "Collection Proxy" (:label outline)))
        (is (empty? (:children outline)))
        (is (= nil (get-in form-data [:values [:collection]])))))))

(deftest collection-proxy-with-collection
  (testing "A collection proxy with a collection set"
    (test-util/with-loaded-project
      (let [node-id   (test-util/resource-node project "/collection_proxy/with_collection.collectionproxy")
            outline   (g/node-value node-id :node-outline)
            form-data (g/node-value node-id :form-data)]
        (is (= "/collection_proxy/default.collection" (resource/resource->proj-path (g/node-value node-id :collection))))
        (is (= "Collection Proxy" (:label outline)))
        (is (empty? (:children outline)))
        (is (= "/collection_proxy/default.collection"
               (resource/resource->proj-path (get-in form-data [:values [:collection]]))))))))

(deftest validation
  (test-util/with-loaded-project
    (let [node-id   (test-util/resource-node project "/collection_proxy/with_collection.collectionproxy")]
      (are [prop v test] (test-util/with-prop [node-id prop v]
                           (is (test (test-util/prop-error node-id prop))))
        :collection nil                                                                   g/error-info?
        :collection (test-util/resource workspace "not_exists.collection")                g/error-fatal?
        :collection (test-util/resource workspace "/collection_proxy/default.collection") nil?))))
