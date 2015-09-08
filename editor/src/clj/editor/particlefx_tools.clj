(ns editor.particlefx-tools
  (:require [dynamo.graph :as g]
            [editor.colors :as colors]
            [editor.math :as math]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [editor.project :as project]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.scene :as scene]
            [editor.scene-layout :as layout]
            [editor.geom :as geom]
            [internal.render.pass :as pass]
            [editor.particle-lib :as plib]
            [editor.ui :as ui]
            [editor.gl.vertex :as vertex]
            [editor.camera :as camera])
  (:import [javax.vecmath Matrix4d Point3d Vector4f Quat4d]
           [com.dynamo.particle.proto Particle$ParticleFX Particle$PlayMode Particle$EmitterType Particle$BlendMode
            Particle$EmissionSpace Particle$BlendMode Particle$ParticleOrientation Particle$ModifierType]
           [javax.media.opengl GL GL2 GL2GL3 GLContext GLProfile GLAutoDrawable GLOffscreenAutoDrawable GLDrawableFactory GLCapabilities]
           [editor.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [com.defold.libs ParticleLibrary]
           [com.sun.jna Pointer]
           [java.nio ByteBuffer IntBuffer]
           [com.google.protobuf ByteString]))

(set! *warn-on-reflection* true)

(def ^:private buttons {:play {:action (fn [controller]
                                         (g/transact
                                           (g/update-property controller :play-mode
                                                              (fn [play-mode]
                                                                (if (= play-mode :idle)
                                                                  :playing
                                                                  :idle)))))}})

(defn handle-selection-input [self action user-data]
  (let [manip (first (get user-data self))
        hot-manip (g/node-value self :hot-manip)]
    (case (:type action)
        #_:mouse-pressed #_(let [op-seq (gensym)
                                toggle (reduce #(or %1 %2) (map #(% action) toggle-modifiers))
                                mode (if toggle :toggle :direct)]
                            (g/transact
                              (concat
                                (g/set-property self :op-seq op-seq)
                                (g/set-property self :start cursor-pos)
                                (g/set-property self :current cursor-pos)
                                (g/set-property self :mode mode)
                                (g/set-property self :prev-selection-set (set (g/node-value self :selection)))))
                            (select self op-seq mode)
                            nil)
        :mouse-released (when (and (contains? buttons manip) (= manip hot-manip))
                          ((get-in buttons [manip :action]) self))
        :mouse-moved (g/transact
                       (g/set-property self :hot-manip manip))
        action))
  #_(let [start      (g/node-value self :start)
         current    (g/node-value self :current)
         op-seq     (g/node-value self :op-seq)
         mode       (g/node-value self :mode)
         cursor-pos [(:x action) (:y action) 0]]
     (case (:type action)
       :mouse-pressed (let [op-seq (gensym)
                            toggle (reduce #(or %1 %2) (map #(% action) toggle-modifiers))
                            mode (if toggle :toggle :direct)]
                        (g/transact
                          (concat
                            (g/set-property self :op-seq op-seq)
                            (g/set-property self :start cursor-pos)
                            (g/set-property self :current cursor-pos)
                            (g/set-property self :mode mode)
                            (g/set-property self :prev-selection-set (set (g/node-value self :selection)))))
                        (select self op-seq mode)
                        nil)
       :mouse-released (do
                         (g/transact
                           (concat
                             (g/set-property self :start nil)
                             (g/set-property self :current nil)
                             (g/set-property self :op-seq nil)
                             (g/set-property self :mode nil)
                             (g/set-property self :prev-selection-set nil)))
                         nil)
       :mouse-moved (if start
                      (do
                        (g/transact (g/set-property self :current cursor-pos))
                        (select self op-seq mode)
                        nil)
                      action)
       action))
  action)

