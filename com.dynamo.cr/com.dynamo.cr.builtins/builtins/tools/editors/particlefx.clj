(ns editors.particlefx
  (:require [dynamo.ui :refer :all]
            [clojure.set :refer [union]]
            [plumbing.core :refer [fnk defnk]]
            [schema.core :as s]
            [schema.macros :as sm]
            [dynamo.buffers :refer :all]
            [dynamo.editors :as ed]
            [dynamo.env :as e]
            [dynamo.geom :as g :refer [to-short-uv]]
            [dynamo.gl :as gl :refer :all]
            [dynamo.gl.shader :as shader]
            [dynamo.gl.texture :as texture]
            [dynamo.gl.protocols :as glp]
            [dynamo.gl.vertex :as vtx]
            [dynamo.node :as n :refer :all]
            [dynamo.system :as ds :refer [transactional in add connect set-property]]
            [internal.ui.scene-editor :refer :all]
            [internal.ui.background :refer :all]
            [internal.ui.grid :refer :all]
            [dynamo.camera :refer :all]
            [dynamo.file :refer :all]
            [dynamo.file.protobuf :refer [protocol-buffer-converters pb->str]]
            [dynamo.project :as p :refer [register-loader register-editor]]
            [dynamo.types :refer :all]
            [dynamo.texture :refer :all]
            [dynamo.image :refer :all]
            [dynamo.outline :refer :all]
            [dynamo.particle-lib :as pl :refer :all]
            [dynamo.ui :as ui :refer [defcommand defhandler]]
            [internal.ui.menus :as menus]
            [internal.ui.handlers :as handlers]
            [internal.render.pass :as pass]
            [service.log :as log :refer [logging-exceptions]]
            [camel-snake-kebab :refer :all])
  (:import  [com.dynamo.particle.proto Particle$ParticleFX
             Particle$Emitter Particle$Emitter$Property Particle$Emitter$ParticleProperty
             Particle$Modifier Particle$Modifier$Property
             Particle$SplinePoint]
            [com.jogamp.opengl.util.awt TextRenderer]
            [java.nio ByteBuffer IntBuffer]
            [dynamo.types Animation Image TextureSet Rect EngineFormatTexture AABB]
            [java.awt.image BufferedImage]
            [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
            [org.eclipse.swt.opengl GLData GLCanvas]
            [javax.vecmath Matrix4d Point3d Quat4d Vector3d]
            [org.eclipse.core.commands ExecutionEvent]
            [com.sun.jna Pointer Native Callback]))

(defnode TransformNode
  (property position {:schema Point3d})
  (property rotation {:schema Quat4d})
  (property scale    {:schema Vector3d}))

(defnk produce-property-value
  [this t points]
  ;; For now, return first value
  {(:key this) (:y (first points))})

(defnode PropertyNode
  (property key s/Any)
  (property points [{:x s/Num :y s/Num :t-x s/Num :t-y s/Num}])
  (input t s/Num)
  (output value s/Any :cached produce-property-value))

(defnode SpreadPropertyNode
  (inherits PropertyNode)
  (property spread s/Num))

(defnode EmitterPropertyNode
  (inherits SpreadPropertyNode))

(defnode ParticlePropertyNode
  (inherits PropertyNode))

(defnode ModifierPropertyNode
  (inherits SpreadPropertyNode))

(defnk produce-properties
  [this property-sources]
  (merge (select-keys this (-> this :descriptor :properties keys)) (apply merge-with concat property-sources)))

(defnode Properties
  (input property-sources [s/Any])
  (output properties s/Any produce-properties))

(defn half [v]
  (* 0.5 v))

(defn render-emitter-outlines
  [ctx ^GL2 gl this]
  (let [properties (n/get-node-value this :properties)
        size-x    (:size-x properties)
        size-y    (:size-y properties)
        size-z    (:size-z properties)]
    (print "hej")
    (do-gl [this            (assoc this :gl-context ctx)
                                this            (assoc this :gl gl)]
                               ;; Convert into vbo
                               (.glBegin gl GL/GL_LINES)
                               (.glVertex3f gl 0.0 0.0 0.0)
                               (.glVertex3f gl (half size-x) size-y 0.0)
                               (.glVertex3f gl (half size-x) size-y 0.0)
                               (.glVertex3f gl (half (- size-x)) size-y 0.0)
                               (.glVertex3f gl (half (- size-x)) size-y 0.0)
                               (.glVertex3f gl 0.0 0.0 0.0)
                               (.glEnd gl))))

(defn render-modifier-outlines
  [ctx ^GL2 gl this]
  (let [properties (n/get-node-value this :properties)
        scale    (:magnitude properties)]
    (print "PROPS!!!")
    (print this)
    (print properties)
    (print scale)
    (do-gl [this (assoc this :gl-context ctx)
            this (assoc this :gl gl)]
           ;; Convert into vbo
           (.glBegin gl GL/GL_LINES)
           (.glVertex3f gl 0.0 0.0 0.0)
           (.glVertex3f gl scale scale 0.0)
           (.glVertex3f gl scale scale 0.0)
           (.glVertex3f gl (- scale) scale 0.0)
           (.glVertex3f gl (- scale) scale 0.0)
           (.glVertex3f gl 0.0 0.0 0.0)
           (.glEnd gl))))

(defnk produce-renderable :- RenderData
  [this renderables]
  (let [world (Matrix4d. g/Identity4d)]
    (apply merge-with concat renderables)))

(defnk unit-bounding-box
  []
  (-> (g/null-aabb)
    (g/aabb-incorporate  1  1  1)
    (g/aabb-incorporate -1 -1 -1)))

(defn render-particles []
  (println "I'm rendering particles!"))

(defnode ParticlefxRender
  (property context     Pointer)
  (input    renderables [RenderData])
  (output   renderable  RenderData :cached produce-renderable))

(defnk compile-particlefx :- s/Bool
  [this g project particlefx]
  #_(.toByteArray (particlefx-protocol-buffer (:texture-name this) textureset))
  )

(defnode ParticlefxSave
  (input particlefx-properties s/Any)
  #_(output compiled-particlefx s/Any :on-update compile-particlefx))

(defnode ParticlefxProperties
  (inherits Properties))

(defnk passthrough-renderables
  [renderables]
  renderables)

(defnode ParticlefxNode
  (inherits OutlineNode)
  (inherits ParticlefxProperties)
  (input    renderables s/Any)
  (output   renderables s/Any :cached passthrough-renderables)
  (output   aabb        AABB  unit-bounding-box)
  #_ParticleLibrary$RenderInstanceCallback
  #_(invoke [this user-context material texture world-transform blend-mode vertex-index vertex-count constants constant-count]
          (render-particles)))

(defnode EmitterProperties
  (inherits Properties)
  (property id s/Str)
  (property mode s/Any)
  (property duration s/Num)
  (property space s/Any)
  (property tile-source s/Str)
  (property animation s/Str)
  (property material s/Str)
  (property blend-mode s/Any)
  (property particle-orientation s/Any)
  (property inherit-velocity s/Num)
  (property max-particle-count s/Int)
  (property type s/Any)
  (property start-delay s/Num))

(defnk produce-emitter-renderable :- RenderData
  [this project renderables]
  (let [world (Matrix4d. g/Identity4d)
        renderable {pass/outline
                    [{:world-transform world
                      :render-fn       (fn [ctx gl glu text-renderer] (render-emitter-outlines ctx gl this))}]}]
    (apply merge-with concat renderable renderables)
    ))

(defnode EmitterRender
  (input renderables    [RenderData])
  (output renderable    RenderData :cached produce-emitter-renderable))

(defnode EmitterNode
  (inherits TransformNode)
  (inherits OutlineNode)
  (inherits EmitterProperties)
  (inherits EmitterRender))

(defnode ModifierProperties
  (inherits Properties)
  (property type s/Any)
  (property use-direction int))

(defnk produce-modifier-renderable :- RenderData
  [this project]
  (let [world (Matrix4d. g/Identity4d)]
    {pass/outline
     [{:world-transform world
       :render-fn       (fn [ctx gl glu text-renderer] (render-modifier-outlines ctx gl this))}]}))

(defnode ModifierRender
    (output renderable    RenderData :cached produce-modifier-renderable))

(defnode ModifierNode
  (inherits TransformNode)
  (inherits OutlineNode)
  (inherits ModifierProperties)
  (inherits ModifierRender))

(protocol-buffer-converters
 Particle$ParticleFX
 {:constructor #'editors.particlefx/make-particlefx-node
 :node-properties {:emitters-list [:tree -> :children
                                   :renderable -> :renderables]
                   :modifiers-list [:tree -> :children
                                    :renderable -> :renderables]}}

 Particle$Emitter
 {:constructor #'editors.particlefx/make-emitter-node
  :basic-properties [:id :mode :duration :space :position :rotation :tile-source :animation :material :blend-mode
                  :particle-orientation :inherit-velocity :max-particle-count :type :start-delay]
  :node-properties {:properties-list [:value -> :property-sources]
                    :particle-properties-list [:value -> :property-sources]
                    :modifiers-list [:tree -> :children
                                     :renderable -> :renderables]}
  :enum-maps {:mode (automatic-protobuf-enum Particle$PlayMode "PLAY_MODE_")
              :space (automatic-protobuf-enum Particle$EmissionSpace "EMISSION_SPACE_")
              :blend-mode (automatic-protobuf-enum Particle$BlendMode "BLEND_MODE_")
              :particle-orientation (automatic-protobuf-enum Particle$ParticleOrientation "PARTICLE_ORIENTATION_")
              :type (automatic-protobuf-enum Particle$EmitterType "EMITTER_TYPE_")}}
 
 Particle$Emitter$Property
 {:constructor #'editors.particlefx/make-emitter-property-node
  :basic-properties [:key :spread :points-list]
  :enum-maps  {:key (automatic-protobuf-enum Particle$EmitterKey "EMITTER_KEY_")}}

 Particle$Emitter$ParticleProperty
 {:constructor #'editors.particlefx/make-particle-property-node
  :basic-properties [:key :points-list]
  :enum-maps  {:key (automatic-protobuf-enum Particle$ParticleKey "PARTICLE_KEY_")}}
 
 Particle$Modifier
 {:constructor #'editors.particlefx/make-modifier-node
  :basic-properties [:type :use-direction :position :rotation]
  :node-properties {:properties-list [:value -> :property-sources]}
  :enum-maps {:type (automatic-protobuf-enum Particle$ModifierType "MODIFIER_TYPE_")}}
 
 Particle$Modifier$Property
 {:constructor #'editors.particlefx/make-modifier-property-node
  :basic-properties [:key :spread :points-list]
  :enum-maps  {:key (automatic-protobuf-enum Particle$ModifierKey "MODIFIER_KEY_")}})

(defmethod message->node Particle$SplinePoint
  [msg]
  {:x (.getX msg)
   :y (.getY msg)
   :t-x (.getTX msg)
   :t-y (.getTY msg)})

(defn on-load
  [path ^Particle$ParticleFX particlefx-message]
  (let [particlefx (message->node particlefx-message :filename path :_id -1)
        particlefx-save (add (make-particlefx-save))]
    (println "LOADING")
    (ds/connect particlefx :self particlefx-save :particlefx-properties)))

(defn on-edit
  [project-node editor-site particlefx-node]
  (let [editor  (make-scene-editor :name "editor")]
    (transactional
      (in (add editor)
        (let [particlefx-render (add (make-particlefx-render))
              background (add (make-background))
              grid       (add (make-grid))
              camera     (add (make-camera-controller :camera (make-camera :orthographic)))
              context (pl/create-context 64 256)]
          (set-property particlefx-render :context context)
          #_(connect particlefx-node   :renderables particlefx-render :renderables)
          (connect camera            :camera      grid   :camera)
          (connect camera            :camera      editor :view-camera)
          (connect camera            :self        editor :controller)
          (connect background        :renderable  editor :renderables)
          (connect grid              :renderable  editor :renderables)
          #_(connect particlefx-render :renderable  editor :renderables)
          (connect particlefx-node   :aabb        editor :aabb)
          #_(
          ))
        editor))))

(p/register-editor "particlefx" #'on-edit)
(p/register-loader "particlefx" (protocol-buffer-loader Particle$ParticleFX on-load))
