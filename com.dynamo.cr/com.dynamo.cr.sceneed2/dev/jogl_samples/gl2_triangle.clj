(ns jogl-samples.gl2-triangle
  (:require [dynamo.editors :as e]
            [internal.ui.editors :as ue]
            [dynamo.gl :as gl]
            [service.log :refer [logging-exceptions]])
  (:import [javax.media.opengl GL GL2]
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
      (.glMatrixMode gl GL2/GL_PROJECTION)
      (.glLoadIdentity gl)
      (.gluOrtho2D glu (float 0.0) (float w) (float 0.0) (float h))
      (.glMatrixMode gl GL2/GL_MODELVIEW)
  (prn :viewport [w h])
      (.glViewport gl 0 0 w h))))

(defn redraw
  [state evt]
  (.redraw (:canvas @state)))

(defn explicit-paint
  [state evt]
  (let [canvas (:canvas @state)
        context (:context @state)
        area (.getClientArea canvas)
        w    (.width area)
        h    (.height area)]
    (gl/on-canvas canvas
      (gl/with-context context [gl glu]
        (gl/gl-clear gl 0 0 0 1)
        (.glLoadIdentity gl)
        (gl/gl-triangles gl
          (gl/gl-color-3f 1 0 0)
          (gl/gl-vertex-2f 0 0)
          (gl/gl-color-3f 0 1 0)
          (gl/gl-vertex-2f w 0)
          (gl/gl-color-3f 0 0 1)
          (gl/gl-vertex-2f (/ w 2) h))))))


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
       (.glPolygonMode gl GL2/GL_FRONT_AND_BACK GL2/GL_FILL)
       (.glClearColor gl 0 0 0.4 1))
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

(defn swap-buffers [state-atom evt]
  (prn :swap)
  (.swapBuffers (:canvas @state-atom)))

(defn fail-safe
  [f]
  (fn
    ([a] (let [r (f a)] (if r r a)))
    ([a b] (let [r (f a b)] (if r r a)))
    ([a b c] (let [r (f a b c)] (if r r a)))))

(defn atomically [at f & args]
  (apply swap! at (fail-safe f) args)
  at)

(defn one-triangle []
  (ue/swt-safe
    (logging-exceptions "main loop"
      (-> (atom {:name "One Triangle"})
       (atomically glshell)
       (on-resize #'update-camera #'redraw)
       (on-paint  #'explicit-paint)
       realize
       show))))

