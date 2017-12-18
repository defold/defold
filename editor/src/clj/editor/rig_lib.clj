(ns editor.rig-lib
  (:require
   [dynamo.graph :as g]
   [editor.math :as math]
   [editor.protobuf :as protobuf]
   [editor.gl :as gl]
   [editor.gl.vertex2 :as vtx]
   [editor.gl.shader :as shader]
   [editor.scene-cache :as scene-cache]
   [util.murmur :as murmur])
  (:import
   (com.dynamo.rig.proto Rig$RigScene Rig$Skeleton Rig$AnimationSet Rig$MeshSet)
   (com.defold.libs RigLibrary RigLibrary$Result RigLibrary$NewContextParams RigLibrary$Matrix4 RigLibrary$Vector4 RigLibrary$RigVertexFormat RigLibrary$RigPlayback)
   (com.sun.jna Memory Pointer)
   (com.sun.jna.ptr IntByReference)
   (com.jogamp.common.nio Buffers)
   (com.jogamp.opengl GL GL2)
   (editor.gl.vertex2 VertexBuffer)
   (java.nio ByteBuffer)
   (javax.vecmath Point3d Quat4d Vector3d Matrix4d)
   (com.google.protobuf Message)))

(set! *warn-on-reflection* true)

;;------------------------------------------------------------------------------
;; librig api

;; vertex formats

(vtx/defvertex spine-vertex-format
  (vec3 position)
  (vec2 texcoord0)
  (vec4.ubyte color true))

(vtx/defvertex model-vertex-format
  (vec3 position)
  (vec2 texcoord0)
  (vec3 normal))


(defn- create-context!
  [max-rig-instance-count]
  (let [params (RigLibrary$NewContextParams. (int max-rig-instance-count))
        ret (RigLibrary/Rig_NewContext params)]
    (if (= ret RigLibrary$Result/RESULT_OK)
      (.getValue (.context params))
      (throw (Exception. "Rig_NewContext")))))

(defn- delete-context!
  [context]
  (RigLibrary/Rig_DeleteContext context))

(defn- update-context!
  [context dt]
  (let [ret (RigLibrary/Rig_Update context dt)]
    (if (= ret RigLibrary$Result/RESULT_OK)
      context
      (throw (Exception. "Rig_Update")))))

(defn- create-instance!
  [context ^ByteBuffer skeleton ^ByteBuffer mesh-set ^ByteBuffer animation-set mesh-id animation-id]
  (let [instance (RigLibrary/Rig_InstanceCreate context
                                                skeleton (.capacity skeleton)
                                                mesh-set (.capacity mesh-set)
                                                animation-set (.capacity animation-set)
                                                mesh-id
                                                (or animation-id 0))]
    (if (= instance Pointer/NULL)
      (throw (Exception. "Rig_InstanceCreate returned null"))
      instance)))

(defn- destroy-instance!
  [context instance]
  (if (RigLibrary/Rig_InstanceDestroy context instance)
    nil
    (throw (Exception. "Rig_InstanceDestroy"))))

(defn- identity-matrix
  []
  (let [m (RigLibrary$Matrix4.)]
    (set! (.m00 m) 1.0)
    (set! (.m11 m) 1.0)
    (set! (.m22 m) 1.0)
    (set! (.m33 m) 1.0)
    m))

(defn- vector4
  [v]
  (RigLibrary$Vector4. ^float v))

(defn- get-vertex-count
  [instance]
  (RigLibrary/Rig_GetVertexCount instance))

(defn- generate-vertex-data!
  [context instance model normal color format vertex-size ^ByteBuffer buf]
  (RigLibrary/Rig_GenerateVertexData context instance model normal color format buf)
  ;; JNA call will write data to buffer, need to update the position manually
  (.position buf (+ (.position buf) (* vertex-size (get-vertex-count instance))))
  buf)

(defn- get-pose-matrices
  ^RigLibrary$Matrix4 [context instance]
  (let [ms (RigLibrary/Rig_GetPoseMatrices context instance)]
    ms))

(defn- release-pose-matrices
  [ms]
  (RigLibrary/Rig_ReleasePoseMatrices ms)
  nil)

(defn- play-animation!
  [instance animation-id playback blend-duration offset playback-rate]
  #_(prn :play-animation! instance animation-id)
  (let [ret (RigLibrary/Rig_PlayAnimation instance animation-id playback blend-duration offset playback-rate)]
    (when-not (= ret RigLibrary$Result/RESULT_OK)
      (throw (Exception. "Rig_PlayAnimation")))))


