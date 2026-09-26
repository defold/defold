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
            [editor.gl.vertex2 :as vtx]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [util.coll :as coll]
            [util.fn :as fn])
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
                  (let [[x y] (nth face (quot (count face) 2))]
                    (test-util/mouse-click! view x y)
                    (is (test-util/selected? app-view component-node))))
                (test-util/mouse-click! view 5 5)
                (is (test-util/selected? app-view go-node))
                (test-util/with-prop [label-node :text ""]
                  (is (coll/empty? (image-points (g/valid-node-value view :frame) #(= 0xff00ff %)))))
                (is (= face (image-points (g/valid-node-value view :frame) #(= 0xff00ff %)))))
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
              (let [[x y] (nth hidden-face (quot (count hidden-face) 2))]
                (test-util/mouse-click! view x y)
                (is (test-util/selected? app-view text-node))
                (g/set-property! clip-node :clipping-mode :clipping-mode-stencil)
                (test-util/mouse-click! view 5 5)
                (test-util/mouse-click! view x y)
                (is (not (test-util/selected? app-view text-node)))))
            (when-not (coll/empty? clipped-face)
              (let [[x y] (first (sort clipped-face))]
                (test-util/mouse-click! view x y)
                (is (test-util/selected? app-view text-node))))))
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
                    (let [[x y] (nth points (quot (count points) 2))]
                      (test-util/mouse-click! view x y)
                      (is (test-util/selected? app-view selection))))
                  (if-let [previous (get @previous-points [mode view])]
                    (is (= previous points))
                    (vswap! previous-points assoc [mode view] points))))))
          (finally
            (#'scene/dispose-preview label-view)
            (#'scene/dispose-preview gui-view)
            (test-util/close-tab! project app-view "/fonts/vector_preview.go")
            (test-util/close-tab! project app-view "/fonts/vector_preview.gui")))))))

(deftest font-mode-switch-in-font-view
  (test-util/with-loaded-project
    (let [path "/fonts/vector_implicit_dynamic.font"
          font-node (project/get-resource-node project path)]
      (g/transact
        {:undoable false}
        [(g/set-property font-node :characters "Example")
         (g/set-property font-node :size 36)
         (g/set-property font-node :runtime true)
         (g/set-property font-node :vector-font-mode :vector-font-mode-sdf)])
      (let [[_ view] (test-util/open-scene-view! project app-view path 512 256)
            previous-points (volatile! {})]
        (try
          (doseq [mode [:sdf :vector :sdf :vector]]
            (testing (name mode)
              (g/transact
                {:undoable false}
                [(g/set-property font-node :vector-font-mode (if (= :sdf mode) :vector-font-mode-sdf :vector-font-mode-vector))
                 (g/set-property font-node :material (workspace/find-resource workspace (if (= :sdf mode) "/builtins/fonts/font-df.material" "/builtins/fonts/font-vector.material")))])
              (let [points (image-points (g/valid-node-value view :frame) #(= 0xf8f8fb %))]
                (is (< 100 (count points)))
                (if-let [previous (get @previous-points mode)]
                  (is (= previous points))
                  (vswap! previous-points assoc mode points)))))
          (finally
            (#'scene/dispose-preview view)
            (test-util/close-tab! project app-view path)))))))

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
                            "AB")
              update-image! (fn/make-call-logger texture/update-image!)]
          (with-redefs [texture/update-image! update-image!]
            (gl/with-drawable-as-current (g/node-value view :drawable)
              (is (= (* 6 (count entries)) (count (font/request-vertex-buffer gl ::batch font-data entries))))
              (is (= 2 (count (fn/call-logger-calls update-image!))))
              (is (= (* 6 (count entries)) (count (font/request-vertex-buffer gl ::batch font-data entries))))
              (is (= 2 (count (fn/call-logger-calls update-image!)))))))
        (finally
          (#'scene/dispose-preview view)
          (test-util/close-tab! project app-view "/fonts/vector_preview.go"))))))

(deftest vector-preview-zoom-reuses-vertices
  (test-util/with-loaded-project
    (let [font-path "/fonts/vector_implicit_dynamic.font"
          font-node (project/get-resource-node project font-path)
          label-node (project/get-resource-node project "/label/test.label")]
      (g/transact
        {:undoable false}
        [(g/set-property font-node :characters "O")
         (g/set-property font-node :size 36)
         (g/set-property font-node :outline-width 2.0)
         (g/set-property font-node :shadow-blur 2.0)
         (g/set-property label-node :font (workspace/find-resource workspace font-path))
         (g/set-property label-node :material (workspace/find-resource workspace "/builtins/fonts/label-vector.material"))
         (g/set-property label-node :text "O")])
      (doseq [effects [false true]
              path [font-path "/fonts/vector_preview.go" "/fonts/vector_preview.gui"]]
        (testing (str path ", effects=" effects)
          (g/transact
            {:undoable false}
            [(g/set-property font-node :outline-alpha (if effects 1.0 0.0))
             (g/set-property font-node :shadow-alpha (if effects 1.0 0.0))])
          (let [[_ view] (test-util/open-scene-view! project app-view path 512 256)
                camera-id (scene/view->camera view)
                initial-camera (g/node-value camera-id :local-camera)
                buffers (volatile! [])
                request-vertex-buffer font/request-vertex-buffer
                update-image! (fn/make-call-logger texture/update-image!)]
            (try
              (with-redefs [font/request-vertex-buffer (fn [& args]
                                                       (let [buffer (apply request-vertex-buffer args)]
                                                         (vswap! buffers conj [buffer (vtx/version buffer)])
                                                         buffer))
                            texture/update-image! update-image!]
                (g/valid-node-value view :frame)
                (let [[[initial-buffer initial-version]] @buffers]
                  (is (pos? (count initial-buffer)))
                  (is (= 2 (count (fn/call-logger-calls update-image!))))
                  (vreset! buffers [])
                  (doseq [zoom [2.0 4.0 0.5 1.0]]
                    (g/set-property! camera-id :local-camera
                                     (-> initial-camera
                                         (update :fov-x / zoom)
                                         (update :fov-y / zoom)))
                    (g/valid-node-value view :frame))
                  (is (= 4 (count @buffers)))
                  (doseq [[buffer version] @buffers]
                    (is (identical? initial-buffer buffer))
                    (is (= initial-version version)))
                  (is (= 2 (count (fn/call-logger-calls update-image!))))))
              (finally
                (#'scene/dispose-preview view)
                (test-util/close-tab! project app-view path)))))))))
