(ns atlas.core
  (:require [dynamo.ui :refer :all]
            [clojure.set :refer [union]]
            [plumbing.core :refer [fnk defnk]]
            [schema.core :as s]
            [schema.macros :as sm]
            [dynamo.buffers :refer :all]
            [dynamo.env :as e]
            [dynamo.geom :refer [to-short-uv]]
            [dynamo.node :refer :all]
            [dynamo.scene-editor :refer [dynamic-scene-editor]]
            [dynamo.file :refer :all]
            [dynamo.file.protobuf :refer [protocol-buffer-converters pb->str]]
            [dynamo.project :as p :refer [register-loader register-editor transact new-resource connect query]]
            [dynamo.types :refer :all]
            [dynamo.texture :refer :all]
            [dynamo.image :refer :all]
            [dynamo.outline :refer :all]
            [service.log :as log :refer [logging-exceptions]]
            [camel-snake-kebab :refer :all])
  (:import  [com.dynamo.atlas.proto AtlasProto AtlasProto$Atlas AtlasProto$AtlasAnimation AtlasProto$AtlasImage]
            [com.dynamo.graphics.proto Graphics$TextureImage Graphics$TextureImage$Image]
            [com.dynamo.textureset.proto TextureSetProto$Constants TextureSetProto$TextureSet TextureSetProto$TextureSetAnimation]
            [java.nio ByteBuffer]
            [dynamo.types Animation Image TextureSet Rect EngineFormatTexture]))