;;------------------------------------------------------------------------------
;; rig player

(defn- proto-map->buf
  [cls m]
  (ByteBuffer/wrap (protobuf/map->bytes cls m)))

(defn make-rig-player
  [{:keys [skeleton animation-set mesh-set] :as rig-scene} mesh-id animation-id]
  (assert (or (nil? animation-id) (number? animation-id)))
  #_(assert (number? mesh-id))
  (let [skeleton-buf (proto-map->buf Rig$Skeleton skeleton)
        mesh-set-buf (proto-map->buf Rig$MeshSet mesh-set)
        animation-set-buf (proto-map->buf Rig$AnimationSet animation-set)
        mesh-id (-> mesh-set :mesh-entries first :id)
        context (create-context! 1)
        instance (create-instance! context skeleton-buf mesh-set-buf animation-set-buf mesh-id animation-id)]
    (when animation-id
      (play-animation! instance animation-id RigLibrary$RigPlayback/PLAYBACK_LOOP_FORWARD 1.0 0.0 1.0))
    {:context context
     :instance instance
     :bone-lengths (into [] (map :length) (:bones skeleton))}))

(defn update-rig-player
  [rig-player dt]
  (update-context! (:context rig-player) dt)
  rig-player)

(defn vertex-count
  [rig-player]
  (get-vertex-count (:instance rig-player)))

(defn vertex-data!
  [rig-player ^Matrix4d world-transform vertex-format ^VertexBuffer vbuf]
  (let [{:keys [context instance]} rig-player
        model (RigLibrary$Matrix4/fromMatrix4d world-transform)
        normal (identity-matrix)
        color (vector4 1.0)
        format (case vertex-format
                 :spine RigLibrary$RigVertexFormat/RIG_VERTEX_FORMAT_SPINE
                 :model RigLibrary$RigVertexFormat/RIG_VERTEX_FORMAT_MODEL)
        vertex-size (case vertex-format
                      :spine (:size spine-vertex-format)
                      :model (:size model-vertex-format))]
    (generate-vertex-data! context instance model normal color format vertex-size (.buf vbuf))))

(defn destroy-rig-player
  [rig-player]
  (let [{:keys [context instance]} rig-player]
    (destroy-instance! context instance)
    (delete-context! context)
    nil))


;;------------------------------------------------------------------------------
;; rendering

;; scene cached rig-player

(defn- make-rig-player*
  [ctx {:keys [rig-scene mesh-id animation-id]}]
  (prn :make-rig-player)
  (-> (make-rig-player rig-scene mesh-id animation-id)
      (update-rig-player 0.0)))

(defn- update-rig-player*
  [ctx rig-player {:keys [rig-scene mesh-id animation-id] :as params}]
  (prn :update-rig-player rig-player)
  (destroy-rig-player rig-player)
  (make-rig-player* ctx params))

(defn- destroy-rig-players*
  [ctx rig-players _]
  (prn :destroy-rig-players )
  (doseq [rig-player rig-players]
    (prn :destroy-rig-player rig-player)
    (destroy-rig-player rig-player)))

(defn- valid-rig-player?
  [{old-rig-scene    :rig-scene
    old-mesh-id      :mesh-id
    old-animation-id :animation-id}
   {new-rig-scene    :rig-scene
    new-mesh-id      :mesh-id
    new-animation-id :animation-id}]
  (and (identical? old-rig-scene
                   new-rig-scene)
       (= old-mesh-id
          new-mesh-id)
       (= old-animation-id
          new-animation-id)))

(scene-cache/register-object-cache! ::rig-player make-rig-player* update-rig-player* destroy-rig-players* valid-rig-player?)

(defn request-rig-player
  [view-id node-id rig-scene animation-name]
  (scene-cache/request-object! ::rig-player [node-id] view-id
                               (cond-> {:rig-scene rig-scene}
                                 animation-name
                                 (assoc :animation-id (murmur/hash64 animation-name)))))




(defn make-updatable
  [node-id rig-scene default-animation]
  {:node-id node-id
   :name ""
   :update-fn (fn [state {:keys [view-id node-id dt]}]
                (let [rig-player (request-rig-player view-id node-id rig-scene default-animation)]
                  (update-rig-player rig-player (double dt))))
   :initial-state {}})


;; bone rendering

