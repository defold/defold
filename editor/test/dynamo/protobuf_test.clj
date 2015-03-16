(ns dynamo.protobuf-test
  (:require [clojure.test :refer :all]
            [dynamo.file.protobuf :refer :all]
            [dynamo.graph :as g]
            [dynamo.image :refer :all]
            [dynamo.system :as ds]
            [dynamo.system.test-support :refer [with-clean-system]]
            [dynamo.texture :refer :all]
            [dynamo.types :as t :refer :all])
  (:import [com.dynamo.cr.sceneed2 TestAtlasProto TestAtlasProto$Atlas TestAtlasProto$AtlasAnimation TestAtlasProto$AtlasImage]
           [dynamo.types Animation Image]))

(g/defnode AtlasNode
  (property extrude-borders t/Int)
  (property margin          t/Int)

  (input images     [t/Str])
  (input animations [t/Str]))

(g/defnode AtlasAnimationNode
  (property id t/Str)
  (input  images    [t/Str])
  (output animation t/Str (g/fnk [id] (str "Animation " id))))

(g/defnode AtlasImageNode
  (property image t/Str))

(protocol-buffer-converters
 TestAtlasProto$Atlas
 {:node-type        AtlasNode
  :basic-properties [:extrude-borders :margin]
  :node-properties  {:images-list [:image -> :images]
                     :animations-list [:animation -> :animations]}}

 TestAtlasProto$AtlasAnimation
 {:node-type        AtlasAnimationNode
  :basic-properties [:id :playback :fps :flip-horizontal :flip-vertical]
  :node-properties  {:images-list [:image -> :images]}}

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
    (with-clean-system
      (let [message    (atlas-with-one-animation "the-animation")
            atlas-node (g/transactional (message->node message))
            anim-node  (ds/node-feeding-into atlas-node :animations)]
        (is (not (nil? atlas-node)))
        (is (= 7 (:margin atlas-node)))
        (is (= "the-animation" (:id anim-node)))))))
