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
            [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.font :as font]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.vertex2 :as vtx]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [util.coll :as coll])
  (:import [com.jogamp.opengl DebugGL3 GL3 GLAutoDrawable]
           [java.awt.image BufferedImage]
           [java.nio Buffer]
           [java.util Arrays]
           [javax.vecmath Matrix4d]))

(set! *warn-on-reflection* true)

(defn- record-numeric-texture-transfers! [^GLAutoDrawable drawable transfers]
  (gl/with-drawable-as-current drawable
    (let [record-transfer! (fn [format type pixels width height]
                             ;; Ignore storage allocation without pixel data and ordinary color textures.
                             (when (and pixels
                                        (contains? #{[GL3/GL_RGBA GL3/GL_HALF_FLOAT]
                                                     [GL3/GL_RED_INTEGER GL3/GL_UNSIGNED_INT]}
                                                   [format type]))
                               (vswap! transfers conj [format type width height])))]
      (.setGL drawable
              (proxy [DebugGL3] [gl]
                (glTexImage2D [target level internal-format width height border format type pixels]
                  (.glTexImage2D gl (int target) (int level) (int internal-format) (int width) (int height)
                                 (int border) (int format) (int type) ^Buffer pixels)
                  (record-transfer! format type pixels width height))
                (glTexSubImage2D [target level x y width height format type pixels]
                  (.glTexSubImage2D gl (int target) (int level) (int x) (int y) (int width) (int height)
                                    (int format) (int type) ^Buffer pixels)
                  (record-transfer! format type pixels width height)))))))

(defn- image-points [^BufferedImage image pixel-predicate]
  (let [width (.getWidth image)
        height (.getHeight image)]
    (into []
          (keep-indexed (fn [index argb]
                          (when (pixel-predicate (bit-and 0xffffff argb))
                            [(rem index width) (- height 1 (quot index width))])))
          (.getRGB image 0 0 width height nil 0 width))))

;; Absolute pixel sizes in Label and GUI markup must not scale with the node's base font size.
(deftest vector-markup-pixel-size-is-independent-of-node-font-size
  (test-util/with-loaded-project
    (let [font-node (project/get-resource-node project "/fonts/vector_implicit_dynamic.font")
          label-node (project/get-resource-node project "/label/test.label")
          gui-node (project/get-resource-node project "/fonts/vector_preview.gui")
          text-node (:node-id (test-util/outline gui-node [0 0 0]))]
      (test-util/prop! label-node :font (workspace/find-resource workspace "/fonts/vector_implicit_dynamic.font"))
      (test-util/prop! font-node :characters "A")
      (doseq [runtime [false true]
              node [label-node text-node]]
        (test-util/prop! font-node :runtime runtime)
        (test-util/prop! node :text "<size=24px>AAAA</size>")
        (let [widths (mapv (fn [size]
                             (test-util/prop! node :font-size size)
                             (:width (g/valid-node-value node :text-layout)))
                           [16.0 32.0 64.0])]
          (is (pos? (first widths)))
          (is (apply = widths)))
        (test-util/prop! node :text "<size=2em>AAAA</size>")
        (test-util/prop! node :font-size 16.0)
        (let [width (:width (g/valid-node-value node :text-layout))]
          (test-util/prop! node :font-size 32.0)
          (is (= (* 2.0 width) (:width (g/valid-node-value node :text-layout)))))))))

