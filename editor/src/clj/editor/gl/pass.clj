(ns editor.gl.pass
  (:require [dynamo.graph :as g]
            [schema.core :as s]
            [editor.types :as types])
  (:import [com.jogamp.opengl GL GL2]
           [com.jogamp.opengl.glu GLU]))

(set! *warn-on-reflection* true)

(defrecord RenderPass [nm selection model-transform depth-clipping]
  types/Pass
  (types/selection?       [this] selection)
  (types/model-transform? [this] model-transform)
  (types/depth-clipping?  [this] depth-clipping))

(defmacro make-pass [sym selection model-transform depth-clipping]
  `(def ~sym (RenderPass. ~(str sym) ~selection ~model-transform ~depth-clipping)))

(defmacro make-passes [& forms]
  (let [ps (partition 4 forms)]
    `(do
       ~@(map #(list* 'make-pass %) ps)
       (def all-passes ~(mapv first ps))
       (def selection-passes ~(mapv first (filter second ps)))
       (def render-passes    ~(mapv first (remove second ps))))))

(make-passes
  ; name selection model-transform depth-clipping
  background            false false false
  infinity-grid         false true  false
  opaque                false true  true
  transparent           false true  true
  outline               false true  true
  manipulator           false true  false
  overlay               false false true
  opaque-selection      true  true  true
  selection             true  true  true  ; transparent selection if you will...
  manipulator-selection true  true  false)

(g/deftype RenderData {(s/optional-key background)            s/Any
                       (s/optional-key infinity-grid)         s/Any
                       (s/optional-key opaque)                s/Any
                       (s/optional-key transparent)           s/Any
                       (s/optional-key outline)               s/Any
                       (s/optional-key manipulator)           s/Any
                       (s/optional-key overlay)               s/Any
                       (s/optional-key opaque-selection)      s/Any
                       (s/optional-key selection)             s/Any
                       (s/optional-key manipulator-selection) s/Any})

(defmulti prepare-gl (fn [pass gl glu] pass))

(defmethod prepare-gl background
  [_ ^GL2 gl ^GLU glu]
  (.glPolygonMode gl GL/GL_FRONT_AND_BACK GL2/GL_FILL)
  (.glDisable gl GL/GL_BLEND)
  (.glDisable gl GL/GL_DEPTH_TEST)
  (.glDepthMask gl false)
  (.glDisable gl GL/GL_SCISSOR_TEST)
  (.glDisable gl GL/GL_STENCIL_TEST)
  (.glStencilMask gl 0x0)
  (.glColorMask gl true true true true)
  (.glDisable gl GL2/GL_LINE_STIPPLE))

(defmethod prepare-gl infinity-grid
  [_ ^GL2 gl ^GLU glu]
  (doto gl
    (.glPolygonMode GL/GL_FRONT_AND_BACK GL2/GL_FILL)
    (.glEnable GL/GL_BLEND)
    (.glBlendFunc GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA)
    (.glDisable GL/GL_DEPTH_TEST)
    (.glDepthMask false)
    (.glDisable GL/GL_SCISSOR_TEST)
    (.glDisable GL/GL_STENCIL_TEST)
    (.glStencilMask 0x0)
    (.glColorMask true true true true)
    (.glDisable GL2/GL_LINE_STIPPLE)))

(defmethod prepare-gl opaque
  [_ ^GL2 gl glu]
  (doto gl
    (.glPolygonMode GL/GL_FRONT_AND_BACK GL2/GL_FILL)
    (.glDisable GL/GL_BLEND)
    (.glEnable GL/GL_DEPTH_TEST)
    (.glDepthMask true)
    (.glDisable GL/GL_SCISSOR_TEST)
    (.glDisable GL/GL_STENCIL_TEST)
    (.glStencilMask 0x0)
    (.glColorMask true true true true)
    (.glDisable GL2/GL_LINE_STIPPLE)))

(defmethod prepare-gl outline
  [_ ^GL2 gl glu]
  (doto gl
    (.glPolygonMode GL/GL_FRONT_AND_BACK GL2/GL_LINE)
    (.glDisable GL/GL_BLEND)
    (.glDisable GL/GL_DEPTH_TEST)
    (.glDepthMask false)
    (.glDisable GL/GL_SCISSOR_TEST)
    (.glDisable GL/GL_STENCIL_TEST)
    (.glStencilMask 0x0)
    (.glColorMask true true true true)
    (.glDisable GL2/GL_LINE_STIPPLE)))

