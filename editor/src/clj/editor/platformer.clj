(ns editor.platformer
  (:require [clojure.set :refer [difference union]]
            [clojure.edn :as edn]
            [dynamo.background :as background]
            [dynamo.buffers :refer :all]
            [dynamo.camera :refer :all]
            [dynamo.file :as file]
            [dynamo.file.protobuf :as protobuf :refer [pb->str]]
            [dynamo.geom :as geom]
            [dynamo.gl :as gl]
            [dynamo.gl.shader :as shader]
            [dynamo.gl.texture :as texture]
            [dynamo.gl.vertex :as vtx]
            [dynamo.graph :as g]
            [dynamo.grid :as grid]
            [dynamo.image :refer :all]
            [dynamo.node :as n]
            [dynamo.project :as p]
            [dynamo.property :as dp]
            [dynamo.system :as ds]
            [dynamo.texture :as tex]
            [dynamo.types :as t :refer :all]
            [dynamo.ui :refer :all]
            [editor.camera :as c]
            [editor.image-node :as ein]
            [editor.scene-editor :as sceneed]
            [internal.render.pass :as pass]
            [plumbing.core :refer [fnk defnk]]
            [schema.core :as s]
            [schema.macros :as sm]
            [service.log :as log])
  (:import [com.dynamo.graphics.proto Graphics$Cubemap Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.jogamp.opengl.util.awt TextRenderer]
           [dynamo.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Point3d Matrix4d]))

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

(defnk produce-renderable :- RenderData
  [this base-texture vertex-buffer]
  (if vertex-buffer
    (let [world (Matrix4d. geom/Identity4d)]
      {pass/transparent
       [{:world-transform world
         :render-fn       (fn [ctx gl glu text-renderer] (render-platformer gl base-texture vertex-buffer))}]})
    {})
  )

(defn find-resource-nodes [project exts]
  (let [all-resource-nodes (filter (fn [node] (let [filename (:filename node)]
                                                (and filename (contains? exts (t/extension filename))))) (map first (ds/sources-of project :nodes)))
        filenames (map (fn [node]
                         (let [filename (:filename node)]
                           (str "/" (t/local-path filename)))) all-resource-nodes)]
    (zipmap filenames all-resource-nodes)))

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

(defnk produce-vertex-buffer [control-points]
  (let [cps (filter-cps control-points)
        quad-count (- (count cps) 1)]
    (when (> quad-count 0)
      (let [mesh (gen-mesh cps)
            vbuf  (->texture-vtx (* 6 quad-count))]
        (doseq [v mesh]
          (conj! vbuf v))
        (persistent! vbuf)))))

(n/defnode PlatformerRender
  #_(input gpu-textures {s/Str s/Any})
  (input control-points [s/Any])
  (input base-texture s/Any)

  (output vertex-buffer s/Any      produce-vertex-buffer)
  (output renderable    RenderData :cached produce-renderable)
  (output aabb          AABB       :cached (fnk [] geom/unit-bounding-box)))

(defn- hit? [cp pos camera viewport]
  (let [screen-cp (c/camera-project camera viewport (Point3d. (:x cp) (:y cp) 0))
        xd (- (:x pos) (.x screen-cp))
        yd (- (:y pos) (.y screen-cp))
        d-sq (* cp-trigger cp-trigger)]
    (when (and (<= (* xd xd) d-sq)
               (<= (* yd yd) d-sq))
      cp)))

(let [test [0 1 2 3]]
  (filter #(not= %1 1) test))

(let [test [0 1 2 3]]
  (conj test 5))

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
        (do (ds/set-property self :control-points (conj inactive-cps world-pos))
          nil)
        action)
      :mouse-pressed
      (let [click-count (get action :click-count 0)]
        (if (= click-count 2)
          (ds/set-property self :control-points
                           (if cp-hit
                             (filter #(not= %1 cp-hit) control-points)
                             (conj control-points world-pos)))
          (do
            (ds/set-property self :active-cp world-pos)
            (ds/set-property self :inactive-cps (filter #(not= %1 cp-hit) control-points))))
        (when (nil? cp-hit) action))
      :mouse-released
      (do
        (ds/set-property self :active-cp nil)
        (ds/set-property self :inactive-cps nil)
        (when (nil? active-cp) action))
      action)))

(defnk produce-gpu-textures
  []
  #_(apply texture/image-texture (map :contents [right-img left-img top-img bottom-img front-img back-img]))
  nil)

(n/defnode PlatformerNode
  (inherits n/ResourceNode)
  (inherits n/OutlineNode)

  (property control-points  [s/Any])
  (property base-texture s/Str)

  ; TODO temp solution
  (property active-cp s/Any)
  (property inactive-cps s/Any)

  (input camera s/Any)
  (input viewport Region)
  (input base-texture-img s/Any)

  (output input-handler Runnable (fnk [] handle-input))
  (output base-texture s/Any :cached (fnk [base-texture-img] (texture/image-texture 
                                                               (:contents base-texture-img)
                                                               {:min-filter gl/linear-mipmap-linear
                                                                :mag-filter gl/linear
                                                                :wrap-s     gl/repeat
                                                                :wrap-t     gl/repeat})))

  (on :load
      (let [project (:project event)
            level (edn/read-string (slurp (:filename self)))]
        (ds/set-property self :control-points (:control-points level))
        (ds/set-property self :base-texture (:base-texture level))
        (let [img-node (t/lookup project (:base-texture level))]
          (ds/connect img-node :content self :base-texture-img)))))

(defn construct-platformer-editor
  [project-node platformer-node]
  (let [editor (n/construct sceneed/SceneEditor)]
    (ds/in (ds/add editor)
           (let [platformer-render (ds/add (n/construct PlatformerRender))
                 renderer     (ds/add (n/construct sceneed/SceneRenderer))
                 background   (ds/add (n/construct background/Gradient))
                 grid         (ds/add (n/construct grid/Grid))
                 camera       (ds/add (n/construct c/CameraController :camera (c/make-camera :orthographic)))]
             (ds/connect background   :renderable      renderer     :renderables)
             (ds/connect grid         :renderable      renderer     :renderables)
             (ds/connect camera       :camera          grid         :camera)
             (ds/connect camera       :camera          renderer     :camera)
             (ds/connect camera       :input-handler   editor       :input-handlers)
             (ds/connect editor       :viewport        camera       :viewport)
             (ds/connect editor       :viewport        renderer     :viewport)
             (ds/connect editor       :drawable        renderer     :drawable)
             (ds/connect renderer     :frame           editor       :frame)

             (ds/connect platformer-node   :base-texture     platformer-render :base-texture)
             (ds/connect camera         :camera      platformer-node     :camera)
             (ds/connect platformer-node  :input-handler editor         :input-handlers)
             (ds/connect editor         :viewport     platformer-node     :viewport)
             (ds/connect platformer-node   :control-points  platformer-render :control-points)
             (ds/connect platformer-render :renderable      renderer     :renderables)
             )
           editor)))