;; Rendering fixed pixel markup must match plain text at that size without reuploading the atlases.
(deftest vector-pixel-size-rendering-reuses-textures
  (test-util/with-loaded-project
    (let [font-node (project/get-resource-node project "/fonts/vector_implicit_dynamic.font")
          label-node (project/get-resource-node project "/label/test.label")]
      (g/transact
        {:undoable false}
        [(g/set-property font-node :characters "A")
         (g/set-property label-node :font (workspace/find-resource workspace "/fonts/vector_implicit_dynamic.font"))
         (g/set-property label-node :material (workspace/find-resource workspace "/builtins/fonts/label-vector.material"))
         (g/set-property label-node :font-size 24.0)
         (g/set-property label-node :text "AAAA")
         (g/set-property label-node :color [1.0 0.0 1.0 1.0])])
      (let [[_ view] (test-util/open-scene-view! project app-view "/fonts/vector_preview.go" 512 256)
            transfers (volatile! [])]
        (try
          (record-numeric-texture-transfers! (g/node-value view :drawable) transfers)
          (let [expected (image-points (g/valid-node-value view :frame) #(= 0xff00ff %))
                initial-transfers @transfers]
            (is (< 50 (count expected)))
            (test-util/prop! label-node :text "<size=24px>AAAA</size>")
            (doseq [size [16.0 32.0 64.0]]
              (test-util/prop! label-node :font-size size)
              (is (= expected (image-points (g/valid-node-value view :frame) #(= 0xff00ff %))))
              (is (= initial-transfers @transfers))))
          (finally
            ((:dispose-preview-fn (workspace/get-view-type workspace :scene)) view)
            (test-util/close-tab! project app-view "/fonts/vector_preview.go")))))))

;; Vector colors, effects, and picking must survive an empty-text round trip.
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
                (let [^BufferedImage before-image (g/valid-node-value view :frame)
                      width (.getWidth before-image)
                      height (.getHeight before-image)
                      ^ints before-pixels (.getRGB before-image 0 0 width height nil 0 width)]
                  (test-util/with-prop [label-node :text ""]
                    (is (coll/empty? (image-points (g/valid-node-value view :frame) #(= 0xff00ff %)))))
                  (let [^BufferedImage after-image (g/valid-node-value view :frame)]
                    (is (Arrays/equals before-pixels (.getRGB after-image 0 0 width height nil 0 width))))))
              (finally
                ((:dispose-preview-fn (workspace/get-view-type workspace :scene)) view)
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
          ((:dispose-preview-fn (workspace/get-view-type workspace :scene)) view)
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
            ((:dispose-preview-fn (workspace/get-view-type workspace :scene)) label-view)
            ((:dispose-preview-fn (workspace/get-view-type workspace :scene)) gui-view)
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
            ((:dispose-preview-fn (workspace/get-view-type workspace :scene)) view)
            (test-util/close-tab! project app-view path)))))))

;; A batch must upload each numeric atlas once, even when it contains multiple text entries.
(deftest vector-batch-uploads-numeric-textures-once
  (test-util/with-loaded-project
    (let [font-node (project/get-resource-node project "/fonts/vector_implicit_dynamic.font")
          _ (g/set-property! font-node :runtime true)
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
              drawable (g/node-value view :drawable)
              transfers (volatile! [])]
          (record-numeric-texture-transfers! drawable transfers)
          (gl/with-drawable-as-current drawable
            (is (= (* 6 (count entries)) (count (font/request-vertex-buffer gl ::batch font-data entries))))
            (is (= {[GL3/GL_RGBA GL3/GL_HALF_FLOAT] 1
                    [GL3/GL_RED_INTEGER GL3/GL_UNSIGNED_INT] 1}
                   (frequencies (mapv #(subvec % 0 2) @transfers))))
            (let [initial-transfers @transfers]
              (is (= (* 6 (count entries)) (count (font/request-vertex-buffer gl ::batch font-data entries))))
              (is (= initial-transfers @transfers)))))
        (finally
          ((:dispose-preview-fn (workspace/get-view-type workspace :scene)) view)
          (test-util/close-tab! project app-view "/fonts/vector_preview.go"))))))

;; Camera changes must reuse vector vertices and their version, with and without bitmap effects.
(deftest vector-zoom-reuses-vertices
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
         (g/set-property label-node :text "O")])
      (doseq [effects [false true]]
        (testing (str "effects=" effects)
          (g/transact
            {:undoable false}
            [(g/set-property font-node :outline-alpha (if effects 1.0 0.0))
             (g/set-property font-node :shadow-alpha (if effects 1.0 0.0))])
          (let [font-data (g/valid-node-value font-node :font-data)
                entry (assoc (g/valid-node-value label-node :text-data)
                        :world-transform (doto (Matrix4d.) (.setIdentity)))
                [_ view] (test-util/open-scene-view! project app-view "/fonts/vector_preview.go" 512 256)
                camera-id (scene/view->camera view)
                initial-camera (g/node-value camera-id :local-camera)]
            (try
              (gl/with-drawable-as-current (g/node-value view :drawable)
                (let [initial-render-args (get (g/valid-node-value view :pass->render-args) pass/transparent)
                      initial-buffer (font/request-vertex-buffer gl ::zoom font-data [entry] initial-render-args)
                      initial-version (vtx/version initial-buffer)]
                  (is (pos? (count initial-buffer)))
                  (is (instance? Matrix4d (:view-proj initial-render-args)))
                  (doseq [zoom [2.0 4.0 0.5 1.0]]
                    (g/set-property! camera-id :local-camera
                      (-> initial-camera
                          (update :fov-x / zoom)
                          (update :fov-y / zoom)))
                    (let [render-args (get (g/valid-node-value view :pass->render-args) pass/transparent)
                          buffer (font/request-vertex-buffer gl ::zoom font-data [entry] render-args)]
                      (is (= (= 1.0 zoom) (= (:view-proj initial-render-args) (:view-proj render-args))))
                      (is (identical? initial-buffer buffer))
                      (is (= initial-version (vtx/version buffer)))))))
              (finally
                ((:dispose-preview-fn (workspace/get-view-type workspace :scene)) view)
                (test-util/close-tab! project app-view "/fonts/vector_preview.go")))))))))

