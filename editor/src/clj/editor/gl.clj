;; Copyright 2020-2024 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;; 
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;; 
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.gl
  "Expose some GL functions and constants with Clojure-ish flavor"
  (:refer-clojure :exclude [repeat])
  (:require [clojure.string :as string]
            [editor.gl.protocols :as p]
            [service.log :as log])
  (:import [java.awt Font]
           [java.nio IntBuffer]
           [com.jogamp.opengl GL GL2 GLContext GLDrawableFactory GLProfile GLException GLAutoDrawable GLOffscreenAutoDrawable GLCapabilities]
           [javax.vecmath Matrix4d]
           [com.jogamp.opengl.awt GLCanvas]
           [com.jogamp.opengl.util.awt TextRenderer]))

(set! *warn-on-reflection* true)

(defonce ^:private gl-info-atom (atom nil))
(defonce ^:private required-functions ["glGenBuffers"])

(defn- profile ^GLProfile []
  (try
    (GLProfile/getGL2ES1)
    (catch GLException e
      (log/warn :message "Failed to acquire GL profile for GL2/GLES1.")
      (GLProfile/getDefault))))

(defn drawable-factory
  (^GLDrawableFactory [] (drawable-factory (profile)))
  (^GLDrawableFactory [^GLProfile profile] (GLDrawableFactory/getFactory profile)))

(defn- unchecked-offscreen-drawable ^GLOffscreenAutoDrawable [w h]
  (let [profile (profile)
        factory (drawable-factory profile)
        caps    (doto (GLCapabilities. profile)
                  (.setOnscreen false)
                  (.setFBO true)
                  (.setPBuffer true)
                  (.setDoubleBuffered false)
                  (.setStencilBits 8))
        drawable (.createOffscreenAutoDrawable factory nil caps nil w h)
        context (.createContext drawable nil)]
    (.setContext drawable context true)
    drawable))

(def ^:private last-make-current-failure (atom 0))

(defn- time-to-log? []
  (let [now (System/currentTimeMillis)]
    (when (> (- now @last-make-current-failure) (* 1000 60))
      (reset! last-make-current-failure now)
      true)))

(def ^:private ignored-message-ids
  (int-array
    [;; Software processing fallback warning. The render mode is
     ;; GL_FEEDBACK or GL_SELECT, neither of which is hardware accelerated.
     0x20005
     ;; Buffer object n will use VIDEO memory as the source for buffer
     ;; object operations
     0x20071]))

(defn- ignore-some-gl-warnings! [^GLContext context]
  (.glDebugMessageControl context
                          GL2/GL_DEBUG_SOURCE_API
                          GL2/GL_DEBUG_TYPE_OTHER
                          GL2/GL_DONT_CARE
                          (count ignored-message-ids)
                          ignored-message-ids
                          0
                          false))

(defn make-current ^GLContext [^GLAutoDrawable drawable]
  (when-let [^GLContext context (.getContext drawable)]
    (try
      (let [result (.makeCurrent context)]
        (if (= result GLContext/CONTEXT_NOT_CURRENT)
          (do
            (when (time-to-log?)
              (log/warn :message "Failed to set gl context as current."))
            nil)
          (doto context ignore-some-gl-warnings!)))
      (catch Exception e
        (when (time-to-log?)
          (log/error :exception e))
        nil))))

(defmacro with-drawable-as-current
  [^GLAutoDrawable drawable & forms]
  `(when-let [^GLContext ~'gl-context (make-current ~drawable)]
     (try
       (let [^GL2 ~'gl (.getGL ~'gl-context)]
         ~@forms)
       (finally (.release ~'gl-context)))))

(defn- init-info! []
  (let [drawable (unchecked-offscreen-drawable 100 100)]
    (with-drawable-as-current drawable
      (let [^GL gl (.getGL gl-context)]
        (reset! gl-info-atom {:vendor (.glGetString gl GL2/GL_VENDOR)
                              :renderer (.glGetString gl GL2/GL_RENDERER)
                              :version (.glGetString gl GL2/GL_VERSION)
                              :shading-language-version (.glGetString gl GL2/GL_SHADING_LANGUAGE_VERSION)
                              :desc (.toString gl-context)
                              :missing-functions (filterv (fn [^String name] (not (.isFunctionAvailable gl-context name))) required-functions)})))
    (.destroy drawable)
    @gl-info-atom))

(defn info []
  (or @gl-info-atom (init-info!)))

(defn gl-support-error ^String []
  (let [info (info)]
    (when-let [missing (seq (:missing-functions info))]
      (string/join "\n"
                   [(format "The graphics device does not support: %s" (string/join ", " missing))
                    (format "GPU: %s" (:renderer info))
                    (format "Driver: %s" (:version info))]))))

(defn offscreen-drawable ^GLOffscreenAutoDrawable [w h]
  (when (empty? (:missing-functions (info)))
    (unchecked-offscreen-drawable w h)))

(defn gl-init-vba [^GL2 gl]
  (let [vba-name-buf (IntBuffer/allocate 1)]
    (.glGenVertexArrays gl 1 vba-name-buf)
    (let [vba-name (.get vba-name-buf 0)]
      (.glBindVertexArray gl vba-name)
      vba-name)))

(defn gl-gen-buffers [^GL2 gl nbufs]
  (let [names (IntBuffer/allocate nbufs)]
    (.glGenBuffers gl nbufs names)
    (.array names)))

(defn gl-delete-buffers [^GL2 gl & bufs]
  (let [names (IntBuffer/allocate (count bufs))]
    (doseq [^int buf bufs]
      (.put names buf))
    (.flip names)
    (.glDeleteBuffers gl (count bufs) ^IntBuffer names)))

(defmacro gl-polygon-mode [gl face mode] `(.glPolygonMode ~gl ~face ~mode))

