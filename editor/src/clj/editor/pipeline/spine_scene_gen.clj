(ns editor.pipeline.spine-scene-gen
  (:require [editor.math :as math])
  (:import [com.defold.editor.pipeline SpineSceneUtil RigUtil$UVTransformProvider RigUtil$Bone RigUtil$Transform RigUtil$Mesh
            RigUtil$Slot RigUtil$Animation RigUtil$AnimationTrack RigUtil$AnimationKey RigUtil$SlotAnimationTrack
            RigUtil$SlotAnimationKey RigUtil$EventKey RigUtil$EventTrack RigUtil$AnimationTrack$Property
            RigUtil$SlotAnimationTrack$Property RigUtil$Event
            TextureSetGenerator$UVTransform]
           [javax.vecmath Point2d Point3d Quat4d Vector2d Vector3d Matrix4d]
           [java.lang Math]))

(set! *warn-on-reflection* true)

(defn- x-bone [^RigUtil$Bone j-bone]
  {:name (.name j-bone)
   :position (math/vecmath->clj (.position (.localT j-bone)))
   :rotation (math/vecmath->clj (.rotation (.localT j-bone)))
   :scale (math/vecmath->clj (.scale (.localT j-bone)))
   :index (.index j-bone)
   :inherit-scale (.inheritScale j-bone)})

(defn- x-slot [^RigUtil$Slot j-slot]
  {:name (.name j-slot)
   :index (.index j-slot)
   :attachment (.attachment j-slot)
   :color (vec (.color j-slot))})

(defn- x-mesh [^RigUtil$Mesh j-mesh]
  {:attachment (.attachment j-mesh)
   :path (.path j-mesh)
   :visible (.visible j-mesh)
   :vertices (.vertices j-mesh)
   :triangles (.triangles j-mesh)
   :bone-weights (.boneWeights j-mesh)
   :bone-indices (.boneIndices j-mesh)})

(defn- angle->quat [angle]
  (let [half-rad (/ (* 0.5 angle Math/PI) 180.0)
        c (Math/cos half-rad)
        s (Math/sin half-rad)]
    (math/vecmath->clj (Quat4d. 0 0 s c))))

(defn- x-track-key [^RigUtil$AnimationKey j-key]
  {:t (.t j-key)
   :value (vec (.value j-key))
   :stepped (.stepped j-key)
   :curve (when-let [c (.curve j-key)] {:x0 (.x0 c) :y0 (.y0 c) :x1 (.x1 c) :y1 (.y1 c)})})

(defn- x-track-prop [j-prop]
  (cond
    (= j-prop RigUtil$AnimationTrack$Property/POSITION) :position
    (= j-prop RigUtil$AnimationTrack$Property/ROTATION) :rotation
    (= j-prop RigUtil$AnimationTrack$Property/SCALE) :scale))

(defn- x-track [^RigUtil$AnimationTrack j-track bones]
  (let [property (x-track-prop (.property j-track))
        keys (mapv x-track-key (.keys j-track))
        keys (if (= property :rotation)
               (mapv #(update-in % [:value] (fn [v] (angle->quat (get v 0)))) keys)
               keys)]
    {:bone (when-let [b (.bone j-track)] (get bones (.index b)))
     :property property
     :keys keys}))

(defn- x-slot-key [^RigUtil$SlotAnimationKey j-key]
  (assoc (x-track-key j-key)
         :attachment (.attachment j-key)
         :order-offset (.orderOffset j-key)))

(defn- x-slot-prop [j-prop]
  (cond
    (= j-prop RigUtil$SlotAnimationTrack$Property/ATTACHMENT) :attachment
    (= j-prop RigUtil$SlotAnimationTrack$Property/COLOR) :color
    (= j-prop RigUtil$SlotAnimationTrack$Property/DRAW_ORDER) :draw-order))

(defn- x-slot-track [^RigUtil$SlotAnimationTrack j-track slots-by-name]
  {:slot (when-let [s (.slot j-track)] (get slots-by-name (.name s)))
   :property (x-slot-prop (.property j-track))
   :keys (mapv x-slot-key (.keys j-track))})

(defn- x-event-key [^RigUtil$EventKey key]
  {:t (.t key)
   :string (.stringPayload key)
   :float (.floatPayload key)
   :int (.intPayload key)})

(defn- x-event-track [^RigUtil$EventTrack j-track]
  {:name (.name j-track)
   :keys (mapv x-event-key (.keys j-track))})

(defn- x-anim [^RigUtil$Animation j-anim bones slots-by-name]
  {:name (.name j-anim)
   :duration (.duration j-anim)
   :tracks (mapv #(x-track % bones) (.tracks j-anim))
   :slot-tracks (mapv #(x-slot-track % slots-by-name) (.slotTracks j-anim))
   :event-tracks (mapv x-event-track (.eventTracks j-anim))})

(defn- x-event [^RigUtil$Event j-event]
  {:name (.name j-event)
   :string (.stringPayload j-event)
   :float (.floatPayload j-event)
   :int (.intPayload j-event)})

(defn- x-scene [^SpineSceneUtil j-scene]
  (let [bones (mapv x-bone (.bones j-scene))
        bones (mapv (fn [^RigUtil$Bone j-bone bone] (assoc bone :parent (when-let [p (.parent j-bone)] (get bones (.index (.parent j-bone)))))) (.bones j-scene) bones)
        j-slots (.values (.slots j-scene))
        slots (mapv x-slot j-slots)
        slots (mapv (fn [^RigUtil$Slot j-slot slot] (assoc slot :bone (when-let [b (.bone j-slot)] (get bones (.index b))))) j-slots slots)
        slots-by-name (into {} (map #(do [(:name %) %]) slots))
        meshes (mapv x-mesh (.meshes j-scene))
        meshes (mapv (fn [^RigUtil$Mesh j-mesh mesh] (assoc mesh :slot (when-let [s (.slot j-mesh)] (get slots-by-name (.name s))))) (.meshes j-scene) meshes)
        skins-by-name (into {} (map (fn [[name j-meshes]] [name (mapv x-mesh j-meshes)]) (.skins j-scene)))
        anims-by-name (into {} (map (fn [[name j-anim]] [name (x-anim j-anim bones slots-by-name)]) (.animations j-scene)))
        events-by-name (into {} (map (fn [[name j-event]] [name (x-event j-event)]) (.events j-scene)))]
    {:bones bones
     :bones-by-name (into {} (map #(do [(:name %) %]) bones))
     :meshes meshes
     :slots-by-name slots-by-name
     :skins-by-name skins-by-name
     :anims-by-name anims-by-name}))

(defn read-scene [input anim-data]
  (let [uv-transform (reify RigUtil$UVTransformProvider
                       (getUVTransform [this anim-id]
                         (when-let [anim (get anim-data anim-id)]
                           (let [tc (flatten (:tex-coords (first (:frames anim))))]
                             (TextureSetGenerator$UVTransform. (Point2d. (nth tc 0) (nth tc 3))
                                                               (Vector2d. (- (nth tc 4) (nth tc 0)) (- (nth tc 7) (nth tc 3)))
                                                               (= (nth tc 0) (nth tc 6)))))))
        j-scene (SpineSceneUtil/loadJson input uv-transform)
        scene (x-scene j-scene)]
    scene))
