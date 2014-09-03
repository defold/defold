(ns jogl-samples.gl3-buffer-object
  (:require [dynamo.editors :as e]
            [internal.ui.editors :as ue]
            [dynamo.gl :as gl]
            [dynamo.gl.protocols :refer [enable disable]]
            [dynamo.gl.shader :as s]
            [dynamo.gl.buffers :as b]
            [dynamo.geom :as g]
            [service.log :refer [logging-exceptions]])
  (:import [javax.media.opengl GL GL2]
           [java.nio ByteBuffer IntBuffer]
           [com.jogamp.common.nio Buffers]
           [org.eclipse.swt SWT]
           [org.eclipse.swt.layout FillLayout]
           [org.eclipse.swt.opengl GLCanvas GLData]
           [org.eclipse.swt.widgets Composite Display Shell Label]))

(defn update-camera
  [state evt]
  (let [canvas (:canvas @state)
        context (:context @state)
        area    (.getClientArea canvas)
        w       (.width area)
        h       (.height area)]
    (.setCurrent canvas)
    (gl/with-context context [gl glu]
      (gl/gl-viewport gl 0 0 w h))))

(defn redraw
  [state evt]
  (.redraw (:canvas @state)))

(def vertex-shader
  "#version 120

   #if __VERSION__ >= 130
	  #define attribute in
	  #define varying out
	 #endif

	 #ifdef GL_ES
	 precision mediump float;
	 precision mediump int;
	 #endif

	 uniform mat4    uniform_Projection;
	 attribute vec4  attribute_Position;
	 attribute vec4  attribute_Color;

	 varying vec4    varying_Color;

	 void main(void)
	 {
	   varying_Color = attribute_Color;
	   gl_Position = uniform_Projection * attribute_Position;
   }")

(def fragment-shader
  "#version 120

   #if __VERSION__ >= 130
     #define varying in
     out vec4 mgl_FragColor;
     #define texture2D texture
     #define gl_FragColor mgl_FragColor
   #endif

   #ifdef GL_ES
     precision mediump float;
     precision mediump int;
   #endif

   varying   vec4    varying_Color;  // incomming varying data to the
                                     // frament shader
                                     // sent from the vertex shader
   void main (void)
   {
     gl_FragColor = varying_Color;
   }")

(defn load-shaders
  [{:keys [context] :as state}]
  (try
    (gl/with-context context [gl glu]
     (assoc state :shader-program
            (s/make-program gl
                            (s/make-vertex-shader   gl vertex-shader)
                            (s/make-fragment-shader gl fragment-shader))))
    (catch Exception e
      (println (.getMessage e)))))

(defn locate-shader-variables
  [{:keys [context shader-program] :as state}]
  (gl/with-context context [gl glu]
    (let [matrix-idx   (.glGetUniformLocation gl shader-program "uniform_Projection")]
      (prn :uniform_Projection matrix-idx)
      (assoc state :matrix-uniform     matrix-idx))))

(def vertices (float-array
                [ 0  1 0 1   1 0 0 1
                 -1 -1 0 1   0 1 0 1
                  1 -1 0 1   0 0 1 1]))

(def fb-vertices (Buffers/newDirectFloatBuffer vertices))

(def vertex-format
  [["attribute_Position" 4 GL/GL_FLOAT false]
   ["attribute_Color"    4 GL/GL_FLOAT false]])

(defn bind-vb
  [{:keys [context color-attribute position-attribute shader-program] :as state}]
  (gl/with-context context [gl glu]
    (gl/gl-use-program gl shader-program)
    (let [buf (b/make-vertex-buffer gl shader-program vertex-format (* (count vertices) 4) fb-vertices)]
      (prn :vertex-buffer buf)
      (assoc state :vertex-buffer buf))))

(defn vertex-buffer-paint
  [state evt]
  (let [{:keys [canvas context shader-program
                matrix-uniform vertex-buffer]} @state]
    (gl/on-canvas canvas
       (gl/with-context context [gl glu]
         (gl/gl-clear gl 0 0.5 0.4 1)

         (let [m (g/as-array (g/ident))]
           (gl/gl-uniform-matrix-4fv gl matrix-uniform 1 false m 0))

;         (enable vertex-buffer)
         (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 3)
;         (disable vertex-buffer)
         ))))

(defn glshell [state]
  (let [display (ue/display)
       shell   (doto (Shell. display)
                 (.setText (:name state))
                 (.setLayout (FillLayout.))
                 (.setSize 720 600))
       panel   (doto (Composite. shell SWT/NONE)
                 (.setLayout (FillLayout.)))
       canvas  (gl/glcanvas panel)
       factory (gl/glfactory)]
   (.setCurrent canvas)
   (let [context (.createExternalGLContext factory)]
     (gl/with-context context [gl glu]
       (prn (gl/gl-version-info gl))
       (gl/gl-polygon-mode gl GL2/GL_FRONT_AND_BACK GL2/GL_FILL))
     (assoc state
            :shell shell
            :canvas canvas
            :context context))))

(defn on-event [type state & fs]
  (swap! state update-in [:events type] #(apply conj % fs))
  state)

(def on-resize (partial on-event SWT/Resize))
(def on-paint  (partial on-event SWT/Paint))

(defn realize [state-atom]
  (when-let [canvas (:canvas @state-atom)]
    (doseq [[evt-type handlers] (:events @state-atom)]
      (e/listen canvas evt-type
                (fn [evt]
                  (doseq [h handlers]
                    (h state-atom evt)))))
    (swap! state-atom dissoc :events))
  state-atom)

(defn show [state-atom]
  (.open (:shell @state-atom)))

(defn fail-safe
  [f]
  (fn
    ([a] (let [r (f a)] (if r r a)))
    ([a b] (let [r (f a b)] (if r r a)))
    ([a b c] (let [r (f a b c)] (if r r a)))))

(defn atomically [at f & args]
  (apply swap! at (fail-safe f) args)
  at)

(defn one-vba-triangle []
  (ue/swt-safe
    (logging-exceptions "main loop"
       (-> (atom {:name "One Vertex Buffer Triangle, using framework objects"})
         (atomically glshell)
         (atomically load-shaders)
         (atomically locate-shader-variables)
         (atomically bind-vb)
         (on-resize #'update-camera #'redraw)
         (on-paint  #'vertex-buffer-paint)
         realize
         show))))
