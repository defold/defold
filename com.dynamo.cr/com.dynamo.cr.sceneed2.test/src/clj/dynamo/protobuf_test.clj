(ns dynamo.protobuf-test
  (:require [schema.macros :as sm]
            [dynamo.file :as f]
            [dynamo.file.protobuf :refer :all]
            [dynamo.node :refer [defnode]]
            [dynamo.types :refer :all]
            [dynamo.outline :refer :all]
            [dynamo.texture :refer :all]
            [dynamo.image :refer :all]
            [clojure.test :refer :all])
  (:import [com.dynamo.atlas.proto AtlasProto AtlasProto$Atlas AtlasProto$AtlasAnimation AtlasProto$AtlasImage]
           [dynamo.types Animation Image TextureSet Rect EngineFormatTexture]))

(sm/defn produce-animation [this] nil)
(sm/defn produce-tree      [this] nil)
(sm/defn produce-image     [this] nil)

(defnode AtlasNode
  (inherits OutlineNode)

  (input images [ImageSource])
  (property extrude-borders (non-negative-integer))
  (property margin          (non-negative-integer)))

(defnode AtlasAnimationNode
  (inherits OutlineNode)

  (output tree      [OutlineItem] produce-tree)
  (output animation Animation     produce-animation))

(defnode AtlasImageNode
  (inherits OutlineNode)

  (output tree [OutlineItem] produce-tree))

(protocol-buffer-converters
  AtlasProto$Atlas
  {:constructor #'dynamo.protobuf-test/make-atlas-node
   :basic-properties [:extrude-borders :margin]
   :node-properties  {:images-list [:tree -> :children,
                                    :image -> :images]
                      :animations-list [:tree -> :children,
                                        :animation -> :animations]}}

  AtlasProto$AtlasAnimation
  {:constructor      #'dynamo.protobuf-test/make-atlas-animation-node
   :basic-properties [:id :playback :fps :flip-horizontal :flip-vertical]
   :node-properties  {:images-list [:tree -> :children,
                                    :image -> :images]}}

  AtlasProto$AtlasImage
  {:constructor #'dynamo.protobuf-test/make-atlas-image-node
   :basic-properties [:image]})

(defn atlas-with-one-animation [anim-id]
  (.build (doto (AtlasProto$Atlas/newBuilder)
            (.addAnimations (doto (AtlasProto$AtlasAnimation/newBuilder)
                              (.setId anim-id))))))

(defn- creations-of [tx nm]
  (count (filter #(= [:create-node nm] %) (map (juxt :type #(get-in % [:node :id])) (flatten tx)))))

(deftest node-connections-have-right-cardinality
  (testing "Children of the atlas node should be created exactly once."
           (let [message  (atlas-with-one-animation "the-animation")
                 atlas-tx (f/message->node message (constantly []))]
             (is (= 1 (creations-of atlas-tx "the-animation"))))))