(defmacro gl-get-attrib-location [gl shader name]                        `(.glGetAttribLocation ~gl ~shader ~name))
(defmacro gl-bind-buffer [gl type name]                                  `(.glBindBuffer ~gl ~type ~name))
(defmacro gl-buffer-data [gl type size data usage]                       `(.glBufferData ~gl ~type ~size ~data ~usage))
(defmacro gl-vertex-attrib-pointer [gl idx size type norm stride offset] `(.glVertexAttribPointer ~gl ~idx ~size ~type ~norm ~stride ~offset))
(defmacro gl-vertex-attrib-divisor [gl idx divisor]                      `(.glVertexAttribDivisor ~gl ~idx ~divisor))
(defmacro gl-enable-vertex-attrib-array [gl idx]                         `(.glEnableVertexAttribArray ~gl ~idx))
(defmacro gl-disable-vertex-attrib-array [gl idx]                        `(.glDisableVertexAttribArray ~gl ~idx))
(defmacro gl-use-program [gl idx]                                        `(.glUseProgram ~gl ~idx))
(defmacro gl-enable [gl cap]                                             `(.glEnable ~gl ~cap))
(defmacro gl-disable [gl cap]                                            `(.glDisable ~gl ~cap))
(defmacro gl-cull-face [gl mode]                                         `(.glCullFace ~gl ~mode))
(defmacro gl-blend-func [gl sfactor dfactor]                             `(.glBlendFunc ~gl ~sfactor ~dfactor))
(defmacro gl-front-face [gl mode]                                        `(.glFrontFace ~gl ~mode))

(defn gl-get-integer-v
  [^GL2 gl ^Integer param sz]
  (let [buff (int-array sz)]
    (.glGetIntegerv gl param buff 0)
    buff))

(defn gl-max-texture-units
  [^GL2 gl]
  (first (gl-get-integer-v gl GL2/GL_MAX_TEXTURE_UNITS 1)))

(defn gl-active-texture
  ^long [^GL2 gl]
  (let [out (int-array 1)]
    (.glGetIntegerv gl GL2/GL_ACTIVE_TEXTURE out 0)
    (aget out 0)))

(defn gl-current-program [^GL2 gl]
  (first (gl-get-integer-v gl GL2/GL_CURRENT_PROGRAM 1)))

(defn text-renderer [font-name font-style font-size]
  (doto (TextRenderer. (Font. font-name font-style font-size) false false)
    ;; NOTE: the TextRenderer implementation has two modes, one using
    ;; vertex arrays and VBOs, and one using immediate mode. This
    ;; forces the use of the immediate mode implementation as we've
    ;; seen issues on some platforms with the other one.
    (.setUseVertexArrays false)))

(defn gl-clear [^GL2 gl r g b a]
  (.glClearColor gl r g b a)
  (.glEnable gl GL/GL_DEPTH_TEST)
  (.glDepthMask gl true)
  (.glClearDepth gl 1.0)
  (.glEnable gl GL/GL_STENCIL_TEST)
  (.glStencilMask gl 0xFF)
  (.glClearStencil gl 0)
  (.glClear gl (bit-or GL/GL_COLOR_BUFFER_BIT GL/GL_DEPTH_BUFFER_BIT GL/GL_STENCIL_BUFFER_BIT))
  (.glDisable gl GL/GL_DEPTH_TEST)
  (.glDepthMask gl false)
  (.glDisable gl GL/GL_STENCIL_TEST)
  (.glStencilMask gl 0x0))

(defmacro gl-begin [gl type & body]
  `(do
     (.glBegin ~gl ~type)
     (doto ~gl
       ~@body)
     (.glEnd ~gl)))

