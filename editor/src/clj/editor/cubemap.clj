(ns editor.cubemap
  (:require [clojure.set :refer [difference union]]
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
           [dynamo.types Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d]))

(vtx/defvertex normal-vtx
  (vec3 position)
  (vec3 normal))

(def unit-sphere
  (let [lats 16
        longs 32
        vbuf (->normal-vtx (* 6 (* lats longs)))]
    (doseq [face (geom/unit-sphere-pos-nrm lats longs)
            v    face]
      (conj! vbuf v))
    (persistent! vbuf)))

(shader/defshader pos-norm-vert
  (uniform mat4 world)
  (attribute vec3 position)
  (attribute vec3 normal)
  (varying vec3 vWorld)
  (varying vec3 vNormal)
  (defn void main []
    (setq vNormal (normalize (* (mat3 (.xyz (nth world 0)) (.xyz (nth world 1)) (.xyz (nth world 2))) normal)))
    (setq vWorld (.xyz (* world (vec4 position 1.0))))
    (setq gl_Position (* gl_ModelViewProjectionMatrix (vec4 position 1.0)))))

(shader/defshader pos-norm-frag
  (uniform vec3 cameraPosition)
  (uniform samplerCube envMap)
  (varying vec3 vWorld)
  (varying vec3 vNormal)
  (defn void main []
    (setq vec3 camToV (normalize (- vWorld cameraPosition)))
    (setq vec3 refl (reflect camToV vNormal))
      (setq gl_FragColor (textureCube envMap refl))))

(def cubemap-shader (shader/make-shader pos-norm-vert pos-norm-frag))

(defn render-cubemap
  [^GL2 gl world camera gpu-texture vertex-binding]
  (gl/with-enabled gl [gpu-texture cubemap-shader vertex-binding]
    (shader/set-uniform cubemap-shader gl "world" world)
    (shader/set-uniform cubemap-shader gl "cameraPosition" (t/position camera))
    (shader/set-uniform cubemap-shader gl "envMap" 0)
    (gl/gl-enable gl GL/GL_CULL_FACE)
    (gl/gl-cull-face gl GL/GL_BACK)
    (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (* 6 (* 16 32)))
    (gl/gl-disable gl GL/GL_CULL_FACE)))

(defnk produce-renderable :- RenderData
  [this camera gpu-texture vertex-binding]
  (let [world (Matrix4d. geom/Identity4d)]
    {pass/transparent
     [{:world-transform world
       :render-fn       (fn [ctx gl glu text-renderer] (render-cubemap gl world camera gpu-texture vertex-binding))}]}))

(defn find-resource-nodes [project exts]
  (let [all-resource-nodes (filter (fn [node] (let [filename (:filename node)]
                                                (and filename (contains? exts (t/extension filename))))) (map first (ds/sources-of project :nodes)))
        filenames (map (fn [node]
                         (let [filename (:filename node)]
                           (str "/" (t/local-path filename)))) all-resource-nodes)]
    (zipmap filenames all-resource-nodes)))

(n/defnode CubemapRender
  (input gpu-texture s/Any)

  (input  camera        Camera)

  (output vertex-binding s/Any     :cached (fnk [] (vtx/use-with unit-sphere cubemap-shader)))
  (output renderable    RenderData :cached produce-renderable)
  (output aabb          AABB       :cached (fnk [] geom/unit-bounding-box)))

(defnk produce-gpu-texture
  [right-img left-img top-img bottom-img front-img back-img]
  (apply texture/image-cubemap-texture (map :contents [right-img left-img top-img bottom-img front-img back-img])))

(n/defnode CubemapNode
  (inherits n/ResourceNode)
  (inherits n/OutlineNode)

  (property right  dp/ImageResource)
  (property left   dp/ImageResource)
  (property top    dp/ImageResource)
  (property bottom dp/ImageResource)
  (property front  dp/ImageResource)
  (property back   dp/ImageResource)

  (input right-img  BufferedImage)
  (input left-img   BufferedImage)
  (input top-img    BufferedImage)
  (input bottom-img BufferedImage)
  (input front-img  BufferedImage)
  (input back-img   BufferedImage)

  (output gpu-texture   s/Any  :cached produce-gpu-texture)

  (on :load
    (let [project (:project event)
          input (:filename self)
          cubemap-message (protobuf/pb->map (protobuf/read-text Graphics$Cubemap input))
          img-nodes (find-resource-nodes project #{"png" "jpg"})]
      (doseq [[side input] cubemap-message]
        (ds/set-property self side input)
        (when-let [img-node (get img-nodes input)]
          (ds/connect img-node :content self (keyword (subs (str side "-img") 1))))))))

(defn construct-cubemap-editor
  [project-node cubemap-node]
  (let [editor (n/construct sceneed/SceneEditor)]
    (ds/in (ds/add editor)
           (let [cubemap-render (ds/add (n/construct CubemapRender))
                 renderer     (ds/add (n/construct sceneed/SceneRenderer))
                 background   (ds/add (n/construct background/Gradient))
                 camera       (ds/add (n/construct c/CameraController :camera (c/make-camera :orthographic)))]
             (ds/connect background   :renderable      renderer     :renderables)
             (ds/connect camera       :camera          renderer     :camera)
             (ds/connect camera       :input-handler   editor       :input-handlers)
             (ds/connect editor       :viewport        camera       :viewport)
             (ds/connect editor       :viewport        renderer     :viewport)
             (ds/connect editor       :drawable        renderer     :drawable)
             (ds/connect renderer     :frame           editor       :frame)

             (ds/connect cubemap-node   :gpu-texture     cubemap-render :gpu-texture)
             (ds/connect cubemap-render :renderable      renderer     :renderables)
             (ds/connect camera       :camera            cubemap-render     :camera)
             )
           editor)))
