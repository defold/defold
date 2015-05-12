(ns internal.render.pass
  (:require [dynamo.types :as t])
  (:import [javax.media.opengl GL GL2]
           [javax.media.opengl.glu GLU]))

(defrecord RenderPass [nm selection model-transform]
  t/Pass
  (t/selection?       [this] selection)
  (t/model-transform? [this] model-transform))

(defmacro make-pass [lbl sel model-xfm]
  `(def ~lbl (RenderPass. ~(str lbl) ~sel ~model-xfm)))

(defmacro make-passes [& forms]
  (let [ps (partition 3 forms)]
    `(do
       ~@(map #(list* 'make-pass %) ps)
       (def selection-passes ~(mapv first (filter second ps)))
       (def render-passes    ~(mapv first (remove second ps))))))

(make-passes
  background     false false
  opaque         false true
  transparent    false true
  icon-outline   false false
  outline        false true
  manipulator    false true
  overlay        false false
  selection      true  true
  manipulator-selection      false true
  icon           false false
  icon-selection true false)

(defmulti prepare-gl (fn [pass gl glu] pass))

(defmethod prepare-gl background
  [_ ^GL2 gl ^GLU glu]
  (.glMatrixMode gl GL2/GL_PROJECTION)
  (.glLoadIdentity gl)
  (.gluOrtho2D glu -1.0 1.0 -1.0 1.0)
  (.glMatrixMode gl GL2/GL_MODELVIEW)
  (.glLoadIdentity gl)
  (.glPolygonMode gl GL/GL_FRONT_AND_BACK GL2/GL_FILL)
  (.glDisable gl GL/GL_BLEND)
  (.glDisable gl GL/GL_DEPTH_TEST)
  (.glDepthMask gl false)
  (.glDisable gl GL/GL_SCISSOR_TEST)
  (.glDisable gl GL/GL_STENCIL_TEST)
  (.glStencilMask gl 0xFF)
  (.glColorMask gl true true true true)
  (.glDisable gl GL2/GL_LINE_STIPPLE))

(defmethod prepare-gl opaque
  [_ ^GL2 gl glu]
  (doto gl
    (.glPolygonMode GL/GL_FRONT_AND_BACK GL2/GL_FILL)
    (.glDisable GL/GL_BLEND)
    (.glEnable GL/GL_DEPTH_TEST)
    (.glDepthMask true)
    (.glDisable GL/GL_SCISSOR_TEST)
    (.glDisable GL/GL_STENCIL_TEST)
    (.glStencilMask 0xFF)
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
    (.glStencilMask 0xFF)
    (.glColorMask true true true true)
    (.glDisable GL2/GL_LINE_STIPPLE)))

(defmethod prepare-gl transparent
  [_ ^GL2 gl glu]
  (doto gl
    (.glPolygonMode GL/GL_FRONT_AND_BACK GL2/GL_FILL)
    (.glEnable GL/GL_BLEND)
    (.glBlendFunc GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA)
    (.glDisable GL/GL_DEPTH_TEST)
    (.glDepthMask false)
    (.glDisable GL/GL_SCISSOR_TEST)
    (.glDisable GL/GL_STENCIL_TEST)
    (.glStencilMask 0xFF)
    (.glColorMask true true true true)
    (.glDisable GL2/GL_LINE_STIPPLE)))

(defmethod prepare-gl selection
  [_ ^GL2 gl glu]
  (doto gl
    (.glPolygonMode GL/GL_FRONT_AND_BACK GL2/GL_FILL)
    (.glEnable GL/GL_BLEND)
    (.glBlendFunc GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA)
    (.glDisable GL/GL_DEPTH_TEST)
    (.glDepthMask false)
    (.glDisable GL/GL_SCISSOR_TEST)
    (.glDisable GL/GL_STENCIL_TEST)
    (.glStencilMask 0xFF)
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
    (.glStencilMask 0xFF)
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
    (.glStencilMask 0xFF)
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
    (.glStencilMask 0xFF)
    (.glColorMask true true true true)
    (.glDisable GL2/GL_LINE_STIPPLE)))

(defmethod prepare-gl icon
  [_ ^GL2 gl glu]
  (doto gl
    (.glPolygonMode GL/GL_FRONT_AND_BACK GL2/GL_FILL)
    (.glEnable GL/GL_BLEND)
    (.glBlendFunc GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA)
    (.glDisable GL/GL_DEPTH_TEST)
    (.glDepthMask false)
    (.glDisable GL/GL_SCISSOR_TEST)
    (.glDisable GL/GL_STENCIL_TEST)
    (.glStencilMask 0xFF)
    (.glColorMask true true true true)
    (.glDisable GL2/GL_LINE_STIPPLE)))

(defmethod prepare-gl icon-selection
  [_ ^GL2 gl glu]
  (doto gl
    (.glPolygonMode GL/GL_FRONT_AND_BACK GL2/GL_FILL)
    (.glEnable GL/GL_BLEND)
    (.glBlendFunc GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA)
    (.glDisable GL/GL_DEPTH_TEST)
    (.glDepthMask false)
    (.glDisable GL/GL_SCISSOR_TEST)
    (.glDisable GL/GL_STENCIL_TEST)
    (.glStencilMask 0xFF)
    (.glColorMask true true true true)
    (.glDisable GL2/GL_LINE_STIPPLE)))

(defmethod prepare-gl icon-outline
  [_ ^GL2 gl glu]
  (doto gl
    (.glPolygonMode GL/GL_FRONT_AND_BACK GL2/GL_LINE)
    (.glDisable GL/GL_BLEND)
    (.glDisable GL/GL_DEPTH_TEST)
    (.glDepthMask false)
    (.glDisable GL/GL_SCISSOR_TEST)
    (.glDisable GL/GL_STENCIL_TEST)
    (.glStencilMask 0xFF)
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
