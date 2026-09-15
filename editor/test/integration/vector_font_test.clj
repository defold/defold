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

(ns integration.vector-font-test
  (:require [clojure.set :as set]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.font :as font]
            [editor.gl :as gl]
            [editor.gl.texture :as texture]
            [editor.scene :as scene]
            [editor.scene-selection :as scene-selection]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [util.coll :as coll])
  (:import [java.awt.image BufferedImage]
           [javax.vecmath Matrix4d]))

(defn- image-points [^BufferedImage image pixel-predicate]
  (let [width (.getWidth image)
        height (.getHeight image)]
    (into []
          (keep-indexed (fn [index argb]
                          (when (pixel-predicate (bit-and 0xffffff argb))
                            [(rem index width) (- height 1 (quot index width))])))
          (.getRGB image 0 0 width height nil 0 width))))

(defn- pick [view point]
  (scene/produce-selection
    (assoc (into {} (map (fn [label] [label (g/node-value view label)]))
                 [:scene-render-data :renderables-aabb+picking-node-id :picking-drawable
                  :camera :viewport :pass->render-args])
           :picking-rect (scene-selection/calc-picking-rect point point))))

(deftest vector-label-preview-and-picking
  (test-util/with-loaded-project
    (let [font-node (project/get-resource-node project "/fonts/vector_implicit_dynamic.font")
          label-node (project/get-resource-node project "/label/test.label")]
      (g/transact
        {:undoable false}
        [(g/set-property font-node :characters "O")
         (g/set-property font-node :size 36)
         (g/set-property font-node :outline-width 2.0)
         (g/set-property font-node :shadow-blur 2.0)
         (g/set-property font-node :shadow-x 2.0)
         (g/set-property font-node :shadow-y -2.0)
         (g/set-property label-node :font (workspace/find-resource workspace "/fonts/vector_implicit_dynamic.font"))
         (g/set-property label-node :material (workspace/find-resource workspace "/builtins/fonts/font-vector.material"))
         (g/set-property label-node :font-size 36.0)
         (g/set-property label-node :text "O")
         (g/set-property label-node :color [1.0 0.0 1.0 1.0])
         (g/set-property label-node :outline [0.0 1.0 1.0 1.0])
         (g/set-property label-node :shadow [1.0 0.0 0.0 1.0])])
      (doseq [runtime [false true]
              effects [false true]]
        (testing (str "runtime=" runtime ", effects=" effects)
          (g/transact
            {:undoable false}
            [(g/set-property font-node :runtime runtime)
             (g/set-property font-node :outline-alpha (if effects 1.0 0.0))
             (g/set-property font-node :shadow-alpha (if effects 1.0 0.0))])
          (let [[go-node view] (test-util/open-scene-view! project app-view "/fonts/vector_preview.go" 512 256)
                component-node (:node-id (test-util/outline go-node [0]))]
            (try
              (let [image (g/valid-node-value view :frame)
                    face (image-points image #(= 0xff00ff %))
                    outline (image-points image #(= 0x00ffff %))
                    shadow (image-points image (fn [rgb]
                                                 (and (< 128 (bit-and 255 (bit-shift-right rgb 16)))
                                                      (> 64 (bit-and 255 (bit-shift-right rgb 8)))
                                                      (> 64 (bit-and 255 rgb)))))]
                (is (< 100 (count face)))
                (is (= effects (< 10 (count outline))))
                (is (= effects (< 10 (count shadow))))
                (when-not (coll/empty? face)
                  (is (= [component-node] (pick view (nth face (quot (count face) 2))))))
                (is (coll/empty? (pick view [5 5]))))
              (finally
                (#'scene/dispose-preview view)
                (test-util/close-tab! project app-view "/fonts/vector_preview.go")))))))))

(deftest vector-gui-preview-clipping-and-picking
  (test-util/with-loaded-project
    (let [font-node (project/get-resource-node project "/fonts/vector_implicit_dynamic.font")
          gui-node (project/get-resource-node project "/fonts/vector_preview.gui")
          clip-node (:node-id (test-util/outline gui-node [0 0]))
          text-node (:node-id (test-util/outline gui-node [0 0 0]))
          [_ view] (test-util/open-scene-view! project app-view "/fonts/vector_preview.gui" 512 256)]
      (try
        (is (= "clip" (g/node-value clip-node :id)))
        (is (= "text" (g/node-value text-node :id)))
        (g/set-property! font-node :characters "O")
        (let [clipped-face (set (image-points (g/valid-node-value view :frame) #(= 0xff00ff %)))]
          (is (< 10 (count clipped-face)))
          (g/set-property! clip-node :clipping-mode :clipping-mode-none)
          (let [whole-face (set (image-points (g/valid-node-value view :frame) #(= 0xff00ff %)))
                hidden-face (vec (set/difference whole-face clipped-face))]
            (is (< 10 (count hidden-face)))
            (when-not (coll/empty? hidden-face)
              (let [hidden-point (nth hidden-face (quot (count hidden-face) 2))]
                (is (= [text-node] (pick view hidden-point)))
                (g/set-property! clip-node :clipping-mode :clipping-mode-stencil)
                (is (not (contains? (set (pick view hidden-point)) text-node)))))
            (when-not (coll/empty? clipped-face)
              (is (= [text-node] (pick view (first (sort clipped-face))))))))
        (finally
          (#'scene/dispose-preview view)
          (test-util/close-tab! project app-view "/fonts/vector_preview.gui"))))))

(deftest font-mode-switch-in-open-views
  (test-util/with-loaded-project
    (let [font-node (project/get-resource-node project "/fonts/vector_implicit_dynamic.font")
          label-node (project/get-resource-node project "/label/test.label")]
      (g/transact
        {:undoable false}
        [(g/set-property font-node :runtime true)
         (g/set-property font-node :size 36)
         (g/set-property font-node :characters "ExampleO")
         (g/set-property font-node :vector-font-mode :vector-font-mode-sdf)
         (g/set-property font-node :material (workspace/find-resource workspace "/builtins/fonts/font-df.material"))
         (g/set-property label-node :font (workspace/find-resource workspace "/fonts/vector_implicit_dynamic.font"))
         (g/set-property label-node :material (workspace/find-resource workspace "/builtins/fonts/label-df.material"))
         (g/set-property label-node :font-size 36.0)
         (g/set-property label-node :text "Example")
         (g/set-property label-node :color [1.0 0.0 1.0 1.0])])
      (let [[go-node label-view] (test-util/open-scene-view! project app-view "/fonts/vector_preview.go" 512 256)
            [gui-node gui-view] (test-util/open-scene-view! project app-view "/fonts/vector_preview.gui" 1024 512)
            expected-selections [(:node-id (test-util/outline go-node [0]))
                                 (:node-id (test-util/outline gui-node [0 0 0]))]
            previous-points (volatile! {})]
        (try
          (doseq [mode [:sdf :vector :sdf :vector]]
            (testing (name mode)
              (g/transact
                {:undoable false}
                [(g/set-property font-node :vector-font-mode (if (= :sdf mode) :vector-font-mode-sdf :vector-font-mode-vector))
                 (g/set-property font-node :material (workspace/find-resource workspace (if (= :sdf mode) "/builtins/fonts/font-df.material" "/builtins/fonts/font-vector.material")))
                 (g/set-property label-node :material (workspace/find-resource workspace (if (= :sdf mode) "/builtins/fonts/label-df.material" "/builtins/fonts/label-vector.material")))])
              (doseq [[view selection] (mapv vector [label-view gui-view] expected-selections)]
                (let [points (image-points (g/valid-node-value view :frame) #(= 0xff00ff %))]
                  (is (< 10 (count points)))
                  (when-not (coll/empty? points)
                    (is (= [selection] (pick view (nth points (quot (count points) 2))))))
                  (if-let [previous (get @previous-points [mode view])]
                    (is (= previous points))
                    (vswap! previous-points assoc [mode view] points))))))
          (finally
            (#'scene/dispose-preview label-view)
            (#'scene/dispose-preview gui-view)
            (test-util/close-tab! project app-view "/fonts/vector_preview.go")
            (test-util/close-tab! project app-view "/fonts/vector_preview.gui")))))))

(deftest vector-batch-uploads-numeric-textures-once
  (test-util/with-loaded-project
    (let [font-node (project/get-resource-node project "/fonts/vector_implicit_dynamic.font")
          font-data (g/valid-node-value font-node :font-data)
          label-node (project/get-resource-node project "/label/test.label")
          [_ view] (test-util/open-scene-view! project app-view "/fonts/vector_preview.go" 512 256)]
      (try
        (g/set-property! label-node :font (workspace/find-resource workspace "/fonts/vector_implicit_dynamic.font"))
        (let [text-data (g/valid-node-value label-node :text-data)
              transform (doto (Matrix4d.) (.setIdentity))
              entries (mapv (fn [c]
                              (assoc text-data
                                :world-transform transform
                                :text-layout (font/layout-text (:font-map font-data) (str c) false 1000.0 0.0 1.0 36.0)))
                            "ABCDEFGHIJKLMNOPQRSTUVWXYZ")
              upload-count (volatile! 0)
              update-image! texture/update-image!]
          (with-redefs [texture/update-image! (fn [& args]
                                               (vswap! upload-count inc)
                                               (apply update-image! args))]
            (gl/with-drawable-as-current (g/node-value view :drawable)
              (is (= (* 6 (count entries)) (count (font/request-vertex-buffer gl ::batch font-data entries))))
              (is (= 2 @upload-count))
              (is (= (* 6 (count entries)) (count (font/request-vertex-buffer gl ::batch font-data entries))))
              (is (= 2 @upload-count)))))
        (finally
          (#'scene/dispose-preview view)
          (test-util/close-tab! project app-view "/fonts/vector_preview.go"))))))