;; Rendering zoomed Font, Label, and GUI previews must not upload unchanged numeric atlases again.
(deftest vector-preview-zoom-reuses-textures
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
                transfers (volatile! [])]
            (try
              (record-numeric-texture-transfers! (g/node-value view :drawable) transfers)
              (is (instance? BufferedImage (g/valid-node-value view :frame)))
              (is (= {[GL3/GL_RGBA GL3/GL_HALF_FLOAT] 1
                      [GL3/GL_RED_INTEGER GL3/GL_UNSIGNED_INT] 1}
                     (frequencies (mapv #(subvec % 0 2) @transfers))))
              (let [initial-transfers @transfers]
                (doseq [zoom [2.0 4.0 0.5 1.0]]
                  (g/set-property! camera-id :local-camera
                    (-> initial-camera
                        (update :fov-x / zoom)
                        (update :fov-y / zoom)))
                  (is (instance? BufferedImage (g/valid-node-value view :frame))))
                (is (= initial-transfers @transfers)))
              (finally
                ((:dispose-preview-fn (workspace/get-view-type workspace :scene)) view)
                (test-util/close-tab! project app-view path)))))))))

;; Copied shaders and unsaved fragment edits must render through the selected material in every font view.
(deftest custom-vector-material-preview
  (test-util/with-scratch-project "test/resources/test_project"
    (let [font-node (project/get-resource-node project "/fonts/vector_implicit_dynamic.font")
          label-node (project/get-resource-node project "/label/test.label")
          builtin-material (project/get-resource-node project "/builtins/fonts/font-vector.material")
          material-desc (assoc (g/valid-node-value builtin-material :save-value)
                          :vertex-program "/fonts/custom-vector.vp"
                          :fragment-program "/fonts/custom-vector.fp")]
      (doseq [ext ["vp" "fp"]]
        (test-util/write-file-resource! workspace (str "/fonts/custom-vector." ext)
          (string/split-lines (slurp (workspace/find-resource workspace (str "/builtins/fonts/font-vector." ext))))))
      (test-util/write-file-resource! workspace "/fonts/custom-vector.material" material-desc)
      (workspace/resource-sync! workspace)
      (let [custom-material (workspace/find-resource workspace "/fonts/custom-vector.material")
            fragment-node (project/get-resource-node project "/fonts/custom-vector.fp")
            original-lines (test-util/code-editor-lines fragment-node)
            green-lines (mapv #(string/replace % "var_color.rgb * var_color.a" "vec3(0.0, 1.0, 0.0) * var_color.a") original-lines)]
        (g/transact
          [(g/set-property font-node :characters "O")
           (g/set-property font-node :material custom-material)
           (g/set-property label-node :font (workspace/find-resource workspace "/fonts/vector_implicit_dynamic.font"))
           (g/set-property label-node :material custom-material)
           (g/set-property label-node :font-size 36.0)
           (g/set-property label-node :text "O")
           (g/set-property label-node :color [1.0 0.0 1.0 1.0])])
        (doseq [path ["/fonts/vector_implicit_dynamic.font" "/fonts/vector_preview.go" "/fonts/vector_preview.gui"]]
          (let [[_ view] (test-util/open-scene-view! project app-view path 512 256)]
            (try
              (let [before (count (image-points (g/valid-node-value view :frame) #(= 0x00ff00 %)))]
                (test-util/set-code-editor-lines! fragment-node green-lines)
                (is (< (+ before 10) (count (image-points (g/valid-node-value view :frame) #(= 0x00ff00 %)))) path)
                (test-util/set-code-editor-lines! fragment-node original-lines)
                (is (= before (count (image-points (g/valid-node-value view :frame) #(= 0x00ff00 %)))) path))
              (finally
                ((:dispose-preview-fn (workspace/get-view-type workspace :scene)) view)
                (test-util/close-tab! project app-view path)))))))))
