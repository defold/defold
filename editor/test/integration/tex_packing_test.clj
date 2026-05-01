;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

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
