(ns editor.pipeline.tex-gen-test
  (:require [clojure.test :refer :all]
            [clojure.java.io :as io]
            [editor.pipeline.tex-gen :as tex-gen])
  (:import [javax.imageio ImageIO]
           [com.dynamo.graphics.proto Graphics$TextureImage]))

(def test-profile {:name "test-profile"
                   :platforms [{:os :os-id-generic
                                :formats [{:format :texture-format-rgba
                                           :compression-level :fast}]
                                :mipmaps false}]})

(deftest gen-bytes
  (let [img (ImageIO/read (io/resource "test_project/graphics/ball.png"))
        b (tex-gen/->bytes img test-profile)
        tex-img (Graphics$TextureImage/parseFrom b)
        alt (.getAlternatives tex-img 0)]
    (is (= 64 (.getWidth alt)))
    (is (= 32 (.getHeight alt)))))