(defmacro gl-quads [gl & body]
  `(gl-begin ~gl GL2/GL_QUADS ~@body))

(defmacro gl-lines [gl & body]
  `(gl-begin ~gl GL/GL_LINES ~@body))

(defmacro gl-triangles [gl & body]
  `(gl-begin ~gl GL2/GL_TRIANGLES ~@body))

(defmacro on-canvas [canvas & body]
  `(do
     (.setCurrent ~canvas)
     (try
       (do ~@body)
       (finally
         (.swapBuffers ~canvas)))))

(defmacro with-gl-bindings
  [glsymb render-args-symb gl-stuff & body]
  (assert (vector? gl-stuff) (str "GL objects must be a vector of values that satisfy GlBind" gl-stuff))
  `(let [gl# ~glsymb
         render-args# ~render-args-symb
         items# ~gl-stuff]
     (doseq [b# items#]
       (when (satisfies? p/GlBind b#)
         (p/bind b# gl# render-args#)))
     (let [res# (do ~@body)]
       (doseq [b# (reverse items#)]
         (when (satisfies? p/GlBind b#)
           (p/unbind b# gl# render-args#)))
       res#)))

(defn bind [^GL2 gl bindable render-args]
  (p/bind bindable gl render-args))

(defn unbind [^GL2 gl bindable render-args]
  (p/unbind bindable gl render-args))

(defmacro do-gl
  [glsymb render-args bindings & body]
  (assert (even? (count bindings)) "Bindings must contain an even number of forms")
  (let [bound-syms (map first (partition 2 bindings))]
    `(let ~bindings
       (with-gl-bindings ~glsymb ~render-args ~(into [] bound-syms)
         ~@body))))

(defmacro gl-push-matrix [gl & body]
  `(let [^GL2 gl# ~gl]
     (try
       (.glPushMatrix gl#)
       ~@body
       (finally
         (.glPopMatrix gl#)))))

(defn matrix->floats [^Matrix4d mat]
  (float-array [(.m00 mat) (.m10 mat) (.m20 mat) (.m30 mat)
                (.m01 mat) (.m11 mat) (.m21 mat) (.m31 mat)
                (.m02 mat) (.m12 mat) (.m22 mat) (.m32 mat)
                (.m03 mat) (.m13 mat) (.m23 mat) (.m33 mat)]))

(defn gl-load-matrix-4d [^GL2 gl ^Matrix4d mat]
  (.glLoadMatrixf gl (matrix->floats mat) 0))

(defn gl-mult-matrix-4d [^GL2 gl ^Matrix4d mat]
  (.glMultMatrixf gl (matrix->floats mat) 0))

(defmacro color
  ([r g b]        `(float-array [(/ ~r 255.0) (/ ~g 255.0) (/ ~b 255.0)]))
  ([r g b a]      `(float-array [(/ ~r 255.0) (/ ~g 255.0) (/ ~b 255.0) a])))

(defmacro gl-color       [gl c]     `(.glColor4d ~gl (nth ~c 0) (nth ~c 1) (nth ~c 2) (nth ~c 3)))
(defmacro gl-color-3f    [gl r g b]     `(.glColor3f ~gl ~r ~g ~b))
(defmacro gl-color-4d    [gl r g b a]   `(.glColor4d ~gl ~r ~g ~b ~a))
(defmacro gl-color-3dv+a [gl dv alpha]  `(gl-color-4d ~gl (first ~dv) (second ~dv) (nth ~dv 2) ~alpha))
(defmacro gl-color-3dv   [gl cv off]    `(.glColor3dv ~gl ~cv ~off))
(defmacro gl-color-3fv   [gl cv off]    `(.glColor3fv ~gl ~cv ~off))

(defmacro gl-vertex-2f   [gl x y]       `(.glVertex2f ~gl ~x ~y))
(defmacro gl-vertex-3d   [gl x y z]     `(.glVertex3d ~gl ~x ~y ~z))
(defmacro gl-vertex-3dv
  ([gl vtx]
   `(.glVertex3dv ~gl ~vtx))
  ([gl vtx off]
   `(.glVertex3dv ~gl ~vtx ~off)))

(defmacro gl-translate-f [gl x y z]     `(.glTranslatef ~gl ~x ~y ~z))