(def ^:private timeline-margin 20)
(def ^:private timeline-height 32)
(def ^:private button-width 32)
(def ^:private layout-desc {:anchors {:bottom 0}
                            :padding 20
                            :max-height 72
                            :children [{:id :play
                                        :max-width 32}
                                       {:id :timeline}]})

(defn- layout-controls [^Region viewport]
  (layout/layout layout-desc viewport))

(defn- render-region [^GL2 gl ^Region region c]
  (let [min-x (:left region)
        min-y (:top region)
        max-x (:right region)
        max-y (:bottom region)
        z 0.0]
    (.glColor3d gl (nth c 0) (nth c 1) (nth c 2))
    (.glBegin gl GL2/GL_LINE_LOOP)
    (.glVertex3d gl min-x min-y z)
    (.glVertex3d gl min-x max-y z)
    (.glVertex3d gl max-x max-y z)
    (.glVertex3d gl max-x min-y z)
    (.glEnd gl)

    (.glBegin gl GL2/GL_QUADS)
    (.glColor4d gl (nth c 0) (nth c 1) (nth c 2) 0.2)
    (.glVertex3d gl min-x, min-y, z);
    (.glVertex3d gl min-x, max-y, z);
    (.glVertex3d gl max-x, max-y, z);
    (.glVertex3d gl max-x, min-y, z);
    (.glEnd gl)))

(defn- render-overlay [^GL2 gl render-args renderables count]
  (let [user-data (:user-data (first renderables))]
    (when-let [pfx-sim (:pfx-sim user-data)]
      (let [stats (plib/stats pfx-sim)]
        (scene/overlay-text gl (format "Particles: %d/%d" (:particles stats) (:max-particles stats)) 12.0 -22.0)
        (scene/overlay-text gl (format "Time: %.2f" (:time stats)) 12.0 -42.0)))))

(defn- render-controller [^GL2 gl render-args renderables count]
  (let [user-data (:user-data (first renderables))
        region (:region user-data)
        c (double-array (if (:hot user-data)
                          colors/defold-blue
                          colors/selected-blue))]
    (render-region gl region c)))

(defn- render-pfx [^GL2 gl render-args renderables count]
  (let [user-data (:user-data (first renderables))
        vtx-binding (:vtx-binding user-data)
        pfx-render-data (:pfx-render-data user-data)
        camera (:camera render-args)
        view-proj (doto (Matrix4d.)
                    (.mul (camera/camera-projection-matrix camera) (camera/camera-view-matrix camera)))]
    (doseq [[gpu-texture blend-mode v-index v-count] pfx-render-data]
      (gl/with-gl-bindings gl [gpu-texture plib/shader vtx-binding]
       (case blend-mode
         :blend-mode-alpha (.glBlendFunc gl GL/GL_ONE GL/GL_ONE_MINUS_SRC_ALPHA)
         (:blend-mode-add :blend-mode-add-alpha) (.glBlendFunc gl GL/GL_ONE GL/GL_ONE)
         :blend-mode-mult (.glBlendFunc gl GL/GL_ZERO GL/GL_SRC_COLOR))
       (shader/set-uniform plib/shader gl "texture" 0)
       (shader/set-uniform plib/shader gl "view_proj" view-proj)
       (shader/set-uniform plib/shader gl "tint" (Vector4f. 1 1 1 1))
       (gl/gl-draw-arrays gl GL/GL_TRIANGLES v-index v-count)
       (.glBlendFunc gl GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA)))))

(defn- convert-blend-mode [blend-mode-index]
  (protobuf/pb-enum->val (.getValueDescriptor (Particle$BlendMode/valueOf ^int blend-mode-index))))

