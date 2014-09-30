(ns atlas.core
  (:require [dynamo.ui :refer :all]
            [clojure.set :refer [union]]
            [plumbing.core :refer [fnk defnk]]
            [schema.core :as s]
            [schema.macros :as sm]
            [dynamo.buffers :refer :all]
            [dynamo.env :as e]
            [dynamo.geom :as g :refer [to-short-uv]]
            [dynamo.gl :as gl :refer [do-gl]]
            [dynamo.gl.shader :as shader]
            [dynamo.gl.texture :as texture]
            [dynamo.gl.protocols :as glp]
            [dynamo.gl.vertex :as vtx]
            [dynamo.node :refer :all]
            [dynamo.scene-editor :refer [dynamic-scene-editor]]
            [dynamo.file :refer :all]
            [dynamo.file.protobuf :refer [protocol-buffer-converters pb->str]]
            [dynamo.project :as p :refer [register-loader register-editor transact new-resource connect query]]
            [dynamo.types :refer :all]
            [dynamo.texture :refer :all]
            [dynamo.image :refer :all]
            [dynamo.outline :refer :all]
            [internal.render.pass :as pass]
            [service.log :as log :refer [logging-exceptions]]
            [camel-snake-kebab :refer :all]
            [clojure.osgi.core :refer [*bundle*]])
  (:import  [com.dynamo.atlas.proto AtlasProto AtlasProto$Atlas AtlasProto$AtlasAnimation AtlasProto$AtlasImage]
            [com.dynamo.graphics.proto Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
            [com.dynamo.textureset.proto TextureSetProto$Constants TextureSetProto$TextureSet TextureSetProto$TextureSetAnimation]
            [com.jogamp.opengl.util.awt TextRenderer]
            [java.nio ByteBuffer]
            [dynamo.types Animation Image TextureSet Rect EngineFormatTexture]
            [java.awt.image BufferedImage]
            [javax.media.opengl GL GL2]
            [javax.vecmath Matrix4d]))

(def integers (iterate (comp int inc) (int 0)))

(def include-debug false)

(defmacro debugging [& forms]
  (when include-debug
    `(do ~@forms)))

(vtx/defvertex engine-format-texture
  (vec3.float position)
  (vec2.short texcoord0 true))

(vtx/defvertex texture-vtx
  (vec4 position)
  (vec2 texcoord0))

(vtx/defvertex uv-only
  (vec2 uv))

(declare tex-outline-vertices)

;; consider moving -> protocol-buffer-related place
(defmacro set-if-present
  ([inst k props]
    `(set-if-present ~inst ~k ~props identity))
  ([inst k props xform]
    (let [setter (symbol (->camelCase (str "set-" (name k))))]
      `(when-let [value# (get ~props ~k)]
         (. ~inst ~setter (~xform value#))))))

(defnode ImageNode
  (inherits OutlineNode)
  (inherits ImageSource))

(defnk produce-animation :- Animation
  [this images :- [Image] id fps flip-horizontal flip-vertical playback]
  (->Animation id images fps flip-horizontal flip-vertical playback))

(defnode AnimationGroupNode
  (inherits OutlineNode)
  (inherits AnimationBehavior)

  (output animation Animation produce-animation))

(sm/defn ^:private consolidate :- [Image]
  [images :- [Image] containers :- [{:images [Image]}]]
  (seq (apply union
              (into #{} images)
              (map #(into #{} (:images %)) containers))))

(defnk produce-textureset :- TextureSet
  [this images :- [Image] animations :- [Animation] margin extrude-borders]
  (-> (pack-textures margin extrude-borders (consolidate images animations))
    (assoc :animations animations)))

(defnode AtlasProperties
  (input assets [OutlineItem])
  (input images [Image])
  (input animations [Animation])

  (property margin          (non-negative-integer))
  (property extrude-borders (non-negative-integer))
  (property filename        (string))

  (output textureset TextureSet :cached produce-textureset))

(sm/defn build-atlas-image :- AtlasProto$AtlasImage
  [image :- Image]
  (.build (doto (AtlasProto$AtlasImage/newBuilder)
            (.setImage (str (.path image))))))

(sm/defn build-atlas-animation :- AtlasProto$AtlasAnimation
  [animation :- Animation]
  (.build (doto (AtlasProto$AtlasAnimation/newBuilder)
            (.addAllImages      (map build-atlas-image (.images animation)))
            (.setId             (.id animation))
            (.setFps            (.fps animation))
            (.setFlipHorizontal (.flip-horizontal animation))
            (.setFlipVertical   (.flip-vertical animation))
            (.setPlayback       (.playback animation)))))

(defnk get-text-format :- s/Str
  "get the text string for this node"
  [this images :- [Image] animations :- [Animation]]
  (pb->str
    (.build
         (doto (AtlasProto$Atlas/newBuilder)
             (.addAllImages     (map build-atlas-image images))
             (.addAllAnimations (map build-atlas-animation animations))
             (set-if-present  :margin this)
             (set-if-present  :extrude-borders this)))))

(defnk save-atlas-file
  [this project filename]
  (let [text (p/get-resource-value project this :text-format)]
    (write-native-text-file filename text)
    :ok))

(defnode AtlasSave
  (output save s/Keyword save-atlas-file)
  (output text-format s/Str get-text-format))

(defn vertex-starts [n-vertices] (take n-vertices (take-nth 6 integers)))
(defn vertex-counts [n-vertices] (take n-vertices (repeat (int 6))))

(defn render-overlay
  [ctx ^GL2 gl ^TextRenderer text-renderer this project]
  (let [textureset ^TextureSet (p/get-resource-value project this :textureset)
        image      ^BufferedImage (.packed-image textureset)]
    (gl/overlay ctx gl text-renderer (format "Size: %dx%d" (.getWidth image) (.getHeight image)) 12.0 -22.0 1.0 1.0)))

(defnk produce-gpu-texture
  [project this gl]
  (texture/image-texture gl (:packed-image (p/get-resource-value project this :textureset))))


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
    (setq gl_FragColor (texture2D texture var_texcoord0.xy))))

(defnk produce-shader :- s/Int
  [this gl]
  (shader/make-shader gl pos-uv-vert pos-uv-frag))

(debugging
  (shader/defshader pos-red
    (varying vec2 var_texcoord0)
    (defn void main []
      (setq gl_FragColor (vec4 1.0 0.0 0.0 1.0))))

  (defn all-red [gl]
    (shader/make-shader gl pos-uv-vert pos-red)))

(defn wireframe
  [gl vbuf]
  (do-gl [wireframer (all-red gl)
          wf-bind    (vtx/use-with gl vbuf wireframer)]
         (gl/gl-polygon-mode gl GL/GL_FRONT_AND_BACK GL2/GL_LINE)
         (gl/gl-draw-arrays  gl GL/GL_TRIANGLES 0 (count vbuf))
         (gl/gl-polygon-mode gl GL/GL_FRONT_AND_BACK GL2/GL_FILL)))

(defn render-textureset
  [ctx gl this project]
  (do-gl [this            (assoc this :gl gl)
          textureset      (p/get-resource-value project this :textureset)
          texture         (p/get-resource-value project this :gpu-texture)
          shader          (p/get-resource-value project this :shader)
          vbuf            (p/get-resource-value project this :vertex-buffer)
          vertex-binding  (vtx/use-with gl vbuf shader)]
         (shader/set-uniform shader "texture" (texture/texture-unit-index texture))
         (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (* 6 (count (:coords textureset))))

         (debugging
           (wireframe gl vbuf))))

(defnk produce-renderable :- RenderData
  [this project]
  {pass/overlay
   [{:world-transform g/Identity4d  :render-fn       (fn [ctx gl glu text-renderer] (render-overlay ctx gl text-renderer this project))}]
   pass/transparent
   [{:world-transform g/Identity4d  :render-fn       (fn [ctx gl glu text-renderer] (render-textureset ctx gl this project))}]})

(defnk produce-renderable-vertex-buffer
  [project this gl]
  (let [textureset (p/get-resource-value project this :textureset)
        shader     (p/get-resource-value project this :shader)
        bounds     (:aabb textureset)
        coords     (:coords textureset)
        vbuf       (->texture-vtx (* 6 (count coords)))
        x-scale    (/ 1.0 (.width bounds))
        y-scale    (/ 1.0 (.height bounds))]
    (doseq [coord coords]
      (let [w  (.width coord)
            h  (.height coord)
            x0 (.x coord)
            y0 (- (.height bounds) (.y coord)) ;; invert for screen
            x1 (+ x0 w)
            y1 (- (.height bounds) (+ (.y coord) h))
            u0 (* x0 x-scale)
            v0 (* y0 y-scale)
            u1 (* x1 x-scale)
            v1 (* y1 y-scale)]
        (doto vbuf
          (conj! [x0 y0 0 1 u0 (- 1 v0)])
          (conj! [x0 y1 0 1 u0 (- 1 v1)])
          (conj! [x1 y1 0 1 u1 (- 1 v1)])

          (conj! [x1 y1 0 1 u1 (- 1 v1)])
          (conj! [x1 y0 0 1 u1 (- 1 v0)])
          (conj! [x0 y0 0 1 u0 (- 1 v0)]))))
    (persistent! vbuf)))

(defnk produce-outline-vertex-buffer
  [project this gl]
  (let [textureset (p/get-resource-value project this :textureset)
        shader     (p/get-resource-value project this :shader)
        bounds     (:aabb textureset)
        coords     (:coords textureset)
        vbuf       (->texture-vtx (* 6 (count coords)))
        x-scale    (/ 1.0 (.width bounds))
        y-scale    (/ 1.0 (.height bounds))]
    (doseq [coord coords]
      (let [w  (.width coord)
            h  (.height coord)
            x0 (.x coord)
            y0 (- (.height bounds) (.y coord)) ;; invert for screen
            x1 (+ x0 w)
            y1 (- (.height bounds) (+ y0 h))
            u0 (* x0 x-scale)
            v0 (* y0 y-scale)
            u1 (* x1 x-scale)
            v1 (* y1 y-scale)]
        (doto vbuf
          (conj! [x0 y0 0 1 u0 (- 1 v0)])
          (conj! [x0 y1 0 1 u0 (- 1 v1)])
          (conj! [x1 y1 0 1 u1 (- 1 v1)])
          (conj! [x1 y0 0 1 u1 (- 1 v0)]))))
    (persistent! vbuf)))

(defnode AtlasRender
  (output shader s/Any                :cached produce-shader)
  (output vertex-buffer s/Any         :cached produce-renderable-vertex-buffer)
  (output outline-vertex-buffer s/Any :cached produce-outline-vertex-buffer)
  (output gpu-texture s/Any           :cached produce-gpu-texture)
  (output renderable RenderData  produce-renderable))

(defnode AtlasNode
  (inherits OutlineNode)
  (inherits AtlasProperties)
  (inherits AtlasRender)
  (inherits AtlasSave))

(protocol-buffer-converters
 AtlasProto$Atlas
 {:constructor #'atlas.core/make-atlas-node
  :basic-properties [:extrude-borders :margin]
  :node-properties  {:images-list [:tree -> :children,
                                   :image -> :images]
                     :animations-list [:tree -> :children,
                                       :animation -> :animations]}}

 AtlasProto$AtlasAnimation
 {:constructor      #'atlas.core/make-animation-group-node
  :basic-properties [:id :playback :fps :flip-horizontal :flip-vertical]
  :node-properties  {:images-list [:tree -> :children,
                                   :image -> :images]}}

 AtlasProto$AtlasImage
 {:constructor #'atlas.core/make-image-node
  :basic-properties [:image]})

(def ^:private outline-vertices-per-placement 4)
(def ^:private vertex-size (.getNumber TextureSetProto$Constants/VERTEX_SIZE))

(sm/defn textureset->texcoords
  [textureset]
  (let [x-scale    (/ 1.0 (.getWidth (.packed-image textureset)))
        y-scale    (/ 1.0 (.getHeight (.packed-image textureset)))
        coords     (:coords textureset)
        bounds     (:aabb textureset)
        vbuf       (->uv-only (* 2 (count coords)))]
    (doseq [coord coords]
      (let [x0 (.x coord)
            y0 (.y coord)
            x1 (+ (.x coord) (.width coord))
            y1 (+ (.y coord) (.height coord))
            u0 (* x0 x-scale)
            u1 (* x1 x-scale)
            v0 (* y0 y-scale)
            v1 (* y1 y-scale)]
        (doto vbuf
          (conj! [u0 v0])
          (conj! [u1 v1]))))
    (persistent! vbuf)))

(sm/defn textureset->vertices
  [textureset]
  (let [x-scale    (/ 1.0 (.getWidth (.packed-image textureset)))
        y-scale    (/ 1.0 (.getHeight (.packed-image textureset)))
        coords     (:coords textureset)
        bounds     (:aabb textureset)
        vbuf       (->engine-format-texture (* 6 (count coords)))]
    (doseq [coord coords]
      (let [x0 (.x coord)
            y0 (.y coord)
            x1 (+ (.x coord) (.width coord))
            y1 (+ (.y coord) (.height coord))
            w2 (* (.width coord) 0.5)
            h2 (* (.height coord) 0.5)
            u0 (to-short-uv (* x0 x-scale))
            u1 (to-short-uv (* x1 x-scale))
            v0 (to-short-uv (* y0 y-scale))
            v1 (to-short-uv (* y1 y-scale))]
        (doto vbuf
          (conj! [(- w2) (- h2) 0 u0 v1])
          (conj! [   w2  (- h2) 0 u1 v1])
          (conj! [   w2     h2  0 u1 v0])

          (conj! [(- w2) (- h2) 0 u0 v1])
          (conj! [   w2     h2  0 u1 v0])
          (conj! [(- w2)    h2  0 v0 v0]))))
    (persistent! vbuf)))

(sm/defn textureset->outline-vertices
  [textureset]
  (let [x-scale    (/ 1.0 (.getWidth (.packed-image textureset)))
        y-scale    (/ 1.0 (.getHeight (.packed-image textureset)))
        coords     (:coords textureset)
        bounds     (:aabb textureset)
        vbuf       (->engine-format-texture (* 6 (count coords)))]
    (doseq [coord coords]
      (let [x0 (.x coord)
            y0 (.y coord)
            x1 (+ (.x coord) (.width coord))
            y1 (+ (.y coord) (.height coord))
            w2 (* (.width coord) 0.5)
            h2 (* (.height coord) 0.5)
            u0 (to-short-uv (* x0 x-scale))
            u1 (to-short-uv (* x1 x-scale))
            v0 (to-short-uv (* y0 y-scale))
            v1 (to-short-uv (* y1 y-scale))]
        (doto vbuf
          (conj! [(- w2) (- h2) 0 u0 v1])
          (conj! [   w2  (- h2) 0 u1 v1])
          (conj! [   w2     h2  0 u1 v0])
          (conj! [(- w2)    h2  0 v0 v0]))))
    (persistent! vbuf)))

(defn build-animation
  [anim begin]
  #_(s/validate Animation anim)
  (let [start     (int begin)
        end       (int (+ begin (* 6 (count (:images anim)))))]
    (.build
      (doto (TextureSetProto$TextureSetAnimation/newBuilder)
         (.setId        (:id anim))
         (.setWidth     (int (:width  (first (:images anim)))))
         (.setHeight    (int (:height (first (:images anim)))))
         (.setStart     start)
         (.setEnd       end)
         (set-if-present :playback anim)
         (set-if-present :fps anim)
         (set-if-present :flip-horizontal anim)
         (set-if-present :flip-vertical anim)
         (set-if-present :is-animation anim)))))

(defn build-animations
  [start-idx aseq]
  (let [animations (remove #(empty? (:images %)) aseq)
        starts (into [start-idx] (map #(+ start-idx (* 6 (count (:images %)))) animations))]
    (map (fn [anim start] (build-animation anim start)) animations starts)))

(sm/defn extract-image-coords :- [Rect]
  "Given an map of paths to coordinates and an animation,
   return a seq of coordinates representing the images in that animation."
  [coord-index :- {} animation :- Animation]
  (map #(get coord-index (get-in % [:path :path])) (:images animation)))

(sm/defn index-coords-by-path
  "Given a list of coordinates, produce a map of path -> coordinates."
  [coords :- [Rect]]
  (zipmap (map #(get-in % [:path :path]) coords) coords))

(sm/defn get-animation-image-coords :- [[Rect]]
  "Given a set of Rect coordinates and a list of animations,
   return a list of lists of coordinates for the images
   in the animations."
  [coords :- [Rect] animations :- [Animation]]
  (let [idx (index-coords-by-path coords)]
    (map #(extract-image-coords idx %) animations)))

(defn texturesetc-protocol-buffer
  [texture-name textureset]
  #_(s/validate TextureSet textureset)
  (let [x-scale    (/ 1.0 (.getWidth (.packed-image textureset)))
        y-scale    (/ 1.0 (.getHeight (.packed-image textureset)))
        coords     (:coords textureset)
        bounds     (:aabb textureset)
        anims      (remove #(empty? (:images %)) (.animations textureset))
        acoords    (get-animation-image-coords coords anims)
        n-rects    (count coords)
        n-vertices (reduce + n-rects (map #(count (.images %)) anims))]
    (.build (doto (TextureSetProto$TextureSet/newBuilder)
            (.setTexture               texture-name)
            (.setTexCoords             (byte-pack (textureset->texcoords textureset)))
            (.addAllAnimations         (build-animations (* 6 n-rects) anims))

            (.addAllVertexStart        (vertex-starts n-vertices))
            (.addAllVertexCount        (vertex-counts n-vertices))
            (.setVertices              (byte-pack (textureset->vertices textureset)))

            (.addAllOutlineVertexStart (take n-vertices (take-nth 4 integers)))
            (.addAllOutlineVertexCount (take n-vertices (repeat (int 4))))
            (.setOutlineVertices       (byte-pack (textureset->outline-vertices textureset)))

            (.setTileCount             (int 0))))))

(defnk compile-texturesetc :- s/Bool
  [this g project textureset :- TextureSet]
  (write-native-file (:textureset-filename this)
                     (.toByteArray (texturesetc-protocol-buffer (:texture-name this) textureset))))

(defn- texturec-protocol-buffer
  [engine-format]
  (s/validate EngineFormatTexture engine-format)
  (.build (doto (Graphics$TextureImage/newBuilder)
            (.addAlternatives
              (doto (Graphics$TextureImage$Image/newBuilder)
                (.setWidth           (.width engine-format))
                (.setHeight          (.height engine-format))
                (.setOriginalWidth   (.original-width engine-format))
                (.setOriginalHeight  (.original-height engine-format))
                (.setFormat          (.format engine-format))
                (.setData            (byte-pack (.data engine-format)))
                (.addAllMipMapOffset (.mipmap-offsets engine-format))
                (.addAllMipMapSize   (.mipmap-sizes engine-format))))
            (.setType            (Graphics$TextureImage$Type/TYPE_2D))
            (.setCount           1))))

(defnk compile-texturec :- s/Bool
  [this g project textureset :- TextureSet]
  (write-native-file (:texture-filename this)
                     (.toByteArray (texturec-protocol-buffer (->engine-format (:packed-image textureset)))))
  :ok)

(defnode CompiledTextureSave
  (input textureset TextureSet)

  (property texture-filename {:schema s/Any :default ""})

  (output   texturec s/Any :on-update compile-texturec))

(defnode CompiledTextureSetSave
  (input textureset TextureSet)

  (property texture-name (string))
  (property textureset-filename {:schema s/Any :default ""})

  (output   texturesetc s/Any :on-update compile-texturesetc))

(defnode TextureSave
  (inherits CompiledTextureSave)
  (inherits CompiledTextureSetSave))

(defn on-load
  [project path ^AtlasProto$Atlas atlas-message]
  (let [atlas-tx (message->node atlas-message (constantly []) :filename path :_id -1)
        compiler (make-texture-save :texture-name        (clojure.string/replace (local-path (replace-extension path "texturesetc")) "content/" "")
                                    :textureset-filename (in-build-directory (replace-extension path "texturesetc"))
                                    :texture-filename    (in-build-directory (replace-extension path "texturec")))]
    (transact project
      [atlas-tx
       (new-resource compiler)
       (connect {:_id -1} :textureset compiler :textureset)])))

(logging-exceptions "Atlas tooling"
  (register-editor (e/current-project) "atlas" #'dynamic-scene-editor)
  (register-loader (e/current-project) "atlas" (protocol-buffer-loader AtlasProto$Atlas on-load)))
