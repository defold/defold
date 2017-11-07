(ns integration.tex-packing-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [editor.math :as math]
            [editor.defold-project :as project]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util])
  (:import [com.dynamo.gameobject.proto GameObject GameObject$CollectionDesc GameObject$CollectionInstanceDesc GameObject$InstanceDesc
            GameObject$EmbeddedInstanceDesc GameObject$PrototypeDesc]
           [editor.types Region]
           [editor.workspace BuildResource]
           [java.awt.image BufferedImage]
           [java.io File]
           [javax.imageio ImageIO]
           [javax.vecmath Point3d Matrix4d]))

(deftest tex-packing
  (testing "Packing a texture set"
           (test-util/with-loaded-project
             (let [path          "/switcher/switcher.atlas"
                   resource-node (test-util/resource-node project path)]
               (let [anims (g/node-value resource-node :anim-data)]
                 (is (contains? anims "blue_candy")))))))
