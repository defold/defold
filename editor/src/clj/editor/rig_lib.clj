(ns editor.rig-lib
  (:require
   [dynamo.graph :as g]
   [editor.math :as math]
   [editor.protobuf :as protobuf]
   [editor.gl.vertex :as vertex]
   [editor.gl.shader :as shader])
  (:import
   (com.dynamo.rig.proto Rig$RigScene Rig$Skeleton Rig$AnimationSet Rig$MeshSet)
   (com.defold.libs RigLibrary RigLibrary$Result RigLibrary$NewContextParams RigLibrary$Matrix4 RigLibrary$Vector4 RigLibrary$RigVertexFormat)
   (com.sun.jna Memory Pointer)
   (com.sun.jna.ptr IntByReference)
   (com.jogamp.common.nio Buffers)
   (java.nio ByteBuffer)
   (javax.vecmath Point3d Quat4d Vector3d Matrix4d)
   (com.google.protobuf Message)))

(set! *warn-on-reflection* true)

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
  [context ^ByteBuffer skeleton ^ByteBuffer mesh-set ^ByteBuffer animation-set mesh-id default-animation]
  (let [instance (RigLibrary/Rig_InstanceCreate context
                                                skeleton (.capacity skeleton)
                                                mesh-set (.capacity mesh-set)
                                                animation-set (.capacity animation-set)
                                                mesh-id
                                                default-animation)]
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
  [n]
  (let [v (RigLibrary$Vector4.)]
    (set! (.x v) n)
    (set! (.y v) n)
    (set! (.z v) n)
    (set! (.w v) n)
    v))

(defn- vertex-count
  [instance]
  (RigLibrary/Rig_GetVertexCount instance))

(defn- generate-vertex-data!
  [context instance model normal color format]
  (let [vcount (vertex-count instance)
        buf (Buffers/newDirectByteBuffer (int (* vcount (* 6 4))))]
    (RigLibrary/Rig_GenerateVertexData context instance
                                       model normal color format
                                       buf)
    buf))




(defn- proto-map->buf
  [cls m]
  (ByteBuffer/wrap (protobuf/map->bytes cls m)))

(defn make-rig-player
  [{:keys [skeleton animation-set mesh-set] :as rig-scene}]
  (let [skeleton-buf (proto-map->buf Rig$Skeleton skeleton)
        mesh-set-buf (proto-map->buf Rig$MeshSet mesh-set)
        animation-set-buf (proto-map->buf Rig$AnimationSet animation-set)
        mesh-id (-> mesh-set :mesh-entries first :id)
        default-animation (-> animation-set :animations first :id)
        context (create-context! 1)
        instance (create-instance! context skeleton-buf mesh-set-buf animation-set-buf mesh-id default-animation)]
    {:context context
     :instance instance}))

(defn update-rig-player
  [rig-player dt]
  (update-context! (:context rig-player) dt))

(defn vertex-data!
  [rig-player]
  (let [{:keys [context instance]} rig-player
        model (identity-matrix)
        normal (identity-matrix)
        color (vector4 1.0)
        format RigLibrary$RigVertexFormat/RIG_VERTEX_FORMAT_SPINE]
    (generate-vertex-data! context instance model normal color format)))

(defn destroy-rig-player
  [rig-player]
  (let [{:keys [context instance]} rig-player]
    (destroy-instance! context instance)
    (delete-context! context)))

(comment

  (def ri  (make-rig-player spine-pb))

  (update-rig-player ri 0.1667)

  (def v (vertex-data! ri))

  v

  
  (def spine-pb (dynamo.graph/node-value (ffirst (user/selection)) :spine-scene-pb))

  (keys spine-pb)
  
  (-> user/spine-pb :mesh-set :mesh-entries count)
  
  (def c (create-context! 1))

  (update-context! c 0.016)

  
  (delete-context! c)

  )
