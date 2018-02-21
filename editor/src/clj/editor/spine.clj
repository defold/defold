(ns editor.spine
  (:require [dynamo.graph :as g]
            [editor.graph-util :as gu]
            [editor.geom :as geom]
            [editor.math :as math]
            [editor.gl :as gl]
            [editor.gl.vertex :as vtx]
            [editor.defold-project :as project]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.render :as render]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
            [editor.validation :as validation]
            [editor.gl.pass :as pass]
            [editor.texture-unit :as texture-unit]
            [editor.types :as types]
            [editor.json :as json]
            [editor.outline :as outline]
            [editor.pipeline :as pipeline]
            [editor.properties :as properties]
            [editor.rig :as rig]
            [service.log :as log]
            [util.murmur :as murmur])
  (:import [com.dynamo.rig.proto Rig$RigScene]
           [com.dynamo.spine.proto Spine$SpineSceneDesc Spine$SpineModelDesc Spine$SpineModelDesc$BlendMode]
           [com.defold.editor.pipeline BezierUtil RigUtil$Transform TextureSetGenerator$UVTransform]
           [editor.types AABB]
           [com.jogamp.opengl GL GL2]
           [javax.vecmath Matrix4d Point2d Point3d Quat4d Vector3d Vector4d Tuple3d Tuple4d]
           [editor.gl.shader ShaderLifecycle]))

(set! *warn-on-reflection* true)

(def spine-scene-icon "icons/32/Icons_16-Spine-scene.png")
(def spine-model-icon "icons/32/Icons_15-Spine-model.png")
(def spine-bone-icon "icons/32/Icons_S_13_radiocircle.png")

(def spine-scene-ext "spinescene")
(def spine-model-ext "spinemodel")

; Node defs

(g/defnk produce-save-value [spine-json-resource atlas-resource sample-rate]
  {:spine-json (resource/resource->proj-path spine-json-resource)
   :atlas (resource/resource->proj-path atlas-resource)
   :sample-rate sample-rate})

(defprotocol Interpolator
  (interpolate [v0 v1 t]))

(extend-protocol Interpolator
  Double
  (interpolate [v0 v1 t]
    (+ v0 (* t (- v1 v0))))
  Long
  (interpolate [v0 v1 t]
    (+ v0 (* t (- v1 v0))))
  Point3d
  (interpolate [v0 v1 t]
    (doto (Point3d.) (.interpolate ^Tuple3d v0 ^Tuple3d v1 ^double t)))
  Vector3d
  (interpolate [v0 v1 t]
    (doto (Vector3d.) (.interpolate ^Tuple3d v0 ^Tuple3d v1 ^double t)))
  Vector4d
  (interpolate [v0 v1 t]
    (doto (Vector4d.) (.interpolate ^Tuple4d v0 ^Tuple4d v1 ^double t)))
  Quat4d
  (interpolate [v0 v1 t]
    (doto (Quat4d.) (.interpolate ^Quat4d v0 ^Quat4d v1 ^double t))))

(defn- ->interpolatable [pb-field v]
  (case pb-field
    :positions (doto (Point3d.) (math/clj->vecmath v))
    :rotations (doto (Quat4d. 0 0 0 1) (math/clj->vecmath v))
    :scale     (doto (Vector3d.) (math/clj->vecmath v))
    :colors    (doto (Vector4d.) (math/clj->vecmath v))
    :mix       v))

(defn- interpolatable-> [pb-field interpolatable]
  (case pb-field
    (:positions :rotations :scale :colors) (math/vecmath->clj interpolatable)
    :mix                                   interpolatable))

(def default-vals {:positions [0 0 0]
                   :rotations [0 0 0 1]
                   :scale [1 1 1]
                   :colors [1 1 1 1]
                   :attachment true
                   :order-offset 0
                   :mix 1.0
                   :positive true}) ;; spine docs say assume false, but implementations actually default to true

(defn- curve [[x0 y0 x1 y1] t]
  (let [t (BezierUtil/findT t 0.0 x0 x1 1.0)]
    (BezierUtil/curve t 0.0 y0 y1 1.0)))

(defn- angle->clj-quat [angle]
  (let [half-rad (/ (* 0.5 angle Math/PI) 180.0)
        c (Math/cos half-rad)
        s (Math/sin half-rad)]
    [0 0 s c]))

(defn- angle->quat [angle]
  (let [[x y z w] (angle->clj-quat angle)]
    (Quat4d. x y z w)))

(defn- hex-pair->color [^String hex-pair]
  (/ (Integer/parseInt hex-pair 16) 255.0))

(defn- hex->color [^String hex]
  (let [n (count hex)]
    (when (and (not= n 6) (not= n 8))
      (throw (ex-info (format "Invalid value for color: '%s'" hex) {:hex hex})))
    [(hex-pair->color (subs hex 0 2))
     (hex-pair->color (subs hex 2 4))
     (hex-pair->color (subs hex 4 6))
     (if (= n 8) (hex-pair->color (subs hex 6 8)) 1.0)]))

(defn- key->value [type key]
  (case type
    "translate"    [(get key "x" 0) (get key "y" 0) 0]
    "rotate"       (angle->clj-quat (get key "angle" 0))
    "scale"        [(get key "x" 1) (get key "y" 1) 1]
    "color"        (hex->color (get key "color" "FFFFFFFF"))
    "drawOrder"    (get key "offset")
    "mix"          (get key "mix")
    "bendPositive" (get key "bendPositive")))

(def timeline-type->pb-field {"translate" :positions
                              "rotate" :rotations
                              "scale" :scale
                              "color" :colors
                              "attachment" :visible
                              "drawOrder" :order-offset
                              "mix" :mix
                              "bendPositive" :positive})

(def timeline-type->vector-type {"translate" :double
                                 "rotate" :double
                                 "scale" :double
                                 "color" :double
                                 "attachment" :boolean
                                 "drawOrder" :long
                                 "mix" :double
                                 "bendPositive" :boolean})

(defn key->curve-data
  [key]
  ; NOTE:
  ; We've seen Spine scenes produced from non-conforming exporters that would
  ; write a null value in place of "stepped". We treat this the same way the
  ; old editor and the official Spine runtimes do - we consider the value to be
  ; unspecified and fall back on the default of "linear".
  (let [curve (get key "curve")]
    (cond
      (= "stepped" curve) nil
      (and (vector? curve) (= 4 (count curve))) curve
      :else [0 0 1 1])))