(g/defnk produce-controller-renderables [_node-id active-pfx hot-manip viewport pfx-sim]
  (let [controller-renderables (if active-pfx
                                 (let [layout (layout-controls viewport)]
                                   (mapv (fn [[id region]]
                                           {:node-id _node-id
                                            :world-transform (Matrix4d. geom/Identity4d)
                                            :selection-data id
                                            :render-fn render-controller
                                            :user-data {:manip id :hot (= hot-manip id) :region region}})
                                         layout))
                                 [])]
    (-> (into {} (map (fn [pass] [pass controller-renderables])
                 [pass/overlay pass/overlay-selection]))
      (update pass/overlay conj {:node-id _node-id
                                 :world-transform (Matrix4d. geom/Identity4d)
                                 :render-fn render-overlay
                                 :user-data {:pfx-sim pfx-sim}}))))

; TODO - un-hardcode
(def ^:private max-emitter-count 32)
(def ^:private max-particle-count 256)

(defn- make-sim [prototype-msg instance-transforms]
  ; TODO - counts from game.project
  (plib/make-sim max-emitter-count max-particle-count prototype-msg instance-transforms))

(g/defnode ParticleFXSim
  (property pfx-sim-ref g/Any)

  (input prototype-msg g/Any)
  (input emitter-sim-data g/Any)
  (input instance-transforms [Matrix4d] :substitute [(doto (Matrix4d.) (.setIdentity))])

  (output fetch-anim-fn Runnable :cached (g/fnk [emitter-sim-data] (fn [index] (get emitter-sim-data index))))
  (output pfx-sim g/Any :cached (g/fnk [pfx-sim-ref prototype-msg instance-transforms]
                                       (when (not-empty instance-transforms)
                                         (let [pfx-sim (if @pfx-sim-ref
                                                         (plib/reload @pfx-sim-ref prototype-msg)
                                                         (let [pfx-sim (make-sim prototype-msg instance-transforms)]
                                                           (reset! pfx-sim-ref pfx-sim)
                                                           pfx-sim))]
                                           pfx-sim))))
  (output update-handler Runnable :cached (g/fnk [_node-id pfx-sim fetch-anim-fn]
                                                 (fn [dt]
                                                   (when pfx-sim
                                                     (plib/simulate pfx-sim dt fetch-anim-fn)
                                                     (g/invalidate! [[_node-id :pfx-renderables]])))))
  (output pfx-renderables pass/RenderData :cached
          (g/fnk [_node-id pfx-sim emitter-sim-data]
                 (let [pfx-render-data (atom [])]
                   (plib/render pfx-sim
                                (fn [emitter-index blend-mode v-index v-count]
                                  (let [gpu-texture (:gpu-texture (get emitter-sim-data emitter-index))]
                                    (swap! pfx-render-data conj [gpu-texture (convert-blend-mode blend-mode) v-index v-count]))))
                   (let [raw-vbuf ^ByteBuffer (:raw-vbuf pfx-sim)]
                     (if (and pfx-sim raw-vbuf (> (.limit raw-vbuf) 0))
                       {pass/transparent [{:node-id _node-id
                                           :world-transform (Matrix4d. geom/Identity4d)
                                           :render-fn render-pfx
                                           :user-data {:vtx-binding (:vtx-binding pfx-sim)
                                                       :pfx-render-data @pfx-render-data}}]}
                       {})))))

 g/IDisposable
 (g/dispose [self]
            (when-let [pfx-sim @(:pfx-sim-ref self)]
              (plib/destroy-sim pfx-sim))))

(defn- make-pfx-sim-node [controller pfx-id]
  (if pfx-id
    (g/make-nodes (g/node-id->graph-id controller)
                 [pfx-sim [ParticleFXSim :pfx-sim-ref (atom nil)]]
                 (g/connect pfx-id :rt-pb-data pfx-sim :prototype-msg)
                 (g/connect pfx-id :emitter-sim-data pfx-sim :emitter-sim-data)
                 (g/connect pfx-sim :pfx-sim controller :pfx-sim)
                 (g/connect pfx-sim :_node-id controller :pfx-sim-node)
                 (g/connect pfx-sim :update-handler controller :update-handler)
                 (g/connect pfx-sim :pfx-renderables controller :pfx-renderables)
                 (g/connect controller :instance-transforms pfx-sim :instance-transforms))
    []))

