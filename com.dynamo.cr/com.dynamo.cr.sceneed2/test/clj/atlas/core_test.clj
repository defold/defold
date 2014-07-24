(ns atlas.core-test
    (:require [dynamo.project.test-support :refer :all]
              [dynamo.project :as p]
              [dynamo.file :as f]
              [dynamo.outline :as outline]
              [dynamo.node :as node]
              [atlas.core :refer :all]
              [clojure.test.check :as tc]
              [clojure.test.check.generators :as gen]
              [clojure.test.check.properties :as prop]
              [clojure.test.check.clojure-test :refer [defspec]]
              [clojure.test :refer :all])
      (:import  [com.dynamo.atlas.proto AtlasProto AtlasProto$Atlas AtlasProto$AtlasAnimation AtlasProto$AtlasImage]
                [com.dynamo.textureset.proto TextureSetProto$TextureSet TextureSetProto$TextureSet$Builder
                                             TextureSetProto$TextureSetAnimation TextureSetProto$TextureSetAnimation$Builder]
                [com.google.protobuf ByteString TextFormat]))

(use-fixtures :each with-clean-project)

;; returns ^AtlasProto$Atlas
(defn get-atlas-message [path])

(defn ninja-atlas-message []
  (with-open [is (clojure.java.io/reader "/tmp/russ/ninjas.atlas")]
    (let [builder (AtlasProto$Atlas/newBuilder)]
      (TextFormat/merge is builder) ; modifies builder in place, returns nil
      (.build builder))))

(defn build-ninjas
  [tempid]
  (let [project *test-project*
        path (f/->ProjectPath (:eclipse-project @project) "/tmp/russ/ninjas" "atlas")
        atlas-tx (f/message->node (ninja-atlas-message) nil nil nil :filename (f/local-path path) :_id tempid)
        compiler (make-atlas-compiler :textureset-filename "/tmp/russ/ninjas.texturesetc"
                                      :texture-filename    "/tmp/russ/ninjas.texturesetc")]
    (p/transact project
      [atlas-tx
       (p/new-resource compiler)
       (p/connect {:_id tempid} :textureset compiler :textureset)])))

(comment
  ;; more real.
  (defn build-ninjas
    [tempid]
    (let [project *test-project*
          eclipse-project (:eclipse-project @*test-project*)
          path (f/->ProjectPath eclipse-project "/tmp/russ/ninjas" "atlas")
          atlas-tx (f/message->node (ninja-atlas-message) nil nil nil :filename (f/local-path path) :_id tempid)
          compiler (make-atlas-compiler :textureset-filename (f/in-build-directory (f/replace-extension path "texturesetc"))
                                        :texture-filename    (f/in-build-directory (f/replace-extension path "texturec")))]
      (p/transact project
        [atlas-tx
         (p/new-resource compiler)
         (p/connect {:_id -1} :textureset compiler :textureset)]))))

(deftest atlases-loaded-from-files
  (let [atlas-tempid  -99
        tx-result     (build-ninjas atlas-tempid)
        tx-graph   (:graph tx-result)
        tx-node    (node/get-node tx-graph (p/resolve-tempid tx-result atlas-tempid))
        prj-graph  (:graph @*test-project*)
        _ (def my-graph prj-graph) ;;terrible
        prj-node   (node/get-node prj-graph (p/resolve-tempid tx-result atlas-tempid))]
    (testing "appear in the transaction result"
      (is (not   (nil? tx-graph)))
      (is (not   (nil? tx-node)))
      (is (= atlas.core.AtlasNode (class tx-node))))
    (testing "appear in the project"
      (is (not   (nil? prj-graph)))
      (is (not   (nil? prj-node)))
      (is (= atlas.core.AtlasNode (class prj-node))))
    (testing "contain packable images"
      (let [imgs (node/get-value prj-graph prj-node :imagelist)]
        (is (= 9 (count imgs))
            )
        (println imgs)))
    ))

;; load an atlas into our project (or create an atlas!)
;; give it some images
;; add an animation
;; give the animation some images
;; perform source-images on the resulting graph and nodes
;; note that we have all the images.

#_(deftest creation
  (testing "an atlas node is created"
           (let [tx-result (transact *test-project* ())]))
  (testing "one node with tempid"
           (let [tx-result (transact *test-project* (new-resource {:_id -5 :indicator "known value"}))]
             (is (= :ok (:status tx-result)))
             (is (= "known value" (:indicator (dg/node (:graph tx-result) (resolve-tempid tx-result -5))))))))

#_(defn on-load
   [project path ^AtlasProto$Atlas atlas-message]
   (let [atlas-tx (message->node atlas-message nil nil nil :filename (local-path path) :_id -1)
         compiler (make-atlas-compiler :textureset-filename (in-build-directory (replace-extension path "texturesetc"))
                                       :texture-filename    (in-build-directory (replace-extension path "texturec")))]
     (transact project
       [atlas-tx
        (new-resource compiler)
        (connect {:_id -1} :textureset compiler :textureset)])))

(run-tests)