(defn- sample [type keys duration sample-rate spf val-fn default-val interpolate?]
  (let [pb-field (timeline-type->pb-field type)
        val-fn (or val-fn key->value)
        default-val (if (nil? default-val) (default-vals pb-field) default-val)
        sample-count (Math/ceil (+ 1 (* duration sample-rate)))
        ; Sort keys
        keys (vec (sort-by #(get % "time") keys))
        vals (mapv #(if-some [v (val-fn type %)] v default-val) keys)
        ; Add dummy key for 0
        [keys vals] (if (or (empty? keys) (> (get (first keys) "time") 0.0))
                      [(vec (cons {"time" 0.0
                                   "curve" "stepped"} keys))
                       (vec (cons default-val vals))]
                      [keys vals])
        ; Convert keys to vecmath vals
        vals (if interpolate?
               (mapv #(->interpolatable pb-field %) vals)
               vals)
        ; Accumulate key counts into sample slots
        key-counts (reduce (fn [counts key]
                             (let [sample (int (* (get key "time") sample-rate))]
                               (update-in counts [sample] inc))) (vec (repeat sample-count 0)) keys)
        ; LUT from sample to key index
        sample->key-idx (loop [key-counts key-counts
                               v (transient [])
                               offset 0]
                          (if-let [key-count (first key-counts)]
                            (recur (rest key-counts) (conj! v offset) (+ (int key-count) offset))
                            (persistent! v)))]
    (reduce (fn [ret sample]
              (let [cursor (* spf sample)
                    idx1 (get sample->key-idx sample)
                    idx0 (dec idx1)
                    k0 (get keys idx0)
                    v0 (get vals idx0)
                    k1 (get keys idx1)
                    v1 (get vals idx1)
                    v (if (and k0 (not interpolate?))
                        v0
                        (if (some? k1)
                          (if (>= cursor (get k1 "time"))
                            v1
                            (if-let [c (key->curve-data k0)]
                              (let [t (/ (- cursor (get k0 "time")) (- (get k1 "time") (get k0 "time")))
                                    rate (curve c t)]
                                (interpolate v0 v1 rate))
                              v0))
                          v0))
                    v (if interpolate?
                        (interpolatable-> pb-field v)
                        v)]
                (if (vector? v)
                  (reduce conj ret v)
                  (conj ret v))))
            (vector-of (timeline-type->vector-type type))
            (range sample-count))))

; This value is used to counter how the key values for rotations are interpreted in spine:
; * All values are modulated into the interval 0 <= x < 360
; * If keys k0 and k1 have a difference > 180, the second key is adjusted with +/- 360 to lessen the difference to < 180
; ** E.g. k0 = 0, k1 = 270 are interpolated as k0 = 0, k1 = -90
(defn- wrap-angles [type keys]
  (if (= type "rotate")
    (loop [prev-key nil
           keys keys
           wrapped []]
      (if-let [key (first keys)]
        (let [key (if prev-key
                    (let [angle (get key "angle" 0.0)
                          diff (double (- angle (get prev-key "angle" 0.0)))]
                      (if (> (Math/abs diff) 180.0)
                        (assoc key "angle" (+ angle (* (Math/signum diff) (- 360.0))))
                        key))
                    key)]
          (recur key (rest keys) (conj wrapped key)))
        wrapped))
    keys))

(defn- build-tracks [timelines duration sample-rate spf bone-id->index]
  (let [tracks-by-bone (reduce-kv (fn [m bone-name timeline]
                                    (let [bone-index (bone-id->index (murmur/hash64 bone-name))]
                                      (reduce-kv (fn [m type keys]
                                                   (if-let [field (timeline-type->pb-field type)]
                                                     (let [pb-track {field (sample type (wrap-angles type keys) duration sample-rate spf nil nil true)
                                                                     :bone-index bone-index}]
                                                       (update-in m [bone-index] merge pb-track))
                                                     m))
                                                 m timeline)))
                                  {} timelines)]
    (sort-by :bone-index (vals tracks-by-bone))))

(defn- build-mesh-tracks [slot-timelines do-timelines duration sample-rate spf slots-data slot->track-data]
  (let [; Reshape do-timelines into slot-timelines
        do-by-slot (into {} (map (fn [[slot timeline]]
                                   [slot {"drawOrder" timeline}])
                             (reduce (fn [m timeline]
                                      (let [t (get timeline "time")
                                            explicit (reduce (fn [m offset]
                                                               (assoc m (get offset "slot") [{"time" t "offset" (get offset "offset")}]))
                                                             {} (get timeline "offsets"))
                                            ; Supply implicit slots with 0 in offset
                                            all (reduce (fn [m slot]
                                                          (if (not (contains? m slot))
                                                            (assoc m slot [{"time" t "offset" 0}])
                                                            m))
                                                        explicit (keys m))]
                                        (merge-with into m all)))
                                    {} do-timelines)))
        slot-timelines (merge-with merge slot-timelines do-by-slot)
        tracks-by-slot (reduce-kv (fn [m slot timeline]
                                    (let [slot-data (get slots-data slot)
                                          tracks (mapv (fn [{:keys [skin-id mesh-index attachment]}]
                                                         (reduce-kv (fn [track type keys]
                                                                      (let [interpolate? (= type "color")
                                                                            val-fn (when (= type "attachment")
                                                                                     (fn [type key]
                                                                                       (= attachment (get key "name"))))
                                                                            default-val (when (= type "attachment")
                                                                                          (= (:attachment slot-data) attachment))
                                                                            field (timeline-type->pb-field type)
                                                                            pb-track {:mesh-index mesh-index
                                                                                      :mesh-id skin-id
                                                                                      field (sample type keys duration sample-rate spf val-fn default-val interpolate?)}]
                                                                        (merge track pb-track)))
                                                                    {} timeline))
                                                       (slot->track-data slot))]
                                      (assoc m slot tracks)))
                               {} slot-timelines)]
    (flatten (vals tracks-by-slot))))

(defn- build-event-tracks
  [events event-name->event-props]
  (mapv (fn [[event-name keys]]
          (let [default (merge {"int" 0 "float" 0 "string" ""} (get event-name->event-props event-name))]
            {:event-id (murmur/hash64 event-name)
             :keys (mapv (fn [k]
                           {:t (get k "time")
                            :integer (get k "int" (get default "int"))
                            :float (get k "float" (get default "float"))
                            :string (murmur/hash64 (get k "string" (get default "string")))})
                         keys)}))
        (reduce (fn [m e]
                  (update-in m [(get e "name")] conj e))
                {} events)))

(defn- build-ik-tracks
  [ik-timelines duration sample-rate spf ik-id->index]
  (->> ik-timelines
       (map (fn [[ik-name key-frames]]
              {:ik-index (ik-id->index (murmur/hash64 ik-name))
               :mix      (sample "mix" key-frames duration sample-rate spf nil nil true)
               :positive (sample "bendPositive" key-frames duration sample-rate spf nil nil false)}))
       (sort-by :ik-index)
       (vec)))

(defn- anim-duration
  [anim]
  (reduce max 0
          (concat (for [[bone-name timelines] (get anim "bones")
                        [timline-type key-frames] timelines
                        {:strs [time]} key-frames]
                    time)
                  (for [[slot-name timelines] (get anim "slots")
                        [timline-type key-frames] timelines
                        {:strs [time]} key-frames]
                    time)
                  (for [[ik-name key-frames] (get anim "ik")
                        {:strs [time]} key-frames]
                    time)
                  (for [{:strs [time]} (get anim "events")]
                    time)
                  (for [{:strs [time]} (get anim "drawOrder")]
                    time))))

(defn- bone->transform [bone]
  (let [t (RigUtil$Transform.)]
    (math/clj->vecmath (.position t) (:position bone))
    (math/clj->vecmath (.rotation t) (:rotation bone))
    (math/clj->vecmath (.scale t) (:scale bone))
    t))

(defn- normalize-weights [weights]
  (let [total-weight (reduce (fn [total w]
                               (+ total (:weight w)))
                             0 weights)]
    (mapv (fn [w] (update w :weight #(/ % total-weight))) weights)))

(def ^:private ^TextureSetGenerator$UVTransform uv-identity (TextureSetGenerator$UVTransform.))

(defn- attachment->mesh [attachment att-name slot-data anim-data bones-remap bone-index->world]
  (when anim-data
    (let [type (get attachment "type" "region")
          world ^RigUtil$Transform (:bone-world slot-data)
          anim-id (get attachment "name" att-name)
          uv-trans (or ^TextureSetGenerator$UVTransform (first (get-in anim-data [anim-id :uv-transforms])) uv-identity)
          mesh (case type
                 "region"
                 (let [local (doto (RigUtil$Transform.)
                               (-> (.position) (.set (get attachment "x" 0) (get attachment "y" 0) 0))
                               (-> (.rotation) (.set ^Quat4d (angle->quat (get attachment "rotation" 0))))
                               (-> (.scale) (.set (get attachment "scaleX" 1) (get attachment "scaleY" 1) 1)))
                       world (doto (RigUtil$Transform. world)
                               (.mul local))
                       width (get attachment "width" 0)
                       height (get attachment "height" 0)
                       vertices (flatten (for [x [-0.5 0.5]
                                               y [-0.5 0.5]
                                               :let [p (Point3d. (* x width) (* y height) 0)
                                                     uv (Point2d. (+ x 0.5) (- 0.5 y))]]
                                           (do
                                             (.apply world p)
                                             (.apply uv-trans uv)
                                             [(.x p) (.y p) (.z p) (.x uv) (.y uv)])))]
                   {:positions (flatten (partition 3 5 vertices))
                    :texcoord0 (flatten (partition 2 5 (drop 3 vertices)))
                    :indices [0 1 2 2 1 3]
                    :weights (take 16 (cycle [1 0 0 0]))
                    :bone-indices (take 16 (cycle [(:bone-index slot-data) 0 0 0]))
                    :skin-color (hex->color (get attachment "color" "ffffffff"))})
                 ("mesh" "skinnedmesh" "weightedmesh")
                 (let [vertices (get attachment "vertices" [])
                       uvs (get attachment "uvs" [])
                       ; A mesh is weighted if the number of vertices > number of UVs.
                       ; Both are x, y pairs if the mesh is not weighted.
                       weighted? (> (count vertices) (count uvs))
                       ; Use uvs because vertices have a dynamic format
                       vertex-count (/ (count uvs) 2)]
                   (if weighted?
                     (let [[positions
                            bone-indices
                            bone-weights] (loop [vertices vertices
                                                 positions []
                                                 bone-indices []
                                                 bone-weights []]
                                            (if-let [bone-count (first vertices)]
                                              (let [weights (take bone-count (map (fn [[bone-index x y weight]]
                                                                                    {:bone-index (bones-remap bone-index)
                                                                                     :x x
                                                                                     :y y
                                                                                     :weight weight})
                                                                                  (partition 4 (rest vertices))))
                                                    p ^Point3d (reduce (fn [^Point3d p w]
                                                                         (let [wp (Point3d. (:x w) (:y w) 0)
                                                                               world ^RigUtil$Transform (bone-index->world (:bone-index w))]
                                                                           (.apply world wp)
                                                                           (.scaleAdd wp ^double (:weight w) p)
                                                                           (.set p wp)
                                                                           p))
                                                                       (Point3d.) weights)
                                                    weights (normalize-weights (take 4 (sort-by #(- 1.0 (:weight %)) weights)))]
                                                (recur (drop (inc (* bone-count 4)) vertices)
                                                       (conj positions (.x p) (.y p) (.z p))
                                                       (into bone-indices (flatten (partition 4 4 (repeat 0) (mapv :bone-index weights))))
                                                       (into bone-weights (flatten (partition 4 4 (repeat 0) (mapv :weight weights))))))
                                              [positions bone-indices bone-weights]))]
                       {:positions positions
                        :texcoord0 (mapcat (fn [[u v]]
                                             (let [uv (Point2d. u v)]
                                               (.apply uv-trans uv)
                                               [(.x uv) (.y uv)]))
                                           (partition 2 uvs))
                        :indices (get attachment "triangles")
                        :weights bone-weights
                        :bone-indices bone-indices
                        :skin-color (hex->color (get attachment "color" "ffffffff"))})
                     (let [weight-count (* vertex-count 4)]
                       {:positions (mapcat (fn [[x y]]
                                             (let [p (Point3d. x y 0)]
                                               (.apply world p)
                                               [(.x p) (.y p) (.z p)]))
                                           (partition 2 vertices))
                        :texcoord0 (mapcat (fn [[u v]]
                                             (let [uv (Point2d. u v)]
                                               (.apply uv-trans uv)
                                               [(.x uv) (.y uv)]))
                                           (partition 2 uvs))
                        :indices (get attachment "triangles")
                        :weights (take weight-count (cycle [1 0 0 0]))
                        :bone-indices (take weight-count (cycle [(:bone-index slot-data) 0 0 0]))
                        :skin-color (hex->color (get attachment "color" "ffffffff"))})))
                 ; Ignore other types
                 nil)]
      (some-> mesh
              (assoc :color (:color slot-data)
                     :visible (= att-name (:attachment slot-data))
                     :draw-order (:draw-order slot-data))))))

(defn skin->meshes [skin slots-data anim-data bones-remap bone-index->world]
  (reduce-kv (fn [m slot attachments]
               (let [mesh-pairs (mapcat (fn [[att-name att]]
                                          (if-let [mesh (attachment->mesh att att-name (get slots-data slot) anim-data bones-remap bone-index->world)]
                                            [[slot att-name] mesh]
                                            []))
                                        attachments)]
                 (if (empty? mesh-pairs)
                   m
                   (apply assoc m mesh-pairs))))
             {} skin))

(defn- prop-resource-error [nil-severity _node-id prop-kw prop-value prop-name]
  (or (validation/prop-error nil-severity _node-id prop-kw validation/prop-nil? prop-value prop-name)
      (validation/prop-error :fatal _node-id prop-kw validation/prop-resource-not-exists? prop-value prop-name)))

(defn- validate-scene-atlas [_node-id atlas]
  (prop-resource-error :fatal _node-id :atlas atlas "Atlas"))

(defn- validate-scene-spine-json [_node-id spine-json]
  (prop-resource-error :fatal _node-id :spine-json spine-json "Spine Json"))

(g/defnk produce-scene-own-build-errors [_node-id atlas spine-json]
  (g/package-errors _node-id
                    (validate-scene-atlas _node-id atlas)
                    (validate-scene-spine-json _node-id spine-json)))

(g/defnk produce-scene-build-targets
  [_node-id own-build-errors resource spine-scene-pb atlas dep-build-targets texture-build-target]
  (g/precluding-errors own-build-errors
    (let [{:keys [animation-set mesh-set skeleton]} spine-scene-pb
          workspace (project/workspace (project/get-project _node-id))
          animation-set-build-target (rig/make-animation-set-build-target workspace _node-id animation-set)
          mesh-set-build-target (rig/make-mesh-set-build-target workspace _node-id mesh-set)
          skeleton-build-target (rig/make-skeleton-build-target workspace _node-id skeleton)
          dep-build-targets (into [animation-set-build-target mesh-set-build-target skeleton-build-target] (flatten dep-build-targets))]
      [(pipeline/make-pb-map-build-target _node-id resource dep-build-targets Rig$RigScene
                                          {:animation-set (:resource animation-set-build-target)
                                           :mesh-set (:resource mesh-set-build-target)
                                           :skeleton (:resource skeleton-build-target)
                                           :texture-set atlas
                                           :textures (some-> texture-build-target :resource vector)})])))

(defn- read-bones
  [spine-scene]
  (mapv (fn [b]
          {:id (murmur/hash64 (get b "name"))
           :name (get b "name")
           :parent (when (contains? b "parent") (murmur/hash64 (get b "parent")))
           :position [(get b "x" 0) (get b "y" 0) 0]
           :rotation (angle->clj-quat (get b "rotation" 0))
           :scale [(get b "scaleX" 1) (get b "scaleY" 1) 1]
           :inherit-scale (get b "inheritScale" true)
           :length (get b "length")})
        (get spine-scene "bones")))

(defn- read-iks
  [spine-scene bone-id->index]
  (mapv (fn [ik]
          (let [bones (get ik "bones")
                [parent child] (if (= 1 (count bones))
                                 [(first bones) (first bones)]
                                 bones)]
            {:id (murmur/hash64 (get ik "name" "unamed"))
             :parent (some-> parent murmur/hash64 bone-id->index)
             :child (some-> child murmur/hash64 bone-id->index)
             :target (some-> (get ik "target") murmur/hash64 bone-id->index)
             :positive (get ik "bendPositive" true)
             :mix (get ik "mix" 1.0)}))
        (get spine-scene "ik")))

(defn- read-slots
  [spine-scene bone-id->index bone-index->world-transform]
  (into {} (map-indexed (fn [i slot]
                          (let [bone-index (bone-id->index (murmur/hash64 (get slot "bone")))]
                            [(get slot "name")
                             {:bone-index bone-index
                              :bone-world (get bone-index->world-transform bone-index)
                              :draw-order i
                              :color (hex->color (get slot "color" "FFFFFFFF"))
                              :attachment (get slot "attachment")}]))
                        (get spine-scene "slots"))))

(defn- bone-world-transforms
  [bones]
  (loop [bones bones
         wt []]
    (if-let [bone (first bones)]
      (let [local-t ^RigUtil$Transform (bone->transform bone)
            world-t (if (not= 0xffff (:parent bone))
                      (let [world-t (doto (RigUtil$Transform. (get wt (:parent bone)))
                                      (.mul local-t))]
                        ;; Reset scale when not inheriting
                        (when (not (:inherit-scale bone))
                          (doto (.scale world-t)
                            (.set (.scale local-t))))
                        world-t)
                      local-t)]
        (recur (rest bones) (conj wt world-t)))
      wt)))

(g/defnk produce-spine-scene-pb [_node-id spine-json spine-scene anim-data sample-rate]
  (let [spf (/ 1.0 sample-rate)
        ;; Bone data
        bones (read-bones spine-scene)
        indexed-bone-children (reduce (fn [m [i b]] (update-in m [(:parent b)] conj [i b])) {} (map-indexed (fn [i b] [i b]) bones))
        root (first (get indexed-bone-children nil))
        ordered-bones (if root
                        (tree-seq (constantly true) (fn [[i b]] (get indexed-bone-children (:id b))) root)
                        [])
        bones-remap (into {} (map-indexed (fn [i [first-i b]] [first-i i]) ordered-bones))
        bones (mapv second ordered-bones)
        bone-id->index (into {} (map-indexed (fn [i b] [(:id b) i]) bones))
        bones (mapv #(assoc % :parent (get bone-id->index (:parent %) 0xffff)) bones)
        bone-index->world-transform (bone-world-transforms bones)

        ;; IK data
        iks (read-iks spine-scene bone-id->index)
        ik-id->index (zipmap (map :id iks) (range))

        ;; Slot data
        slots-data (read-slots spine-scene bone-id->index bone-index->world-transform)
        slot-count (count (get spine-scene "slots"))

        ;; Skin data
        mesh-sort-fn (fn [[k v]] (:draw-order v))
        sort-meshes (partial sort-by mesh-sort-fn)
        skins (get spine-scene "skins" {})
        generic-meshes (skin->meshes (get skins "default" {}) slots-data anim-data bones-remap bone-index->world-transform)
        new-skins {"" (sort-meshes generic-meshes)} ; "default" skin is called "" (hashed to 0) in build data
        new-skins (reduce (fn [m [skin slots]]
                            (let [specific-meshes (skin->meshes slots slots-data anim-data bones-remap bone-index->world-transform)]
                              (assoc m skin (sort-meshes (merge generic-meshes specific-meshes)))))
                          new-skins
                          (filter (fn [[skin _]] (not= "default" skin)) skins))
        slot->track-data (reduce-kv (fn [m skin meshes]
                                      (let [skin-id (murmur/hash64 skin)]
                                        (reduce-kv (fn [m i [[slot att]]]
                                                     (update m slot conj {:skin-id skin-id :mesh-index i :attachment att}))
                                                   m (vec meshes))))
                                    {} new-skins)

        ;; Protobuf
        pb (try
             {:skeleton {:bones bones
                         :iks iks}
              :animation-set (let [event-name->event-props (get spine-scene "events" {})
                                   animations (mapv (fn [[name a]]
                                                      (let [duration (anim-duration a)]
                                                        {:id (murmur/hash64 name)
                                                         :sample-rate sample-rate
                                                         :duration duration
                                                         :tracks (build-tracks (get a "bones") duration sample-rate spf bone-id->index)
                                                         :mesh-tracks (build-mesh-tracks (get a "slots") (get a "drawOrder") duration sample-rate spf slots-data slot->track-data)
                                                         :event-tracks (build-event-tracks (get a "events") event-name->event-props )
                                                         :ik-tracks (build-ik-tracks (get a "ik") duration sample-rate spf ik-id->index)}))
                                                    (get spine-scene "animations"))]
                               {:animations animations})
              :mesh-set {:slot-count slot-count
                         :mesh-entries (mapv (fn [[skin meshes]]
                                               {:id (murmur/hash64 skin)
                                                :meshes (mapv second meshes)})
                                             new-skins)}}
             (catch Exception e
               (log/error :exception e)
               (g/->error _node-id :spine-json :fatal spine-json (str "Incompatible data found in spine json " (resource/resource->proj-path spine-json)))))]
    pb))

(defn- transform-positions [^Matrix4d transform mesh]
  (let [p (Point3d.)]
    (update mesh :positions (fn [positions]
                              (->> positions
                                (partition 3)
                                (mapcat (fn [[x y z]]
                                          (.set p x y z)
                                          (.transform transform p)
                                          [(.x p) (.y p) (.z p)])))))))

(defn- renderable->meshes [renderable]
  (let [mesh-entries (get-in renderable [:user-data :spine-scene-pb :mesh-set :mesh-entries])
        skin-id (some-> renderable :user-data :skin murmur/hash64)
        mesh-entry (or (first (filter #(= (:id %) skin-id) mesh-entries))
                       (first mesh-entries))]
    (->> (:meshes mesh-entry)
         (filter :visible)
         (sort-by :draw-order)
         (map (partial transform-positions (:world-transform renderable)))
         (map (fn [mesh]
                (let [color (get-in renderable [:user-data :color] [1.0 1.0 1.0 1.0])]
                  (update mesh :color (fn [src tint] (mapv * src tint)) color)))))))

(defn- mesh->verts [mesh]
  (let [verts (mapv concat (partition 3 (:positions mesh)) (partition 2 (:texcoord0 mesh)) (repeat (:color mesh)))]
    (map (partial get verts) (:indices mesh))))

(defn gen-vb [renderables]
  (let [meshes (mapcat renderable->meshes renderables)
        vcount (reduce + 0 (map (comp count :indices) meshes))]
    (when (> vcount 0)
      (let [vb (render/->vtx-pos-tex-col vcount)
            verts (mapcat mesh->verts meshes)]
        (persistent! (reduce conj! vb verts))))))

(def color [1.0 1.0 1.0 1.0])

(defn- skeleton-vs [parent-pos bone vs ^Matrix4d wt]
  (let [pos (Vector3d.)
        t (doto (Matrix4d.)
            (.mul wt ^Matrix4d (:transform bone)))
        _ (.get ^Matrix4d t pos)
        pos [(.x pos) (.y pos) (.z pos)]
        vs (if parent-pos
             (conj vs (into parent-pos color) (into pos color))
             vs)]
    (reduce (fn [vs bone] (skeleton-vs pos bone vs wt)) vs (:children bone))))

(defn- gen-skeleton-vb [renderables]
  (let [vs (loop [renderables renderables
                  vs []]
             (if-let [r (first renderables)]
               (let [skeleton (get-in r [:user-data :scene-structure :skeleton])]
                 (recur (rest renderables) (skeleton-vs nil skeleton vs (:world-transform r))))
               vs))
        vcount (count vs)]
    (when (> vcount 0)
      (let [vb (render/->vtx-pos-col vcount)]
        (persistent! (reduce conj! vb vs))))))

(defn render-spine-scenes [^GL2 gl render-args renderables rcount]
  (let [pass (:pass render-args)]
    (cond
      (= pass pass/outline)
      (render/render-aabb-outline gl render-args ::spine-outline renderables rcount)

      (= pass pass/transparent)
      (do (when-let [vb (gen-vb renderables)]
            (let [user-data (:user-data (first renderables))
                  blend-mode (:blend-mode user-data)
                  gpu-textures (:gpu-textures user-data)
                  shader (get user-data :shader render/shader-tex-tint)
                  vertex-binding (vtx/use-with ::spine-trans vb shader)]
              (gl/with-gl-bindings gl render-args [shader vertex-binding]
                (texture-unit/bind-gpu-textures! gl gpu-textures shader render-args)
                (case blend-mode
                  :blend-mode-alpha (.glBlendFunc gl GL/GL_ONE GL/GL_ONE_MINUS_SRC_ALPHA)
                  (:blend-mode-add :blend-mode-add-alpha) (.glBlendFunc gl GL/GL_ONE GL/GL_ONE)
                  :blend-mode-mult (.glBlendFunc gl GL/GL_ZERO GL/GL_SRC_COLOR))
                (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (count vb))
                (.glBlendFunc gl GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA)
                (texture-unit/unbind-gpu-textures! gl gpu-textures shader render-args))))
          (when-let [vb (gen-skeleton-vb renderables)]
            (let [vertex-binding (vtx/use-with ::spine-skeleton vb render/shader-outline)]
              (gl/with-gl-bindings gl render-args [render/shader-outline vertex-binding]
                (gl/gl-draw-arrays gl GL/GL_LINES 0 (count vb))))))

      (= pass pass/selection)
      (when-let [vb (gen-vb renderables)]
        (let [vertex-binding (vtx/use-with ::spine-selection vb render/shader-tex-tint)]
          (gl/with-gl-bindings gl render-args [render/shader-tex-tint vertex-binding]
            (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (count vb))))))))

(g/defnk produce-scene [_node-id aabb gpu-textures spine-scene-pb scene-structure]
  (let [scene {:node-id _node-id
               :aabb aabb}]
    (if (and (seq gpu-textures) scene-structure)
      (let [blend-mode :blend-mode-alpha]
        (assoc scene :renderable {:render-fn render-spine-scenes
                                  :batch-key (texture-unit/batch-key-entry gpu-textures)
                                  :select-batch-key _node-id
                                  :user-data {:spine-scene-pb spine-scene-pb
                                              :scene-structure scene-structure
                                              :gpu-textures gpu-textures
                                              :blend-mode blend-mode}
                                  :passes [pass/transparent pass/selection pass/outline]}))
      scene)))

(defn- mesh->aabb [aabb mesh]
  (let [positions (partition 3 (:positions mesh))]
    (reduce (fn [aabb pos] (apply geom/aabb-incorporate aabb pos)) aabb positions)))

(g/defnode SpineSceneNode
  (inherits resource-node/ResourceNode)

  (property spine-json resource/Resource
            (value (gu/passthrough spine-json-resource))
            (set (fn [_evaluation-context self old-value new-value]
                   (project/resource-setter self old-value new-value
                                            [:resource :spine-json-resource]
                                            [:content :spine-scene]
                                            [:structure :scene-structure]
                                            [:node-outline :source-outline])))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext "json"}))
            (dynamic error (g/fnk [_node-id spine-json]
                             (validate-scene-spine-json _node-id spine-json))))

  (property atlas resource/Resource
            (value (gu/passthrough atlas-resource))
            (set (fn [_evaluation-context self old-value new-value]
                   (project/resource-setter self old-value new-value
                                            [:resource :atlas-resource]
                                            [:anim-data :anim-data]
                                            [:gpu-texture-generator :gpu-texture-generator]
                                            [:texture-build-target :texture-build-target]
                                            [:texture-build-target :dep-build-targets]
                                            [:build-targets :dep-build-targets])))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext "atlas"}))
            (dynamic error (g/fnk [_node-id atlas]
                             (validate-scene-atlas _node-id atlas))))

  (property sample-rate g/Num)

  (input spine-json-resource resource/Resource)
  (input atlas-resource resource/Resource)

  (input anim-data g/Any)
  (input default-tex-params g/Any)
  (input gpu-texture-generator g/Any)
  (input texture-build-target g/Any)
  (input dep-build-targets g/Any :array)
  (input spine-scene g/Any)
  (input scene-structure g/Any)

  (output save-value g/Any :cached produce-save-value)
  (output own-build-errors g/Any :cached produce-scene-own-build-errors)
  (output build-targets g/Any :cached produce-scene-build-targets)
  (output spine-scene-pb g/Any :cached produce-spine-scene-pb)
  (output scene g/Any :cached produce-scene)
  (output aabb AABB :cached (g/fnk [spine-scene-pb] (let [meshes (mapcat :meshes (get-in spine-scene-pb [:mesh-set :mesh-entries]))]
                                                      (reduce mesh->aabb (geom/null-aabb) meshes))))
  (output anim-data g/Any (gu/passthrough anim-data))
  (output scene-structure g/Any (gu/passthrough scene-structure))
  (output spine-anim-ids g/Any (g/fnk [spine-scene] (keys (get spine-scene "animations"))))
  (output texture-build-target g/Any (gu/passthrough texture-build-target))
  (output gpu-texture-generator g/Any (gu/passthrough gpu-texture-generator))
  (output gpu-textures g/Any :cached (g/fnk [default-tex-params gpu-texture-generator]
                                       ;; NOTE: This is only used when previewing a SpineScene in the Scene view.
                                       ;; We produce a full mapping using the relevant material samplers when used
                                       ;; with a SpineModel.
                                       (when (some? gpu-texture-generator)
                                         (texture-unit/gpu-textures-by-sampler-name default-tex-params [gpu-texture-generator] nil)))))

(defn load-spine-scene [project self resource spine]
  (let [spine-resource (workspace/resolve-resource resource (:spine-json spine))
        atlas (workspace/resolve-resource resource (:atlas spine))]
    (concat
      (g/connect project :default-tex-params self :default-tex-params)
      (g/set-property self
                      :spine-json spine-resource
                      :atlas atlas
                      :sample-rate (:sample-rate spine)))))

(g/defnk produce-model-save-value [spine-scene-resource default-animation skin material-resource blend-mode texture1-7-resources]
  (cond-> {:spine-scene (resource/resource->proj-path spine-scene-resource)
           :default-animation default-animation
           :skin skin
           :material (resource/resource->proj-path material-resource)
           :blend-mode blend-mode}

          (seq texture1-7-resources)
          (assoc :textures (into [""] (map resource/resource->proj-path) texture1-7-resources))))

(defn ->skin-choicebox [spine-skins]
  (properties/->choicebox (cons "" (remove (partial = "default") spine-skins))))

(defn validate-skin [node-id prop-kw spine-skins spine-skin]
  (when-not (empty? spine-skin)
    (validation/prop-error :fatal node-id prop-kw
                           (fn [skin skins]
                             (when-not (contains? skins skin)
                               (format "skin '%s' could not be found in the specified spine scene" skin)))
                           spine-skin
                           (disj (set spine-skins) "default"))))

(defn- validate-model-default-animation [node-id spine-scene spine-anim-ids default-animation]
  (when (and spine-scene (not-empty default-animation))
    (validation/prop-error :fatal node-id :default-animation
                           (fn [anim ids]
                             (when-not (contains? ids anim)
                               (format "animation '%s' could not be found in the specified spine scene" anim)))
                           default-animation
                           (set spine-anim-ids))))

(defn- validate-model-material [node-id material]
  (prop-resource-error :fatal node-id :material material "Material"))

(defn- validate-model-skin [node-id spine-scene scene-structure skin]
  (when spine-scene
    (validate-skin node-id :skin (:skins scene-structure) skin)))

(defn- validate-model-spine-scene [node-id spine-scene]
  (prop-resource-error :fatal node-id :spine-scene spine-scene "Spine Scene"))

(g/defnk produce-model-own-build-errors [_node-id default-animation material spine-anim-ids spine-scene scene-structure skin]
  (g/package-errors _node-id
                    (validate-model-material _node-id material)
                    (validate-model-spine-scene _node-id spine-scene)
                    (validate-model-skin _node-id spine-scene scene-structure skin)
                    (validate-model-default-animation _node-id spine-scene spine-anim-ids default-animation)))

(g/defnk produce-model-build-targets [_node-id own-build-errors resource save-value dep-build-targets texture-build-targets scene-structure]
  (g/precluding-errors own-build-errors
    [(pipeline/make-pb-map-build-target _node-id resource dep-build-targets Spine$SpineModelDesc
                                        (-> save-value
                                            (assoc :textures (mapv :resource texture-build-targets))
                                            (update :skin (fn [skin] (or skin "")))))]))

(g/defnode SpineModelNode
  (inherits resource-node/ResourceNode) ; NOTE: Cannot use texture-unit/TextureUnitBaseNode since we get texture 0 from the spine scene.

  (property spine-scene resource/Resource
            (value (gu/passthrough spine-scene-resource))
            (set (fn [_evaluation-context self old-value new-value]
                   (project/resource-setter self old-value new-value
                                            [:resource :spine-scene-resource]
                                            [:scene :spine-scene-scene]
                                            [:spine-anim-ids :spine-anim-ids]
                                            [:aabb :aabb]
                                            [:build-targets :dep-build-targets]
                                            [:node-outline :source-outline]
                                            [:anim-data :anim-data]
                                            [:scene-structure :scene-structure]
                                            [:atlas :texture0-resource]
                                            [:texture-build-target :dep-build-targets]
                                            [:texture-build-target :texture0-texture-build-target]
                                            [:gpu-texture-generator :texture0-gpu-texture-generator])))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext spine-scene-ext}))
            (dynamic error (g/fnk [_node-id spine-scene]
                             (validate-model-spine-scene _node-id spine-scene))))
  (property blend-mode g/Any (default :blend-mode-alpha)
            (dynamic tip (validation/blend-mode-tip blend-mode Spine$SpineModelDesc$BlendMode))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Spine$SpineModelDesc$BlendMode))))
  (property material resource/Resource
            (value (gu/passthrough material-resource))
            (set (fn [_evaluation-context self old-value new-value]
                   (project/resource-setter self old-value new-value
                                            [:resource :material-resource]
                                            [:shader :material-shader]
                                            [:samplers :material-samplers]
                                            [:build-targets :dep-build-targets])))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext "material"}))
            (dynamic error (g/fnk [_node-id material]
                             (validate-model-material _node-id material))))
  (property default-animation g/Str
            (dynamic error (g/fnk [_node-id spine-anim-ids default-animation spine-scene]
                             (validate-model-default-animation _node-id spine-scene spine-anim-ids default-animation)))
            (dynamic edit-type (g/fnk [spine-anim-ids] (properties/->choicebox spine-anim-ids))))
  (property skin g/Str
            (dynamic error (g/fnk [_node-id skin scene-structure spine-scene]
                             (validate-model-skin _node-id spine-scene scene-structure skin)))
            (dynamic edit-type (g/fnk [scene-structure] (->skin-choicebox (:skins scene-structure)))))

  (input dep-build-targets g/Any :array)
  (input spine-scene-resource resource/Resource)
  (input spine-scene-scene g/Any)
  (input scene-structure g/Any)
  (input spine-anim-ids g/Any)
  (input aabb AABB)
  (input material-resource resource/Resource)
  (input material-shader ShaderLifecycle)
  (input material-samplers g/Any)
  (input default-tex-params g/Any)
  (input anim-data g/Any)

  (output anim-ids g/Any :cached (g/fnk [anim-data] (vec (sort (keys anim-data)))))
  (output material-shader ShaderLifecycle (gu/passthrough material-shader))
  (output scene g/Any :cached (g/fnk [spine-scene-scene blend-mode gpu-textures material-shader skin]
                                (when (some? material-shader)
                                  (if (:renderable spine-scene-scene)
                                    (-> spine-scene-scene
                                        (assoc-in [:renderable :batch-key] [blend-mode material-shader (texture-unit/batch-key-entry gpu-textures) skin])
                                        (assoc-in [:renderable :user-data :blend-mode] blend-mode)
                                        (assoc-in [:renderable :user-data :shader] material-shader)
                                        (assoc-in [:renderable :user-data :gpu-textures] gpu-textures)
                                        (assoc-in [:renderable :user-data :skin] skin))
                                    spine-scene-scene))))
  (output save-value g/Any :cached produce-model-save-value)
  (output own-build-errors g/Any :cached produce-model-own-build-errors)
  (output build-targets g/Any :cached produce-model-build-targets)
  (output aabb AABB (gu/passthrough aabb))

  ;; Multiple textures
  (input texture0-resource resource/Resource)
  (input texture0-texture-build-target g/Any)
  (input texture0-gpu-texture-generator g/Any)
  (input texture1-7-resources resource/Resource :array)
  (input texture1-7-texture-build-targets g/Any :array)
  (input texture1-7-gpu-texture-generators g/Any :array)

  (property textures1-7 resource/ResourceVec
            (value (gu/passthrough texture1-7-resources))
            (set (fn [evaluation-context self _old-value new-value]
                   ;; TODO: Use old-value directly once DEFEDIT-1277 is fixed.
                   (let [old-value (g/node-value self :texture1-7-resources evaluation-context)]
                     (concat
                       (texture-unit/connect-resources self old-value new-value [[:texture-build-target :dep-build-targets]])
                       (texture-unit/connect-resources-preserving-index self old-value new-value [[:resource :texture1-7-resources]
                                                                                                  [:texture-build-target :texture1-7-texture-build-targets]
                                                                                                  [:gpu-texture-generator :texture1-7-gpu-texture-generators]])))))
            (dynamic visible (g/constantly false)))

  (output texture-build-targets g/Any (g/fnk [texture0-texture-build-target texture1-7-texture-build-targets] (into [texture0-texture-build-target] texture1-7-texture-build-targets)))
  (output gpu-texture-generators g/Any (g/fnk [texture0-gpu-texture-generator texture1-7-gpu-texture-generators] (into [texture0-gpu-texture-generator] texture1-7-gpu-texture-generators)))
  (output gpu-textures g/Any :cached (g/fnk [default-tex-params gpu-texture-generators material-samplers]
                                       (texture-unit/gpu-textures-by-sampler-name default-tex-params gpu-texture-generators material-samplers)))
  (output _properties g/Properties :cached (g/fnk [_node-id _declared-properties material-samplers texture0-resource texture1-7-resources]
                                             (if (empty? material-samplers)
                                               _declared-properties
                                               (texture-unit/augment-properties _declared-properties
                                                                                (concat
                                                                                  {:texture0 {:node-id _node-id
                                                                                              :type g/Str
                                                                                              :value (resource/resource->proj-path texture0-resource)
                                                                                              :label (:name (first material-samplers))
                                                                                              :read-only? true}}
                                                                                  (map (fn [prop-label array-index texture-resource]
                                                                                         [(texture-unit/unit-index->texture-prop-kw (inc array-index))
                                                                                          (texture-unit/texture-unit-property-info _node-id :textures1-7 prop-label array-index texture-resource)])
                                                                                       (map :name (drop 1 material-samplers))
                                                                                       (range 7)
                                                                                       (concat texture1-7-resources (repeat nil)))))))))

(defn load-spine-model [project self resource spine]
  (let [resolve-fn (partial workspace/resolve-resource resource)
        spine (-> spine
                  (update :spine-scene resolve-fn)
                  (update :material resolve-fn)
                  (dissoc :textures)
                  (assoc :textures1-7 (mapv resolve-fn (drop 1 (:textures spine)))))]
    (concat
      (g/connect project :default-tex-params self :default-tex-params)
      (for [[k v] spine]
        (g/set-property self k v)))))

(defn register-resource-types [workspace]
  (concat
    (resource-node/register-ddf-resource-type workspace
      :ext spine-scene-ext
      :build-ext "rigscenec"
      :label "Spine Scene"
      :node-type SpineSceneNode
      :ddf-type Spine$SpineSceneDesc
      :load-fn load-spine-scene
      :icon spine-scene-icon
      :view-types [:scene :text]
      :view-opts {:scene {:grid true}})
    (resource-node/register-ddf-resource-type workspace
      :ext spine-model-ext
      :label "Spine Model"
      :node-type SpineModelNode
      :ddf-type Spine$SpineModelDesc
      :load-fn load-spine-model
      :icon spine-model-icon
      :view-types [:scene :text]
      :view-opts {:scene {:grid true}}
      :tags #{:component}
      :tag-opts {:component {:transform-properties #{:position :rotation}}})))

(g/defnk produce-transform [position rotation scale]
  (math/->mat4-non-uniform (Vector3d. (double-array position))
                           (math/euler-z->quat rotation)
                           (Vector3d. (double-array scale))))

(g/defnode SpineBone
  (inherits outline/OutlineNode)
  (property name g/Str (dynamic read-only? (g/constantly true)))
  (property position types/Vec3
            (dynamic edit-type (g/constantly (properties/vec3->vec2 0.0)))
            (dynamic read-only? (g/constantly true)))
  (property rotation g/Num (dynamic read-only? (g/constantly true)))
  (property scale types/Vec3
            (dynamic edit-type (g/constantly (properties/vec3->vec2 1.0)))
            (dynamic read-only? (g/constantly true)))
  (property length g/Num
            (dynamic read-only? (g/constantly true)))

  (input child-bones g/Any :array)

  (output transform Matrix4d :cached produce-transform)
  (output bone g/Any (g/fnk [name transform child-bones]
                            {:name name
                             :local-transform transform
                             :children child-bones}))
  (output node-outline outline/OutlineData (g/fnk [_node-id name child-outlines]
                                                  {:node-id _node-id
                                                   :label name
                                                   :icon spine-bone-icon
                                                   :children child-outlines
                                                   :read-only true})))

(defn- update-transforms [^Matrix4d parent bone]
  (let [t ^Matrix4d (:local-transform bone)
        t (doto (Matrix4d.)
            (.mul parent t))]
    (-> bone
      (assoc :transform t)
      (assoc :children (mapv #(update-transforms t %) (:children bone))))))

(g/defnode SpineSceneJson
  (inherits outline/OutlineNode)
  (input source-outline outline/OutlineData)
  (output node-outline outline/OutlineData (g/fnk [source-outline] source-outline))
  (input skeleton g/Any)
  (input content g/Any)
  (output structure g/Any :cached (g/fnk [skeleton content]
                                         {:skeleton (update-transforms (math/->mat4) skeleton)
                                          :skins (vec (sort (keys (get content "skins"))))})))

(defn accept-spine-scene-json [content]
  (when (or (get-in content ["skeleton" "spine"])
            (and (get content "bones") (get content "animations")))
    content))

(defn- tx-first-created [tx-data]
  (get-in (first tx-data) [:node :_node-id]))

(defn load-spine-scene-json [node-id content]
  (let [bones (get content "bones")
        graph (g/node-id->graph-id node-id)
        scene-tx-data (g/make-nodes graph [scene SpineSceneJson]
                                    (g/connect scene :_node-id node-id :nodes)
                                    (g/connect scene :node-outline node-id :child-outlines)
                                    (g/connect scene :structure node-id :structure)
                                    (g/connect node-id :content scene :content))
        scene-id (tx-first-created scene-tx-data)]
    (concat
      scene-tx-data
      (loop [bones bones
             tx-data []
             bone-ids {}]
        (if-let [bone (first bones)]
          (let [name (get bone "name")
                parent (get bone "parent")
                x (get bone "x" 0)
                y (get bone "y" 0)
                rotation (get bone "rotation" 0)
                scale-x (get bone "scaleX" 1.0)
                scale-y (get bone "scaleY" 1.0)
                length (get bone "length")
                bone-tx-data (g/make-nodes graph [bone [SpineBone :name name :position [x y 0] :rotation rotation :scale [scale-x scale-y 1.0] :length length]]
                                           (g/connect bone :_node-id node-id :nodes)
                                           (if-let [parent (get bone-ids parent)]
                                             (concat
                                               (g/connect bone :node-outline parent :child-outlines)
                                               (g/connect bone :bone parent :child-bones))
                                             (concat
                                               (g/connect bone :node-outline scene-id :source-outline)
                                               (g/connect bone :bone scene-id :skeleton))))
                bone-id (tx-first-created bone-tx-data)]
            (recur (rest bones) (conj tx-data bone-tx-data) (assoc bone-ids name bone-id)))
          tx-data)))))

(json/register-json-loader ::spine-scene accept-spine-scene-json load-spine-scene-json)
