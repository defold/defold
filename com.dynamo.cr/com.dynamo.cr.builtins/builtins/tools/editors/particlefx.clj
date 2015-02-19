(ns editors.particlefx
  (:require [plumbing.core :refer [fnk defnk]]
            [dynamo.background :as background]
            [dynamo.camera :refer :all]
            [schema.core :as s]
            [dynamo.editors :as ed]
            [dynamo.file :refer :all]
            [dynamo.file.protobuf :as protobuf :refer [protocol-buffer-converters]]
            [dynamo.geom :as g]
            [dynamo.gl :as gl]
            [dynamo.grid :as grid]
            [dynamo.node :as n]
            [dynamo.particle-lib :as pl :refer :all]
            [dynamo.project :as p]
            [dynamo.system :as ds]
            [dynamo.types :refer :all]
            [dynamo.ui :refer :all]
            [internal.render.pass :as pass])
  (:import  [com.dynamo.particle.proto Particle$ParticleFX
             Particle$Emitter Particle$Emitter$Property Particle$Emitter$ParticleProperty
             Particle$Modifier Particle$Modifier$Property
             Particle$SplinePoint]
            [com.jogamp.opengl.util.awt TextRenderer]
            [java.nio ByteBuffer IntBuffer]
            [dynamo.types AABB]
            [java.awt.image BufferedImage]
            [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
            [javax.vecmath Matrix4d Point3d Quat4d Vector3d]
            [org.eclipse.core.commands ExecutionEvent]
            [com.sun.jna Pointer Native Callback]))

(n/defnode TransformNode
  (property position {:schema Point3d})
  (property rotation {:schema Quat4d})
  (property scale    {:schema Vector3d}))

(defnk produce-property-value
  [this t points]
  ;; For now, return first value
  {(:key this) (:y (first points))})

(n/defnode PropertyNode
  (property key s/Any)
  (property points [{:x s/Num :y s/Num :t-x s/Num :t-y s/Num}])
  (input t s/Num)
  (output value s/Any :cached produce-property-value))

(n/defnode SpreadPropertyNode
  (inherits PropertyNode)
  (property spread s/Num))

(n/defnode EmitterPropertyNode
  (inherits SpreadPropertyNode))

(n/defnode ParticlePropertyNode
  (inherits PropertyNode))

(n/defnode ModifierPropertyNode
  (inherits SpreadPropertyNode))

(defnk produce-properties
  [this property-sources]
  (merge (select-keys this (-> this :descriptor :properties keys)) (apply merge-with concat property-sources)))

(n/defnode PropertiesNode
  (input property-sources [s/Any])
  (output properties s/Any produce-properties))

(defn half [v]
  (* 0.5 v))

(defn render-emitter-outlines
  [ctx ^GL2 gl this properties]
  (let [size-x    (:size-x properties)
        size-y    (:size-y properties)
        size-z    (:size-z properties)]
    (print "hej")
    (gl/do-gl gl [this (assoc this :gl-context ctx)
                  this (assoc this :gl gl)]
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
  [ctx ^GL2 gl this properties]
  (let [scale    (:magnitude properties)]
    (print "PROPS!!!")
    (print this)
    (print properties)
    (print scale)
    (gl/do-gl gl [this (assoc this :gl-context ctx)
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

(n/defnode ParticlefxRender
  (property context     Pointer)
  (input    renderables [RenderData])
  (output   renderable  RenderData :cached produce-renderable))

(defnk compile-particlefx :- s/Bool
  [this g project particlefx]
  ; TODO
  )

(n/defnode ParticlefxSave
  (input particlefx-properties s/Any)
  #_(output compiled-particlefx s/Any :on-update compile-particlefx))

(n/defnode ParticlefxProperties
  (inherits PropertiesNode))

(defnk passthrough-renderables
  [renderables]
  renderables)

(n/defnode ParticlefxNode
  (inherits n/OutlineNode)
  (output outline-label s/Str (fnk [] "Particle FX"))
  (inherits ParticlefxProperties)
  (input    renderables s/Any)
  (output   renderables s/Any :cached passthrough-renderables)
  (output   aabb        AABB  unit-bounding-box)
  #_ParticleLibrary$RenderInstanceCallback
  #_(invoke [this user-context material texture world-transform blend-mode vertex-index vertex-count constants constant-count]
          (render-particles)))

(n/defnode EmitterProperties
  (inherits PropertiesNode)
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
  [this project renderables properties]
  (let [world (Matrix4d. g/Identity4d)
        renderable {pass/outline
                    [{:world-transform world
                      :render-fn       (fn [ctx gl glu text-renderer] (render-emitter-outlines ctx gl this properties))}]}]
    (apply merge-with concat renderable renderables)
    ))

(n/defnode EmitterRender
  (input renderables    [RenderData])
  (output renderable    RenderData :cached produce-emitter-renderable))

(n/defnode EmitterNode
  (inherits TransformNode)
  (inherits n/OutlineNode)
  (output outline-label s/Str (fnk [] "emitter"))
  (inherits EmitterProperties)
  (inherits EmitterRender))

(n/defnode ModifierProperties
  (inherits PropertiesNode)
  (property type s/Any)
  (property use-direction int))

(defnk produce-modifier-renderable :- RenderData
  [this project properties]
  (let [world (Matrix4d. g/Identity4d)]
    {pass/outline
     [{:world-transform world
       :render-fn       (fn [ctx gl glu text-renderer] (render-modifier-outlines ctx gl this properties))}]}))

(n/defnode ModifierRender
    (output renderable    RenderData :cached produce-modifier-renderable))

(n/defnode ModifierNode
  (inherits TransformNode)
  (inherits n/OutlineNode)
  (output outline-label s/Str (fnk [type] (str type)))
  (inherits ModifierProperties)
  (inherits ModifierRender))

(protocol-buffer-converters
Particle$ParticleFX
{:node-type       ParticlefxNode
 :node-properties {:emitters-list [:tree -> :children
                                   :renderable -> :renderables]
                   :modifiers-list [:tree -> :children
                                    :renderable -> :renderables]}}

Particle$Emitter
{:node-type        EmitterNode
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
{:node-type        EmitterPropertyNode
 :basic-properties [:key :spread :points-list]
 :enum-maps        {:key (automatic-protobuf-enum Particle$EmitterKey "EMITTER_KEY_")}}

Particle$Emitter$ParticleProperty
{:node-type        ParticlePropertyNode
 :basic-properties [:key :points-list]
 :enum-maps        {:key (automatic-protobuf-enum Particle$ParticleKey "PARTICLE_KEY_")}}

Particle$Modifier
{:node-type        ModifierNode
 :basic-properties [:type :use-direction :position :rotation]
 :node-properties  {:properties-list [:value -> :property-sources]}
 :enum-maps        {:type (automatic-protobuf-enum Particle$ModifierType "MODIFIER_TYPE_")}}

Particle$Modifier$Property
{:node-type        ModifierPropertyNode
 :basic-properties [:key :spread :points-list]
 :enum-maps        {:key (automatic-protobuf-enum Particle$ModifierKey "MODIFIER_KEY_")}})

(defmethod protobuf/message->node Particle$SplinePoint
  [^Particle$SplinePoint msg]
  {:x (.getX msg)
   :y (.getY msg)
   :t-x (.getTX msg)
   :t-y (.getTY msg)})

(defn on-load
  [project-node path reader]
  (let [particlefx-message (protobuf/read-text Particle$ParticleFX reader)
        particlefx         (protobuf/message->node particlefx-message)
        particlefx-save    (ds/add (n/construct ParticlefxSave))]
    (ds/connect particlefx :self particlefx-save :particlefx-properties)
    particlefx))

(defn on-edit
  [project-node editor-site particlefx-node]
  (let [editor (n/construct ed/SceneEditor :name "editor")]
    (ds/in (ds/add editor)
      (let [particlefx-render (ds/add (n/construct ParticlefxRender))
            background        (ds/add (n/construct background/Gradient))
            grid              (ds/add (n/construct grid/Grid))
            camera            (ds/add (n/construct CameraController :camera (make-camera :orthographic)))
            context           (pl/create-context 64 256)]
        (ds/set-property particlefx-render :context context)
        #_(connect particlefx-node   :renderables particlefx-render :renderables)
        (ds/connect camera            :camera      grid   :camera)
        (ds/connect camera            :camera      editor :view-camera)
        (ds/connect camera            :self        editor :controller)
        (ds/connect background        :renderable  editor :renderables)
        (ds/connect grid              :renderable  editor :renderables)
        #_(connect particlefx-render :renderable  editor :renderables)
        (ds/connect particlefx-node   :aabb        editor :aabb)
        #_(
        ))
      editor)))

(when (ds/in-transaction?)
  (p/register-editor "particlefx" #'on-edit)
  (p/register-node-type "particlefx" ParticlefxNode))