(vtx/defvertex bone-vertex-format
  (vec3 position))

(shader/defshader bone-vertex-shader
  (attribute vec4 position)
  (uniform mat4 view_proj)
  (uniform mat4 pose_matrix)
  (defn void main []
    (setq gl_Position (* (* view_proj pose_matrix) position))))

(shader/defshader bone-fragment-shader
  (defn void main []
    (setq gl_FragColor (vec4 0.0 1.0 0.0 1.0))))

(def bone-shader (shader/make-shader ::bone-shader bone-vertex-shader bone-fragment-shader))

(defn make-octahedron-vertices
  [scale length]
  [[length 0 0]
   [0 (- scale) 0]
   [(- scale) 0 0]
   [0 scale 0]
   [0 0 scale]
   [0 0 (- scale)]])

(def octahedron-faces
  [[1 0 4]
   [2 1 4]
   [3 2 4]
   [0 3 4]
   [0 1 5]
   [1 2 5]
   [2 3 5]
   [3 0 5]])

(def octahedron-vertex-count (* (count octahedron-faces) 3))

(defn- make-bone-mesh
  [bone-length]
  (let [vbuf (vtx/make-vertex-buffer bone-vertex-format :dynamic (* 1 octahedron-vertex-count))
        vertices (make-octahedron-vertices 5.0 bone-length)
        put-face! (fn [vbuf face-index]
                   (let [[x y z] (nth vertices face-index)]
                     (bone-vertex-format-put! vbuf x y z)))]
    (run! (fn [[v1 v2 v3]]
          (-> vbuf
              (put-face! v1)
              (put-face! v2)
              (put-face! v3)))
          octahedron-faces)
    vbuf))

(defmethod shader/set-uniform-at-index RigLibrary$Matrix4
  [^GL2 gl progn loc ^RigLibrary$Matrix4 val]
  (.glUniformMatrix4fv gl loc 1 false
                       (float-array [(.m00 val) (.m10 val) (.m20 val) (.m30 val)
                                     (.m01 val) (.m11 val) (.m21 val) (.m31 val)
                                     (.m02 val) (.m12 val) (.m22 val) (.m32 val)
                                     (.m03 val) (.m13 val) (.m23 val) (.m33 val)]) 0))

(defn draw-bones!
  [^GL2 gl render-args rig-player]
  (let [{:keys [context instance bone-lengths]} rig-player
        bone-count (count bone-lengths)
        ms (get-pose-matrices context instance)
        msa (.toArray ms (int bone-count))]
    (try
      (doseq [i (range bone-count)]
        (let [m (aget msa i)
              bone-length (or (bone-lengths i) 5.0)
              vbuf (make-bone-mesh bone-length)]
          (vtx/flip! vbuf)
          (let [vertex-binding (vtx/use-with ::bone vbuf bone-shader)]
            (gl/with-gl-bindings gl render-args [bone-shader vertex-binding]
              (.glPolygonMode gl GL2/GL_FRONT_AND_BACK GL2/GL_LINE)
              (shader/set-uniform bone-shader gl "view_proj" (:view-proj render-args))
              (shader/set-uniform bone-shader gl "pose_matrix" m)
              (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (count vbuf))
              (.glPolygonMode gl GL2/GL_FRONT_AND_BACK GL2/GL_FILL)))))
      (finally
        (when ms (release-pose-matrices (.getPointer ms)))))))



(comment

  (def spine-pb (dynamo.graph/node-value (ffirst (user/selection)) :spine-scene-pb))

  (-> spine-pb :skeleton :bones count)

  (map :length (-> spine-pb :skeleton :bones ))

  (def rp  (make-rig-player spine-pb (util.murmur/hash64 "walk")))

  (update-rig-player rp 0.0)

  (vertex-data! rp (doto (Matrix4d.) (.setIdentity)))


  (def pm  (get-pose-matrices (:context rp) (:instance rp)))

  (def pma (.toArray pm 17))

  (count pma)

  (map println (seq pma))

  (into [] (map :length) (-> spine-pb :skeleton :bones))


  ;; bone-vertex-data

  - get matrices
  - create octahedron vertices
    - transform by pose matrix

  #_(release-pose-matrices (.getPointer pm))



  (keys spine-pb)

  (-> user/spine-pb :mesh-set :mesh-entries count)

  (def c (create-context! 1))

  (update-context! c 0.016)


  (delete-context! c)

  )
