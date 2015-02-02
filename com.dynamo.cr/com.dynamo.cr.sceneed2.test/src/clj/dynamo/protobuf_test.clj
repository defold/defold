(ns dynamo.protobuf-test
  (:require [clojure.test :refer :all]
            [schema.core :as s]
            [schema.macros :as sm]
            [dynamo.file :as f]
            [dynamo.file.protobuf :refer :all]
            [dynamo.node :as n]
            [dynamo.types :refer :all]
            [dynamo.texture :refer :all]
            [dynamo.image :refer :all]
            [dynamo.system :as ds]
            [dynamo.system.test-support :refer [with-clean-world]]
            [internal.transaction :as it])
  (:import [com.dynamo.cr.sceneed2 TestAtlasProto TestAtlasProto$Atlas TestAtlasProto$AtlasAnimation TestAtlasProto$AtlasImage]
           [dynamo.types Animation Image]))

(sm/defn produce-animation [this] nil)
(sm/defn produce-tree      [this] nil)
(sm/defn produce-image     [this] nil)

(n/defnode AtlasNode
  (inherits n/OutlineNode)

  (input images     [Image])
  (input animations [Animation])

  (property extrude-borders s/Int)
  (property margin          s/Int))

(n/defnode AtlasAnimationNode
  (inherits n/OutlineNode)

  (output tree      [OutlineItem] produce-tree)
  (output animation Animation     produce-animation))

(n/defnode AtlasImageNode
  (inherits n/OutlineNode)

  (output tree [OutlineItem] produce-tree))

(protocol-buffer-converters
 TestAtlasProto$Atlas
 {:node-type        AtlasNode
  :basic-properties [:extrude-borders :margin]
  :node-properties  {:images-list [:tree -> :children,
                                   :image -> :images]
                     :animations-list [:tree -> :children,
                                       :animation -> :animations]}}

 TestAtlasProto$AtlasAnimation
 {:node-type        AtlasAnimationNode
  :basic-properties [:id :playback :fps :flip-horizontal :flip-vertical]
  :node-properties  {:images-list [:tree -> :children,
                                   :image -> :images]}}

 TestAtlasProto$AtlasImage
 {:node-type        AtlasImageNode
  :basic-properties [:image]})

(defn atlas-with-one-animation [anim-id]
  (.build (doto (TestAtlasProto$Atlas/newBuilder)
            (.setMargin 7)
            (.addAnimations (doto (TestAtlasProto$AtlasAnimation/newBuilder)
                              (.setId anim-id))))))

(deftest node-connections-have-right-cardinality
  (testing "Children of the atlas node should be created exactly once."
    (with-clean-world
      (let [message    (atlas-with-one-animation "the-animation")
            atlas-node (ds/transactional (message->node message))
            anim-node  (ds/node-feeding-into atlas-node :animations)]
        (is (not (nil? atlas-node)))
        (is (= 7 (:margin atlas-node)))
        (is (= "the-animation" (:id anim-node)))))))