(defmethod prepare-gl transparent
  [_ ^GL2 gl glu]
  (doto gl
    (.glPolygonMode GL/GL_FRONT_AND_BACK GL2/GL_FILL)
    (.glEnable GL/GL_BLEND)
    (.glBlendFunc GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA)
    (.glEnable GL/GL_DEPTH_TEST)
    (.glDepthMask false)
    (.glDisable GL/GL_SCISSOR_TEST)
    (.glDisable GL/GL_STENCIL_TEST)
    (.glStencilMask 0x0)
    (.glColorMask true true true true)
    (.glDisable GL2/GL_LINE_STIPPLE)))

(defmethod prepare-gl opaque-selection
  [_ ^GL2 gl glu]
  (doto gl
    (.glPolygonMode GL/GL_FRONT_AND_BACK GL2/GL_FILL)
    (.glDisable GL/GL_BLEND)
    (.glEnable GL/GL_DEPTH_TEST)
    (.glDepthMask true)
    (.glDisable GL/GL_SCISSOR_TEST)
    (.glDisable GL/GL_STENCIL_TEST)
    (.glStencilMask 0x0)
    (.glColorMask true true true true)
    (.glDisable GL2/GL_LINE_STIPPLE)))

(defmethod prepare-gl selection
  [_ ^GL2 gl glu]
  (doto gl
    (.glPolygonMode GL/GL_FRONT_AND_BACK GL2/GL_FILL)
    (.glEnable GL/GL_BLEND)
    (.glBlendFunc GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA)
    (.glEnable GL/GL_DEPTH_TEST)
    (.glDepthMask false)
    (.glDisable GL/GL_SCISSOR_TEST)
    (.glDisable GL/GL_STENCIL_TEST)
    (.glStencilMask 0x0)
    (.glColorMask true true true true)
    (.glDisable GL2/GL_LINE_STIPPLE)))

(defmethod prepare-gl manipulator
  [_ ^GL2 gl glu]
  (doto gl
    (.glPolygonMode GL/GL_FRONT_AND_BACK GL2/GL_FILL)
    (.glEnable GL/GL_BLEND)
    (.glBlendFunc GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA)
    (.glDisable GL/GL_DEPTH_TEST)
    (.glDepthMask false)
    (.glDisable GL/GL_SCISSOR_TEST)
    (.glDisable GL/GL_STENCIL_TEST)
    (.glStencilMask 0x0)
    (.glColorMask true true true true)
    (.glDisable GL2/GL_LINE_STIPPLE)))

(defmethod prepare-gl manipulator-selection
  [_ ^GL2 gl glu]
  (doto gl
    (.glPolygonMode GL/GL_FRONT_AND_BACK GL2/GL_FILL)
    (.glEnable GL/GL_BLEND)
    (.glBlendFunc GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA)
    (.glDisable GL/GL_DEPTH_TEST)
    (.glDepthMask false)
    (.glDisable GL/GL_SCISSOR_TEST)
    (.glDisable GL/GL_STENCIL_TEST)
    (.glStencilMask 0x0)
    (.glColorMask true true true true)
    (.glDisable GL2/GL_LINE_STIPPLE)))

(defmethod prepare-gl overlay
  [_ ^GL2 gl glu]
  (doto gl
    (.glPolygonMode GL/GL_FRONT_AND_BACK GL2/GL_FILL)
    (.glEnable GL/GL_BLEND)
    (.glBlendFunc GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA)
    (.glDisable GL/GL_DEPTH_TEST)
    (.glDepthMask false)
    (.glDisable GL/GL_SCISSOR_TEST)
    (.glDisable GL/GL_STENCIL_TEST)
    (.glStencilMask 0x0)
    (.glColorMask true true true true)
    (.glDisable GL2/GL_LINE_STIPPLE)))

(doseq [[v doc]
        {*ns*
         "Render passes organize rendering into layers."

         #'selection-passes
         "Vector of the passes that participate in selection"

         #'render-passes
         "Vector of all non-selection passes, ordered from back to front."}]
  (alter-meta! v assoc :doc doc))
