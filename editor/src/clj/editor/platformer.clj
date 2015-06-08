(ns editor.platformer
  (:require [clojure.edn :as edn]
            [clojure.pprint :refer [pprint]]
            [clojure.java.io :as io]
            [dynamo.buffers :refer :all]
            [dynamo.camera :refer :all]
            [dynamo.geom :as geom]
            [dynamo.gl :as gl]
            [dynamo.gl.shader :as shader]
            [dynamo.gl.texture :as texture]
            [dynamo.gl.vertex :as vtx]
            [dynamo.graph :as g]
            [dynamo.types :as t :refer :all]
            [dynamo.ui :refer :all]
            [editor.camera :as c]
            [editor.project :as project]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [internal.render.pass :as pass])
  (:import [com.dynamo.graphics.proto Graphics$Cubemap Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.jogamp.opengl.util.awt TextRenderer]
           [dynamo.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Point3d Matrix4d]))

(def platformer-icon "icons/diagramm.png")

(def cp-trigger 5)

(vtx/defvertex texture-vtx
  (vec3 position)
  (vec2 texcoord0))

(shader/defshader pos-uv-vert
  (attribute vec4 position)
  (attribute vec2 texcoord0)
  (varying vec2 var_texcoord0)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_texcoord0 texcoord0)))

(shader/defshader pos-uv-frag
  (varying vec2 var_texcoord0)
  (uniform sampler2D texture)
  (defn void main []
    (setq gl_FragColor (texture2D texture var_texcoord0.xy))
    #_(setq gl_FragColor (vec4 1 1 1 1))))

(def platformer-shader (shader/make-shader pos-uv-vert pos-uv-frag))

(defn render-platformer
  [^GL2 gl base-texture vertex-buffer]
  (let [vcount (count vertex-buffer)
        vertex-binding (vtx/use-with vertex-buffer platformer-shader)]
    (gl/with-enabled gl [base-texture platformer-shader vertex-binding]
      (shader/set-uniform platformer-shader gl "texture" 0)
      (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 vcount))))

(defn gen-mesh [control-points]
  (let [min-y (reduce #(min %1 (:y %2)) 0 control-points)
        pairs (partition 2 1 control-points)
        tex-s (/ 1 600.0)
        mesh (mapcat (fn [[cp0 cp1]]
                       (let [x0 (:x cp0)
                             y0 (:y cp0)
                             x1 (:x cp1)
                             y1 (:y cp1)
                             z 0.1
                             v00 [x0 min-y z (* x0 tex-s) (* min-y tex-s)]
                             v10 [x1 min-y z (* x1 tex-s) (* min-y tex-s)]
                             v01 [x0 y0 z (* x0 tex-s) (* y0 tex-s)]
                             v11 [x1 y1 z (* x1 tex-s) (* y1 tex-s)]]
                         [v00 v10 v01 v10 v11 v01]))
                   pairs)]
    mesh))

(defn filter-cps [control-points]
  (sort-by :x (flatten control-points)))

(defn- hit? [cp pos camera viewport]
  (let [screen-cp (c/camera-project camera viewport (Point3d. (:x cp) (:y cp) 0))
        xd (- (:x pos) (.x screen-cp))
        yd (- (:y pos) (.y screen-cp))
        d-sq (* cp-trigger cp-trigger)]
    (when (and (<= (* xd xd) d-sq)
               (<= (* yd yd) d-sq))
      cp)))

(defn handle-input [self action]
  (let [camera (g/node-value self :camera)
        viewport (g/node-value self :viewport)
        control-points (g/node-value self :control-points)
        active-cp (g/node-value self :active-cp)
        inactive-cps (g/node-value self :inactive-cps)
        pos {:x (:x action) :y (:y action)}
        world-pos-v4 (c/camera-unproject camera viewport (:x action) (:y action) 0)
        world-pos {:x (.x world-pos-v4) :y (.y world-pos-v4)}
        cp-hit (some #(hit? % pos camera viewport) control-points)]
    (case (:type action)
      :mouse-moved
      (if active-cp
        (do (g/set-property self :control-points (conj inactive-cps world-pos))
          nil)
        action)
      :mouse-pressed
      (let [click-count (get action :click-count 0)]
        (if (= click-count 2)
          (g/set-property self :control-points
                           (if cp-hit
                             (filter #(not= %1 cp-hit) control-points)
                             (conj control-points world-pos)))
          (do
            (g/set-property self :active-cp world-pos)
            (g/set-property self :inactive-cps (filter #(not= %1 cp-hit) control-points))))
        (when (nil? cp-hit) action))
      :mouse-released
      (do
        (g/set-property self :active-cp nil)
        (g/set-property self :inactive-cps nil)
        (when (nil? active-cp) action))
      action)))

(g/defnode PlatformerController
  (property active-cp t/Any (visible (g/fnk [] false)))
  (property inactive-cps t/Any (visible (g/fnk [] false)))

  (input source t/Any)
  (input camera t/Any)
  (input viewport Region)

  (output input-handler Runnable (g/fnk [] handle-input)))

(g/defnk produce-save-data [resource control-points base-texture]
  {:resource resource
   :content (with-out-str (pprint {:control-points control-points :base-texture base-texture}))})

(defn gen-vertex-buffer [control-points]
  (let [cps (filter-cps control-points)
        quad-count (- (count cps) 1)]
    (when (> quad-count 0)
      (let [mesh (gen-mesh cps)
            vbuf  (->texture-vtx (* 6 quad-count))]
        (doseq [v mesh]
          (conj! vbuf v))
        (persistent! vbuf)))))

(g/defnk produce-scene
  [self aabb base-texture-tex control-points]
  (let [scene {:node-id (g/node-id self) :aabb aabb}
        vertex-buffer (gen-vertex-buffer control-points)]
    (if vertex-buffer
      (assoc scene :renderable {:render-fn (fn [gl render-args renderables count]
                                             (render-platformer gl base-texture-tex vertex-buffer))
                                :passes [pass/transparent]})
     scene)))

(g/defnode PlatformerNode
  (inherits project/ResourceNode)

  (property control-points  [t/Any] (visible (g/fnk [] false)))
  (property base-texture t/Str)

  (input base-texture-img BufferedImage)

  (output base-texture-tex t/Any  :cached (g/fnk [base-texture-img] (texture/image-texture
                                                                     base-texture-img
                                                                     {:min-filter gl/linear-mipmap-linear
                                                                      :mag-filter gl/linear
                                                                      :wrap-s     gl/repeat
                                                                      :wrap-t     gl/repeat})))
  (output aabb          AABB  :cached (g/fnk [control-points] (reduce (fn [aabb cp] (geom/aabb-incorporate aabb (:x cp) (:y cp) 0)) (geom/null-aabb) control-points)))
  (output save-data     t/Any :cached produce-save-data)
  (output scene         t/Any :cached produce-scene))

(defn load-level [project self input]
  (with-open [reader (PushbackReader. (io/reader (:resource self)))]
    (let [level (edn/read reader)]
      (concat
       (g/set-property self :control-points (:control-points level))
       (g/set-property self :base-texture (:base-texture level))
       (if-let [img-node (project/resolve-resource-node self (:base-texture level))]
         (g/connect img-node :content self :base-texture-img)
         [])))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "platformer"
                                    :node-type PlatformerNode
                                    :load-fn load-level
                                    :icon platformer-icon
                                    :view-types [:scene]
                                    :view-opts {:scene {:grid true}}))