(defmacro gl-draw-arrays [gl prim-type start count]      `(.glDrawArrays ~(with-meta gl {:tag `GL}) ~prim-type ~start ~count))
(defmacro gl-draw-elements [gl prim-type start count]    `(.glDrawElements ~(with-meta gl {:tag `GL}) ~prim-type ~count GL/GL_UNSIGNED_INT ~start))

(defmacro gl-uniform-matrix-4fv [gl idx cnt transpose val offset] `(.glUniformMatrix4fv ~gl ~idx ~cnt ~transpose ~val ~offset))

(defmacro glu-ortho [glu region]
  `(.gluOrtho2D ~glu (double (.left ~region)) (double (.right ~region)) (double (.bottom ~region)) (double (.top ~region))))

(def red                    GL2/GL_RED)
(def green                  GL2/GL_GREEN)
(def blue                   GL2/GL_BLUE)
(def alpha                  GL2/GL_ALPHA)
(def zero                   GL2/GL_ZERO)
(def one                    GL2/GL_ONE)
(def lequal                 GL2/GL_LEQUAL)
(def gequal                 GL2/GL_GEQUAL)
(def less                   GL2/GL_LESS)
(def greater                GL2/GL_GREATER)
(def equal                  GL2/GL_EQUAL)
(def notequal               GL2/GL_NOTEQUAL)
(def always                 GL2/GL_ALWAYS)
(def never                  GL2/GL_NEVER)
(def clamp-to-edge          GL2/GL_CLAMP_TO_EDGE)
(def clamp-to-border        GL2/GL_CLAMP_TO_BORDER)
(def mirrored-repeat        GL2/GL_MIRRORED_REPEAT)
(def repeat                 GL2/GL_REPEAT)
(def clamp                  GL2/GL_CLAMP)
(def compare-ref-to-texture GL2/GL_COMPARE_REF_TO_TEXTURE)
(def none                   GL2/GL_NONE)
(def nearest                GL2/GL_NEAREST)
(def linear                 GL2/GL_LINEAR)
(def nearest-mipmap-nearest GL2/GL_NEAREST_MIPMAP_NEAREST)
(def linear-mipmap-nearest  GL2/GL_LINEAR_MIPMAP_NEAREST)
(def nearest-mipmap-linear  GL2/GL_NEAREST_MIPMAP_LINEAR)
(def linear-mipmap-linear   GL2/GL_LINEAR_MIPMAP_LINEAR)

(defn viewport-array ^"[I" [viewport]
  (int-array [(:left viewport)
              (:top viewport)
              (:right viewport)
              (:bottom viewport)]))

(defmacro glu-pick-matrix [glu pick-rect viewport]
  `(let [pick-rect# ~pick-rect]
     (.gluPickMatrix ~glu
       (double (:x pick-rect#))
       (double (- (:bottom ~viewport) (:y pick-rect#)))
       (double (:width pick-rect#))
       (double (:height pick-rect#))
       (viewport-array ~viewport)
       (int 0))))

(defn overlay
  ([^GL2 gl ^TextRenderer text-renderer ^String chars ^Float xloc ^Float yloc]
   (overlay gl text-renderer chars xloc yloc 1 1 1 1))
  ([^GL2 gl ^TextRenderer text-renderer ^String chars ^Float xloc ^Float yloc r g b a]
   (overlay gl text-renderer chars xloc yloc r g b a 0.0))
  ([^GL2 gl ^TextRenderer text-renderer ^String chars ^Float xloc ^Float yloc r g b a ^Float rot-z]
   (gl-push-matrix gl
                   (.glScaled gl 1 -1 1)
                   (.glTranslated gl xloc yloc 0)
                   (.glRotated gl rot-z 0 0 1)
                   (.setColor text-renderer r g b a)
                   (.begin3DRendering text-renderer)
                   (.draw3D text-renderer chars 0.0 0.0 1.0 1.0)
                   (.end3DRendering text-renderer))))

(defn set-blend-mode [^GL gl blend-mode]
  ;; Assumes pre-multiplied source/destination
  ;; Equivalent of defold/engine/gamesys/src/gamesys/components/comp_particlefx.cpp
  (case blend-mode
    :blend-mode-alpha (gl-blend-func gl GL/GL_ONE GL/GL_ONE_MINUS_SRC_ALPHA)
    (:blend-mode-add :blend-mode-add-alpha) (gl-blend-func gl GL/GL_ONE GL/GL_ONE)
    :blend-mode-mult (gl-blend-func gl GL/GL_DST_COLOR GL/GL_ONE_MINUS_SRC_ALPHA)
    :blend-mode-screen (gl-blend-func gl GL/GL_ONE_MINUS_DST_COLOR GL/GL_ONE)))
