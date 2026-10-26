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

(ns integration.svg-test
  (:require [cljfx.api :as fx]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.atlas :as atlas]
            [editor.camera :as camera]
            [editor.defold-project :as project]
            [editor.gl.shader :as shader]
            [editor.gl.vertex2 :as vertex2]
            [editor.gui :as gui]
            [editor.image-util :as image-util]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.scene :as scene]
            [editor.sprite :as sprite]
            [editor.svg :as svg]
            [editor.texture-set :as texture-set]
            [editor.types :as types]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [util.coll :as coll])
  (:import [com.dynamo.bob.pipeline VectorAtlasCompiler]
           [com.dynamo.gamesys.proto AtlasProto$Atlas]
           [java.awt.image BufferedImage]
           [java.nio ByteBuffer ByteOrder]
           [javafx.scene.control Label]
           [javafx.scene.layout AnchorPane]
           [javax.vecmath Matrix4d]))

(set! *warn-on-reflection* true)

;; Drop filtering rejects incompatible images and infers an empty atlas's mode.
;; Saved vector atlases reopen with the same contents.
(deftest svg-drop-and-save
  (test-util/with-scratch-project "test/resources/svg_project"
    (let [atlas (project/get-resource-node project "/vector.atlas")
          raster (project/get-resource-node project "/raster.atlas")
          svg (workspace/find-resource workspace "/shape.svg")
          png (workspace/find-resource workspace "/raster.png")
          vector-before (g/valid-node-value atlas :save-value)
          raster-before (g/valid-node-value raster :save-value)]
      (g/transact (#'atlas/handle-drop atlas [] workspace nil [png]))
      (g/transact (#'atlas/handle-drop raster [] workspace nil [svg]))
      (is (= vector-before (g/valid-node-value atlas :save-value)))
      (is (= raster-before (g/valid-node-value raster :save-value)))
      (doseq [[path images mode] [["/empty-vector.atlas" [svg png] :vector]
                                  ["/empty-raster.atlas" [png svg] :raster]]]
        (let [empty-resource (test-util/make-resource! workspace path {})]
          (workspace/resource-sync! workspace)
          (let [empty-atlas (project/get-resource-node project empty-resource)]
            (g/transact (#'atlas/handle-drop empty-atlas [] workspace nil
                                             (into [(workspace/find-resource workspace "/game.project")] images)))
            (is (= mode (g/valid-node-value empty-atlas :atlas-mode)))
            (is (= [(resource/proj-path (first images))]
                   (mapv :image (:images (g/valid-node-value empty-atlas :save-value))))))))
      (g/set-property! atlas :rename-patterns "shape=saved")
      (test-util/save-project! project)
      (let [atlas-resource (g/valid-node-value atlas :resource)
            read-fn (:read-fn (resource/resource-type atlas-resource))
            saved (first (resource/read-source-value+sha256-hex atlas-resource #(read-fn {} atlas-resource %)))]
        (is (= "shape=saved" (:rename-patterns saved)))
        (is (= (:images vector-before) (:images saved)))
        (let [reopened (test-util/make-resource! workspace "/reopened.atlas" saved)]
          (workspace/resource-sync! workspace)
          (is (= (g/valid-node-value atlas :texture-set)
                 (g/valid-node-value (project/get-resource-node project reopened) :texture-set))))))))

;; Raster GUI previews keep their original vertex size and premultiplied colors.
;; SVG playback time belongs only to the vector vertex layout.
(deftest gui-preview-vertex-layouts
  (let [renderable {:world-transform (doto (Matrix4d.) (.setIdentity))
                    :user-data {:geom-data [[1.0 2.0 3.0]]
                                :uv-data [[4.0 5.0]]
                                :color [1.0 0.5 0.25 0.5]
                                :page-index 6.0}}]
    (doseq [[gpu-texture expected] [[nil [1.0 2.0 3.0 4.0 5.0 0.5 0.25 0.125 0.5 6.0]]
                                    [{:vector true} [1.0 2.0 3.0 4.0 5.0 1.0 0.5 0.25 0.5 6.0 0.0]]]]
      (let [vertices (#'gui/gen-geom-vb [(assoc-in renderable [:user-data :gpu-texture] gpu-texture)])
            ^ByteBuffer buffer (vertex2/buf vertices)]
        (is (= 1 (count vertices)))
        (is (= (* Float/BYTES (count expected)) (.limit buffer)))
        (is (= expected (mapv (fn [^long index] (.getFloat buffer (int (* Float/BYTES index))))
                              (range (count expected)))))))))

;; Preserve Bob's payload, image variants and animations in editor builds.
(deftest svg-compiler-agreement
  (test-util/with-loaded-project "test/resources/svg_project"
    (let [atlas (project/get-resource-node project "/vector.atlas")
          image (project/get-resource-node project "/shape.svg")
          result (VectorAtlasCompiler/compile (protobuf/map->pb AtlasProto$Atlas (g/valid-node-value atlas :save-value))
                                              {"/shape.svg" (g/valid-node-value image :svg-content)})
          expected (protobuf/pb->map-with-defaults (.build (.-builder result)))]
      (is (= :vector (g/valid-node-value atlas :atlas-mode)))
      (is (= expected (g/valid-node-value atlas :texture-set)))
      (is (nil? (g/valid-node-value atlas :packed-page-images-generator)))
      (is (= 0 (g/valid-node-value atlas :texture-page-count)))
      (is (= 1 (count (g/valid-node-value atlas :build-targets)))))))

;; Vector atlases ignore matching profiles and profile errors; raster atlases
;; retain their profile dependency.
(deftest svg-atlas-ignores-texture-profiles
  (test-util/with-scratch-project "test/resources/svg_project"
    (test-util/write-file-resource! workspace "/test.texture_profiles"
      {:path-settings [{:path "/**" :profile "Raster"}]
       :profiles [{:name "Raster"
                   :platforms [{:os :os-id-generic
                                :mipmaps true
                                :premultiply-alpha true
                                :formats [{:format :texture-format-rgba}]}]}]})
    (workspace/resource-sync! workspace)
    (let [game-project (project/get-resource-node project "/game.project")
          vector-atlas (project/get-resource-node project "/vector.atlas")
          raster-atlas (project/get-resource-node project "/raster.atlas")]
      (test-util/set-setting! game-project ["graphics" "texture_profiles"]
                              (workspace/find-resource workspace "/test.texture_profiles"))
      (is (nil? (g/valid-node-value vector-atlas :texture-profile)))
      (is (= "Raster" (:name (g/valid-node-value raster-atlas :texture-profile))))
      (let [targets (g/valid-node-value vector-atlas :build-targets)]
        (test-util/set-setting! game-project ["graphics" "texture_profiles"]
                                (workspace/resolve-workspace-resource workspace "/missing.texture_profiles"))
        (is (g/error? (g/node-value raster-atlas :texture-profile)))
        (is (nil? (g/valid-node-value vector-atlas :texture-profile)))
        (is (= targets (g/valid-node-value vector-atlas :build-targets)))
        (is (map? (g/valid-node-value vector-atlas :scene)))))))

;; Vector atlas info displays its localized label without layout positions.
;; Raster previews retain their coordinates; neither case needs a GL preview.
(deftest atlas-preview-info
  (test-util/with-loaded-project "test/resources/svg_project"
    (doseq [[path expected] [["/vector.atlas" #"^Vector atlas$"] ["/raster.atlas" #"Position:"]]]
      (let [overlay (scene/produce-overlay-anchor-pane-props
                      {:cursor-pos [160.0 120.0]
                       :scene (g/valid-node-value (project/get-resource-node project path) :scene)
                       :tool-info-text nil
                       :active-updatable-ids #{}
                       :updatables {}
                       :camera (camera/make-camera :orthographic)
                       :viewport (types/->Region 0 320 0 240)
                       :localization test-util/localization
                       :keymap nil
                       :camera-inset-data nil})
            texts (ui/run-now
                    (let [component (fx/create-component (assoc overlay :fx/type :anchor-pane))]
                      (try
                        (mapv #(.getText ^Label %) (.getChildren ^AnchorPane (fx/instance component)))
                        (finally
                          (fx/delete-component component)))))]
        (is (coll/any? #(re-find expected %) texts) path)))))

;; Bob/editor agree on compact SVG tracks. Unsaved animation edits invalidate
;; their dependent atlases and GUI builds.
(deftest svg-animation-compiler-agreement
  (test-util/with-loaded-project "test/resources/svg_project"
    (let [atlas (project/get-resource-node project "/animated.atlas")
          image (project/get-resource-node project "/animated.svg")
          result (VectorAtlasCompiler/compile (protobuf/map->pb AtlasProto$Atlas (g/valid-node-value atlas :save-value))
                                              {"/animated.svg" (g/valid-node-value image :svg-content)})
          expected (protobuf/pb->map-with-defaults (.build (.-builder result)))
          before (g/valid-node-value atlas :texture-set)]
      (is (= expected before))
      (is (= 1 (count (:geometries before))))
      (is (= 1.0 (get-in before [:animations 0 :duration])))
      (is (= 60 (get-in before [:animations 0 :fps])))
      (is (= :playback-once-forward (get-in before [:animations 0 :playback])))
      (is (not (g/error? (g/node-value (project/get-resource-node project "/animated.gui") :build-targets))))
      (test-util/set-code-editor-lines! image ["<svg xmlns='http://www.w3.org/2000/svg' width='100' height='80'><rect width='40' height='40'><animate attributeName='fill' from='red' to='green' dur='.5s' fill='freeze'/></rect></svg>"])
      (let [after (g/valid-node-value atlas :texture-set)]
        (is (= 1 (count (:geometries after))))
        (is (= 0.5 (get-in after [:animations 0 :duration])))
        (is (not= (:vector before) (:vector after)))))))

;; SVG timelines support sprite preview playback with a single geometry frame.
;; The preview clock reaches vertex data; static SVG sprites stay idle.
(deftest svg-sprite-preview-animation
  (test-util/with-loaded-project "test/resources/svg_project"
    (let [animated (g/valid-node-value (project/get-resource-node project "/animated.sprite") :scene)
          static (g/valid-node-value (project/get-resource-node project "/vector.sprite") :scene)]
      (is (nil? (:updatable static)))
      (is (map? (:updatable animated)))
      (when-let [updatable (:updatable animated)]
        (let [state ((:update-fn updatable) (:initial-state updatable) {:dt 0.25})
              renderable (assoc (:renderable animated)
                           :world-transform (doto (Matrix4d.) (.setIdentity))
                           :updatable (assoc updatable :state state))
              data (#'sprite/renderable-data renderable)
              bytes (get-in data [:vertex-attribute-bytes :svg-time])]
          (is (= 0 (:frame state)))
          (is (= 0.25 (:svg-time state)))
          (is (= 0.25 (.getFloat (doto (ByteBuffer/wrap bytes) (.order (ByteOrder/nativeOrder)))))))))))

;; Vector previews evaluate continuous time and weighted clips without sampling.
(deftest svg-continuous-preview-time
  (let [animation {:duration 4.0
                   :playback :playback-loop-forward
                   :frame-durations [1.0 3.0]
                   :frames [{:vector-duration 1.0} {:vector-duration 3.0}]}
        early (texture-set/step-animation {:t 0.0 :frame 0} 0.003 animation)
        later (texture-set/step-animation {:t 1.0 :frame 1} 0.25 animation)]
    (is (= 0 (:frame early)))
    (is (= 0.003 (:svg-time early)))
    (is (= 1 (:frame later)))
    (is (= 0.25 (:svg-time later)))))

;; SVG edits must invalidate the dependent atlas payload without saving first.
(deftest svg-edits-rebuild-atlases
  (test-util/with-loaded-project "test/resources/svg_project"
    (let [atlas (project/get-resource-node project "/vector.atlas")
          image (project/get-resource-node project "/shape.svg")
          before (g/valid-node-value atlas :texture-set)]
      (test-util/set-code-editor-lines! image ["<svg xmlns='http://www.w3.org/2000/svg' width='160' height='80'><rect width='160' height='80' fill='lime'/></svg>"])
      (let [after (g/valid-node-value atlas :texture-set)]
        (is (not= (:vector before) (:vector after)))
        (is (= 160.0 (get-in after [:vector :images 0 :width])))))))

;; Validate mixed contents and keep invalid trim and rename settings repairable.
(deftest svg-atlas-validation
  (test-util/with-loaded-project "test/resources/svg_project"
    (let [mixed (project/get-resource-node project "/mixed.atlas")
          atlas (project/get-resource-node project "/vector.atlas")
          image (:node-id (coll/first-where #(g/node-instance? atlas/AtlasImage (:node-id %)) (:children (g/valid-node-value atlas :node-outline))))]
      (is (= :mixed (g/valid-node-value mixed :atlas-mode)))
      (is (g/error-fatal? (g/node-value mixed :build-targets)))
      (g/set-property! image :sprite-trim-mode :sprite-trim-mode-4)
      (is (g/error-fatal? (g/node-value atlas :build-targets)))
      (is (not (get-in (g/valid-node-value image :_properties) [:properties :sprite-trim-mode :read-only?])))
      (properties/set-values! (get-in (properties/coalesce [(g/valid-node-value image :_properties)]) [:properties :sprite-trim-mode])
                              [:sprite-trim-mode-off])
      (is (not (g/error? (g/node-value atlas :build-targets))))
      (g/set-property! atlas :rename-patterns "shape=one=two")
      (is (g/error-fatal? (test-util/prop-error atlas :rename-patterns)))
      (is (g/error-fatal? (g/node-value atlas :texture-set)))
      (g/set-property! atlas :rename-patterns "")
      (is (not (g/error? (g/node-value atlas :build-targets)))))))

;; Invalid SVG content leaves atlas and sprite image properties repairable.
(deftest invalid-svg-keeps-atlas-and-sprite-editable
  (test-util/with-loaded-project "test/resources/svg_project"
    (let [atlas (project/get-resource-node project "/vector.atlas")
          sprite (project/get-resource-node project "/vector.sprite")
          svg-image (project/get-resource-node project "/shape.svg")
          image-node (:node-id (coll/first-where #(g/node-instance? atlas/AtlasImage (:node-id %))
                                                 (:children (g/valid-node-value atlas :node-outline))))]
      (test-util/set-code-editor-lines! svg-image ["<svg broken"])
      (let [error (g/node-value svg-image :svg-image)]
        (is (g/error-fatal? error))
        (is (= :invalid-content (get-in error [:user-data :type])))
        (is (= "/shape.svg" (resource/proj-path (get-in error [:user-data :resource])))))
      (is (= :vector (g/valid-node-value atlas :atlas-mode)))
      (is (coll/not-empty (:children (g/valid-node-value atlas :node-outline))))
      (is (= #{"svg"} (get-in (g/valid-node-value image-node :_properties) [:properties :image :edit-type :ext])))
      (is (= (workspace/find-resource workspace "/vector.atlas")
             (test-util/prop sprite :__sampler__curve_texture__0)))
      (is (g/error-fatal? (test-util/prop-error sprite :__sampler__curve_texture__0)))
      (is (g/error-fatal? (g/node-value sprite :build-targets)))
      (let [replacement (workspace/find-resource workspace "/raster.atlas")]
        (test-util/prop! sprite :__sampler__curve_texture__0 replacement)
        (is (= replacement (test-util/prop sprite :__sampler__curve_texture__0))))
      (test-util/prop! sprite :material (workspace/find-resource workspace "/builtins/materials/sprite.material"))
      (test-util/prop! sprite :default-animation "raster")
      (is (not (g/error? (g/node-value sprite :build-targets)))))))

;; FPS edits leave raster and vector layout data cached.
(deftest animation-edits-preserve-atlas-layout
  (test-util/with-scratch-project "test/resources/svg_project"
    (let [raster-resource (test-util/make-resource! workspace "/animated-raster.atlas"
                                                    {:images [{:image "/raster.png"}]
                                                     :animations [{:id "loop" :fps 8 :images [{:image "/raster.png"}]}]})]
      (workspace/resource-sync! workspace)
      (doseq [resource [raster-resource (workspace/find-resource workspace "/vector.atlas")]]
        (let [atlas (project/get-resource-node project resource)
              animation (:node-id (coll/first-where #(g/node-instance? atlas/AtlasAnimation (:node-id %))
                                                    (:children (g/valid-node-value atlas :node-outline))))
              layout (g/valid-node-value atlas :layout-data)]
          (g/set-property! animation :fps 13)
          (is (identical? layout (g/valid-node-value atlas :layout-data)))
          (is (= 13 (:fps (coll/first-where #(= (g/valid-node-value animation :id) (:id %))
                                            (:animations (g/valid-node-value atlas :texture-set)))))))))))

;; Picker filtering needs only references; raster previews skip Slug programs.
(deftest raster-atlases-skip-vector-preparation
  (test-util/with-loaded-project "test/resources/svg_project"
    (let [reads (atom 0)
          shaders (atom 0)
          read-size image-util/read-size
          read-shader shader/read-combined-shader-info
          raster (project/get-resource-node project "/raster.atlas")]
      (g/clear-system-cache!)
      ;; Count calls while preserving the actual I/O and compilation behavior.
      #_{:clj-kondo/ignore [:discouraged-var]}
      (with-redefs [image-util/read-size (fn [resource] (swap! reads inc) (read-size resource))
                    shader/read-combined-shader-info (fn [& args] (swap! shaders inc) (apply read-shader args))]
        (is (svg/raster-atlas-resource? (workspace/find-resource workspace "/raster.atlas")))
        (is (not (svg/raster-atlas-resource? (workspace/find-resource workspace "/vector.atlas"))))
        (is (zero? @reads))
        (is (not (svg/vector-texture? (g/valid-node-value raster :gpu-texture))))
        (is (zero? @shaders))))))

;; SVG and atlas previews share programs for identical built-in sources.
(deftest vector-previews-share-programs
  (test-util/with-loaded-project "test/resources/svg_project"
    (let [svg-texture (g/valid-node-value (project/get-resource-node project "/shape.svg") :gpu-texture)
          atlas-texture (g/valid-node-value (project/get-resource-node project "/vector.atlas") :gpu-texture)]
      (is (identical? (:preview-shader svg-texture) (:preview-shader atlas-texture)))
      (is (identical? (:selection-shader svg-texture) (:selection-shader atlas-texture))))))

;; Invalid vector samplers produce material property errors and prevent builds.
(deftest vector-material-contract-errors
  (test-util/with-scratch-project "test/resources/svg_project"
    (let [sprite (project/get-resource-node project "/vector.sprite")
          material (g/valid-node-value (project/get-resource-node project "/builtins/materials/sprite-vector.material") :save-value)
          wrong-type (test-util/make-resource! workspace "/wrong-type.fp")
          materials [(test-util/make-resource! workspace "/wrong-type.material" (assoc material :fragment-program (resource/proj-path wrong-type)))
                     (test-util/make-resource! workspace "/wrong-filter.material"
                                               (update-in material [:samplers 0] assoc :filter-min :filter-mode-min-linear))]]
      (spit wrong-type "#version 140\nuniform sampler2D curve_texture;\nuniform sampler2D band_texture;\nuniform sampler2D paint_texture;\nout vec4 color;\nvoid main() { color = texelFetch(curve_texture, ivec2(0), 0) + texelFetch(band_texture, ivec2(0), 0) + texelFetch(paint_texture, ivec2(0), 0); }\n")
      (workspace/resource-sync! workspace)
      (doseq [material-resource materials]
        (test-util/prop! sprite :material material-resource)
        (let [error (test-util/prop-error sprite :material)]
          (is (g/error-fatal? error))
          (is (= material-resource (:value error))))
        (is (g/error-fatal? (g/node-value sprite :build-targets)))))))

;; Expose one image and accept nine-slice while rejecting raster materials.
(deftest vector-sprite-validation
  (test-util/with-loaded-project "test/resources/svg_project"
    (let [sprite (project/get-resource-node project "/vector.sprite")]
      (is (= ["curve_texture"] (mapv :name (g/valid-node-value sprite :material-samplers))))
      (is (not (g/error? (g/node-value sprite :build-targets))))
      (g/set-property! sprite :slice9 [1.0 0.0 0.0 0.0])
      (g/set-property! sprite :size-mode :size-mode-manual)
      (is (not (g/error? (g/node-value sprite :build-targets))))
      (is (not (get-in (g/valid-node-value sprite :_properties) [:properties :slice9 :read-only?])))
      (test-util/prop! sprite :material (workspace/find-resource workspace "/builtins/materials/sprite.material"))
      (is (g/error-fatal? (g/node-value sprite :build-targets))))))

;; Switching to SVG preserves authored borders, including undo and save/reopen.
(deftest vector-sprite-preserves-slice9-with-undo
  (test-util/with-scratch-project "test/resources/svg_project"
    (let [sprite (project/get-resource-node project "/raster.sprite")
          binding (:_node-id (first (g/valid-node-value sprite :texture-binding-infos)))]
      (properties/set-values! (get-in (properties/coalesce [(g/valid-node-value binding :_properties)])
                                      [:properties :texture])
                              [(workspace/find-resource workspace "/vector.atlas")])
      (is (= [8.0 8.0 8.0 8.0] (g/valid-node-value sprite :slice9)))
      (g/undo! :undo/global)
      (is (= [8.0 8.0 8.0 8.0] (g/valid-node-value sprite :slice9)))
      (g/redo! :undo/global)
      (test-util/prop! sprite :material (workspace/find-resource workspace "/builtins/materials/sprite-vector.material"))
      (test-util/prop! sprite :default-animation "shape")
      (is (not (g/error? (g/node-value sprite :build-targets))))
      (test-util/save-project! project)
      (let [reopened (test-util/make-resource! workspace "/reopened.sprite" (g/valid-node-value sprite :save-value))]
        (workspace/resource-sync! workspace)
        (is (= (g/valid-node-value sprite :save-value)
               (g/valid-node-value (project/get-resource-node project reopened) :save-value)))))))

;; SVG cells retain viewport cuts, paint and pivot through resize and flips.
(deftest vector-sprite-slice9-geometry
  (test-util/with-loaded-project "test/resources/svg_project"
    (let [atlas (project/get-resource-node project "/vector.atlas")
          animations (g/valid-node-value atlas :anim-data)]
      (doseq [animation ["shape" "flipbook"]
              [width height] [[300.75 200.25] [40.0 35.0]]]
        (let [frame (first (:frames (get animations animation)))
              flipped (= "flipbook" animation)
              [px py] (:pivot frame)
              xs [0.0 (if flipped 30.0 10.0) (- width (if flipped 10.0 30.0)) width]
              ys [0.0 (if flipped 20.0 15.0) (- height (if flipped 15.0 20.0)) height]
              out (texture-set/vertex-data frame :size-mode-manual [width height] [10.0 20.0 30.0 15.0] :pivot-center)]
          (is (= 6 (count (:position-data (texture-set/vertex-data frame :size-mode-manual [width height] nil :pivot-center)))))
          (is (= (:page-index frame) (:page-index out)))
          (is (= (coll/into-> xs #{} (map #(- % (* (+ 0.5 px) width))))
                 (coll/into-> (:position-data out) #{} (map first))))
          (is (= (coll/into-> ys #{} (map #(- % (* (+ 0.5 py) height))))
                 (coll/into-> (:position-data out) #{} (map second))))
          (when (= width 300.75)
            (is (= 54 (count (:position-data out))))
            (is (= #{0.0 10.0 70.0 100.0} (coll/into-> (:uv-data out) #{} (map first))))
            (is (= #{0.0 20.0 65.0 80.0} (coll/into-> (:uv-data out) #{} (map second))))))))))

;; GUI vector texture validation remains editable, preserves nine-slice on save,
;; and rebuilds from unsaved SVG content through the regular graph dependencies.
(deftest svg-gui-boxes
  (test-util/with-scratch-project "test/resources/svg_project"
    (let [gui (project/get-resource-node project "/vector.gui")
          box (get (g/valid-node-value gui :node-ids) "box")
          image (project/get-resource-node project "/shape.svg")
          before (g/valid-node-value box :scene-renderable-user-data)]
      (is (nil? (test-util/prop-error box :material)))
      (is (not (g/error? (g/node-value gui :build-targets))))
      (is (= 54 (count (:geom-data before))))
      (g/set-property! box :material "")
      (is (g/error-fatal? (test-util/prop-error box :material)))
      (properties/set-values! (get-in (properties/coalesce [(g/valid-node-value box :_properties)]) [:properties :material]) ["vector"])
      (is (nil? (test-util/prop-error box :material)))
      (g/undo! :undo/global)
      (is (g/error-fatal? (test-util/prop-error box :material)))
      (g/redo! :undo/global)
      (is (nil? (test-util/prop-error box :material)))
      (is (= [8.0 12.0 16.0 20.0] (g/valid-node-value box :slice9)))
      (test-util/set-code-editor-lines! image ["<svg xmlns='http://www.w3.org/2000/svg' width='160' height='80'><rect width='160' height='80' fill='red'/></svg>"])
      (is (not= (:uv-data before) (:uv-data (g/valid-node-value box :scene-renderable-user-data))))
      (is (not (g/error? (g/node-value gui :build-targets))))
      (test-util/save-project! project)
      (let [saved (g/valid-node-value gui :save-value)
            reopened (test-util/make-resource! workspace "/reopened.gui" saved)]
        (workspace/resource-sync! workspace)
        (is (= saved (g/valid-node-value (project/get-resource-node project reopened) :save-value)))))))

;; SVG, atlas, sprite and GUI previews must render gradients and support
;; picking.
(deftest svg-preview-rendering
  (test-util/with-loaded-project "test/resources/svg_project"
    (doseq [[path large-viewbox] [["/shape.svg" false] ["/vector.atlas" false] ["/vector.go" false]
                                  ["/sliced.go" false] ["/vector.gui" false] ["/animated.go" false]
                                  ["/animated.gui" false] ["/animated.go" true] ["/animated.gui" true]]]
      (when large-viewbox
        ;; A small viewport scale is valid and must not be treated as singular.
        (test-util/set-code-editor-lines! (project/get-resource-node project "/animated.svg")
          ["<svg xmlns='http://www.w3.org/2000/svg' width='100' height='80' viewBox='0 0 1000000000 800000000'><rect width='400000000' height='400000000' fill='red'><animate attributeName='fill' from='red' to='blue' dur='1s'/></rect></svg>"]))
      (let [[_node view] (test-util/open-scene-view! project app-view path 320 240)]
        (try
          (let [^BufferedImage image (g/valid-node-value view :frame)
                pixels (.getRGB image 0 0 (.getWidth image) (.getHeight image) nil 0 (.getWidth image))
                red-pixels (coll/into-> pixels [] (keep-indexed (fn [index rgb]
                                                                  (when (> (bit-and 255 (bit-shift-right rgb 16))
                                                                           (+ 50 (bit-and 255 rgb))) index))))]
            (is (< 100 (count red-pixels)) path)
            (when (= "/vector.go" path)
              (let [green-pixels (coll/into-> pixels []
                                   (keep-indexed
                                     (fn [index rgb]
                                       (let [r (bit-and 255 (bit-shift-right rgb 16))
                                             green (bit-and 255 (bit-shift-right rgb 8))
                                             b (bit-and 255 rgb)]
                                         (when (and (> green (+ r 30)) (> green (+ b 15)))
                                           index)))))
                    mean-y (fn [indices]
                             (/ (transduce (map #(quot % (.getWidth image))) + indices)
                                (max 1 (count indices))))]
                ;; Readback starts at the bottom.
                ;; The green circle is above the center of the SVG.
                (is (< 10 (count green-pixels)))
                (is (> (mean-y green-pixels) (mean-y red-pixels)))))
            (when-let [index (first red-pixels)]
              (test-util/mouse-click! view (rem index (.getWidth image)) (- (.getHeight image) 1 (quot index (.getWidth image))))
              (is (coll/not-empty (g/node-value app-view :selected-node-ids)) path)))
          (finally
            ((:dispose-preview-fn (workspace/get-view-type workspace :scene)) view)
            (test-util/close-tab! project app-view path)))))))