;; consider moving -> protocol-buffer-related place
(defmacro set-if-present
  ([inst k props]
    `(set-if-present ~inst ~k ~props identity))
  ([inst k props xform]
    (let [setter (symbol (->camelCase (str "set-" (name k))))]
      `(when-let [value# (get ~props ~k)]
         (. ~inst ~setter (~xform value#))))))

(defnode ImageNode
 OutlineNode
 ImageSource)

(defnk produce-animation :- Animation
  [this images :- [Image]]
  (->Animation (:id this) images (:fps this) (:flip-horizontal this) (:flip-vertical this) (:playback this)))

(defnode AnimationGroupNode
  OutlineNode
  AnimationBehavior
  {:inputs     {:images     [ImageSource]}
   :transforms {:animation  #'produce-animation}})

(sm/defn ^:private consolidate :- [Image]
  [images :- [Image] containers :- [{:images [Image]}]]
  (seq (apply union
              (into #{} images)
              (map #(into #{} (:images %)) containers))))

(defnk produce-textureset :- TextureSet
  [this images :- [Image] animations :- [Animation]]
  (-> (pack-textures (:margin this) (:extrude-border this) (consolidate images animations))
    (assoc :animations animations)))

(def AtlasProperties
  {:inputs     {:assets     [OutlineItem]
                :images     [ImageSource]
                :animations [Animation]}
   :properties {:margin (non-negative-integer)
                :extrude-borders (non-negative-integer)
                :filename (string)}
   :transforms {:textureset #'produce-textureset}
   :cached     #{:textureset}
   :on-update  #{:textureset}})


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
  [this project]
  (let [text (p/get-resource-value project this :text-format)]
    (write-native-text-file (:filename this) text)
    :ok))

(def AtlasSave
  {:transforms {:save        #'save-atlas-file
                :text-format #'get-text-format}})

(defnode AtlasNode
  OutlineNode
  AtlasProperties
  {:transforms {:save        #'save-atlas-file
                :text-format #'get-text-format}})

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

(sm/defn ^:private put-floats! :- ByteBuffer
  [buf :- ByteBuffer fs :- [s/Num]]
  (doseq [f fs] (.putFloat buf f))
  buf)

(def ^:private triangle-vertices-per-placement 6)
(def ^:private outline-vertices-per-placement 4)
(def ^:private vertex-size (.getNumber TextureSetProto$Constants/VERTEX_SIZE))

(def ^:private texcoords-per-placement 2)
(def ^:private floats-per-texcoord 2)
(def ^:private bytes-per-float 4)

(sm/defn ^:private  placement->tex-coords :- [s/Num]
  [xs :- s/Num ys :- s/Num p :- Rect]
  [(* (.x p) xs)
   (* (.y p) ys)
   (* (+ (.x p) (.width p)) xs)
   (* (+ (.y p) (.height p)) ys)])

(sm/defn ^:private tex-coords :- ByteBuffer
  "Return a new byte-buffer with the texture coordinates packed into it."
  [xs :- s/Num ys :- s/Num bounds :- Rect placements :- [Rect]]
  (->> placements
    (mapcat (partial placement->tex-coords xs ys))
    (put-floats! (new-byte-buffer (count placements) bytes-per-float floats-per-texcoord texcoords-per-placement))
    (.rewind)))

(defmacro put-vertex [buf x y z u v]
  `(do
     (.putFloat ~buf ~x)
     (.putFloat ~buf ~y)
     (.putFloat ~buf ~z)
     (.putShort ~buf ~u)
     (.putShort ~buf ~v)))

(sm/defn ^:private ->placement :- {}
  [r :- Rect]
  {:x0 (.x r)
   :y0 (.y r)
   :x1 (+ (.x r) (.width r))
   :y1 (+ (.y r) (.height r))
   :w2 (* (.width r) 0.5)
   :h2 (* (.height r) 0.5)})

(sm/defn put-vertices :- ByteBuffer
  [buf :- ByteBuffer xs :- s/Num ys :- s/Num ps :- [Rect]]
  (doseq [p ps]
    (let [{:keys [x0 y0 x1 y1 w2 h2]} (->placement p)]
      (put-vertex buf (- w2) (- h2) 0 (to-short-uv (* x0 xs)) (to-short-uv (* y1 ys)))
      (put-vertex buf    w2  (- h2) 0 (to-short-uv (* x1 xs)) (to-short-uv (* y1 ys)))
      (put-vertex buf    w2     h2  0 (to-short-uv (* x1 xs)) (to-short-uv (* y0 ys)))

      (put-vertex buf (- w2) (- h2) 0 (to-short-uv (* x0 xs)) (to-short-uv (* y1 ys)))
      (put-vertex buf    w2     h2  0 (to-short-uv (* x1 xs)) (to-short-uv (* y0 ys)))
      (put-vertex buf (- w2)    h2  0 (to-short-uv (* x0 xs)) (to-short-uv (* y0 ys)))))
  buf)

(sm/defn ^:private tex-vertices :- ByteBuffer
  "Return a new byte-buffer with the texture vertices packed into it.
   Each vertex is packed as:
     x :- Float
     y :- Float
     z :- Float
     u :- Short
     v :- Short"
  ([xs :- s/Num ys :- s/Num bounds :- Rect placements :- [Rect]]
    (let [buf (new-byte-buffer (count placements) triangle-vertices-per-placement vertex-size)]
      (put-vertices buf xs ys placements)))
  ([xs :- s/Num ys :- s/Num bounds :- Rect placements :- [Rect] x-placements :- [[Rect]]]
    (let [pcount (reduce + (count placements ) (map count x-placements))
          buf   (new-byte-buffer pcount triangle-vertices-per-placement vertex-size)
          ps     (into placements (apply concat x-placements))]
      (put-vertices buf xs ys ps))))

(sm/defn ^:private put-outline-vertices :- ByteBuffer
  [buf :- ByteBuffer xs :- s/Num ys :- s/Num ps :- [Rect]]
  (doseq [p ps]
    (let [{:keys [x0 y0 x1 y1 w2 h2]} (->placement p)]
      (put-vertex buf (- w2) (- h2) 0 (to-short-uv (* x0 xs)) (to-short-uv (* y1 ys)))
      (put-vertex buf    w2  (- h2) 0 (to-short-uv (* x1 xs)) (to-short-uv (* y1 ys)))
      (put-vertex buf    w2     h2  0 (to-short-uv (* x1 xs)) (to-short-uv (* y0 ys)))
      (put-vertex buf (- w2)    h2  0 (to-short-uv (* x0 xs)) (to-short-uv (* y0 ys)))))
  buf)

(sm/defn ^:private tex-outline-vertices :- ByteBuffer
  "Return a new byte-buffer with the texture vertices packed into it.
   Each vertex is packed as:
     x :- Float
     y :- Float
     z :- Float
     u :- Short
     v :- Short"
  ([xs :- s/Num ys :- s/Num bounds :- Rect placements :- [Rect]]
    (let [buf (new-byte-buffer (count placements) outline-vertices-per-placement vertex-size)]
      (put-outline-vertices buf xs ys placements)))
  ([xs :- s/Num ys :- s/Num bounds :- Rect placements :- [Rect] x-placements :- [[Rect]]]
    (let [pcount (reduce + (count placements) (map count x-placements))
          buf   (new-byte-buffer pcount triangle-vertices-per-placement vertex-size)
          ps     (into placements (apply concat x-placements))]
      (put-outline-vertices buf xs ys ps))))

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
        n-vertices (reduce + n-rects (map #(count (.images %)) anims))
        integers   (iterate (comp int inc) (int 0))]

    (.build (doto (TextureSetProto$TextureSet/newBuilder)
            (.setTexture               texture-name)
            (.setTexCoords             (byte-pack    (tex-coords x-scale y-scale bounds coords)))
            (.addAllAnimations         (build-animations (* 6 n-rects) anims))

            (.addAllVertexStart        (take n-vertices (take-nth 6 integers)))
            (.addAllVertexCount        (take n-vertices (repeat (int 6))))
            (.setVertices              (byte-pack (tex-vertices x-scale y-scale bounds coords acoords)))

            (.addAllOutlineVertexStart (take n-vertices (take-nth 4 integers)))
            (.addAllOutlineVertexCount (take n-vertices (repeat (int 4))))
            (.setOutlineVertices       (byte-pack (tex-outline-vertices x-scale y-scale bounds coords acoords)))

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
                (.addAllMipMapSize   (.mipmap-sizes engine-format)))))))

(defnk compile-texturec :- s/Bool
  [this g project textureset :- TextureSet]
  (write-native-file (:texture-filename this)
                     (.toByteArray (texturec-protocol-buffer (->engine-format (:packed-image textureset)))))
  :ok)

(def CompiledTextureSave
  {:inputs     {:textureset       TextureSet}
   :properties {:texture-filename {:schema s/Any :default ""}}
   :transforms {:texturec         #'compile-texturec}
   :on-update  #{:texturec}})

(def CompiledTextureSetSave
  {:inputs     {:textureset          TextureSet}
   :properties {:texture-name        (string)
                :textureset-filename {:schema s/Any :default ""}}
   :transforms {:texturesetc         #'compile-texturesetc}
   :on-update  #{:texturesetc}})

(defnode TextureSave
  CompiledTextureSave
  CompiledTextureSetSave)

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

(logging-exceptions
  (register-loader (e/current-project) "atlas" (protocol-buffer-loader AtlasProto$Atlas on-load))
  (register-editor (e/current-project) "atlas" #'dynamic-scene-editor))
