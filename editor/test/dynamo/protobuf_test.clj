(ns dynamo.protobuf-test
  (:require [clojure.test :refer :all]
            [dynamo.file :as f]
            [dynamo.file.protobuf :refer :all]
            [dynamo.graph :as g]
            [dynamo.image :refer :all]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.system.test-support :refer [with-clean-system]]
            [dynamo.texture :refer :all]
            [dynamo.types :refer :all]
            [internal.transaction :as it]
            [plumbing.core :refer [fnk]]
            [schema.core :as s]
            [schema.macros :as sm])
  (:import [com.dynamo.cr.sceneed2 TestAtlasProto TestAtlasProto$Atlas TestAtlasProto$AtlasAnimation TestAtlasProto$AtlasImage]
           [dynamo.types Animation Image]))

(n/defnode AtlasNode
  (property extrude-borders s/Int)
  (property margin          s/Int)

  (input images     [s/Str])
  (input animations [s/Str]))

(n/defnode AtlasAnimationNode
  (property id s/Str)
  (input  images    [s/Str])
  (output animation s/Str (fnk [id] (str "Animation " id))))

(n/defnode AtlasImageNode
  (property image s/Str))

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