(defn- identity []
  (doto (Matrix4d.) (.setIdentity)))

(g/defnode ParticleFXController
  (property hot-manip g/Keyword)
  (property current-active-pfx g/NodeID)
  (property play-mode g/Keyword)

  (input pfx-sim-node g/NodeID)
  (input pfx-sim g/Any)
  (input pfx-renderables pass/RenderData)
  (input viewport Region)
  (input selection g/Any)
  (input selected-renderables g/Any)
  (input duration g/Num)
  (input update-handler Runnable :substitute (constantly (fn [_])))
  (input scene g/Any)

  (output active-pfx g/NodeID (g/fnk [_node-id current-active-pfx selected-renderables pfx-sim-node]
                                     (let [pfx-id (first
                                                    (filter some?
                                                            (map (fn [r] (get-in r [:user-data :pfx-id]))
                                                                 selected-renderables)))]
                                       (when (not= pfx-id current-active-pfx)
                                         (g/transact
                                           (concat
                                             (if pfx-sim-node
                                               (g/delete-node pfx-sim-node)
                                               [])
                                             (g/set-property _node-id :current-active-pfx pfx-id)
                                             (make-pfx-sim-node _node-id pfx-id))))
                                       pfx-id)))
  (output controller-renderables pass/RenderData :cached produce-controller-renderables)
  (output renderables pass/RenderData :cached
          (g/fnk [controller-renderables pfx-renderables]
                 (merge controller-renderables pfx-renderables)))
  (output input-handler Runnable :cached (g/always handle-selection-input))
  (output update-handler Runnable :cached (g/fnk [update-handler play-mode]
                                                 (fn [dt] (let [dt (if (= play-mode :playing) dt 0.0)]
                                                            (update-handler dt)))))
  #_(output instance-transforms [Matrix4d] :cached
           (g/fnk [scene current-active-pfx]
                  (if current-active-pfx
                    (if (= current-active-pfx (:node-id scene))
                      [(doto (Matrix4d.) (.setIdentity))]
                      (let [xfn (fn [item] (or (:transform item) (doto (Matrix4d.) (.setIdentity))))
                            instance-ids (mapv (fn [[_ _ in-node _]] in-node) (filter (fn [[_ out-label in-node in-label]] (= out-label :scene)) (g/outputs current-active-pfx)))
                            instance-set (set instance-ids)
                            #_match-fn #_(fn [item] (not (empty? (filter #(= (:node-id %) current-active-pfx) (:children item)))))
                            #_instances #_(filter match-fn (tree-seq (constantly true) :children scene))]
                        (loop [items [scene]
                               xforms []
                               world-transforms {}]
                          (let [new-xforms (mapv (fn [item] [(:node-id item) (conj xforms (xfn item))]) items)
                                to-add (filter (fn [[id xfors]] (contains? instance-set id)) new-xforms)]
                            (if (empty? to-add)
                              (conj world-transforms [node-id new-xforms]))))
      (prn "scene" (keys (first (:children scene))))
      (mapv #(g/node-value % :transform) instance-ids)))
                    []))))

#_(scene/defcontroller :pfx-player
   (make [view renderer]
         (let [view-graph  (g/node-id->graph-id view)]
           (g/make-nodes view-graph
                   [pfx [ParticleFXController :play-mode :idle]]

                   (g/connect pfx      :renderables        renderer :tool-renderables)
                   (g/connect pfx      :input-handler     view     :input-handlers)
                   (g/connect pfx      :update-handler    view     :update-handlers)
                   (g/connect renderer :selected-renderables pfx   :selected-renderables)
                   (g/connect view     :selection         pfx      :selection)
                   (g/connect view     :viewport          pfx      :viewport)
                   (g/connect view     :scene             pfx      :scene)))))
