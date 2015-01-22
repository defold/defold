(ns dynamo.gl
  (:require [dynamo.gl.protocols :as p])
  (:import [java.awt Font]
           [java.nio IntBuffer]
           [javax.media.opengl GL GL2 GLDrawableFactory GLProfile]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d]
           [org.eclipse.swt SWT]
           [org.eclipse.swt.layout GridData]
           [org.eclipse.swt.opengl GLData GLCanvas]
           [com.jogamp.opengl.util.awt TextRenderer]))

(set! *warn-on-reflection* true)

(defn gl-version-info [^GL2 gl]
  {:vendor                   (.glGetString gl GL2/GL_VENDOR)
   :renderer                 (.glGetString gl GL2/GL_RENDERER)
   :version                  (.glGetString gl GL2/GL_VERSION)
   :shading-language-version (.glGetString gl GL2/GL_SHADING_LANGUAGE_VERSION)
   :glcontext-class          (class gl)})

(defn- griddata []
  (let [gd (GridData. SWT/FILL SWT/FILL true true)]
    (set! (. gd widthHint) SWT/DEFAULT)
    (set! (. gd heightHint) SWT/DEFAULT)
    gd))

(defn gldata []
  (let [d (GLData.)]
    (set! (. d doubleBuffer) true)
    (set! (. d depthSize) 24)
    d))

(defn glcanvas ^GLCanvas
  [parent]
  (doto (GLCanvas. parent (bit-or SWT/NO_REDRAW_RESIZE SWT/NO_BACKGROUND) (gldata))
    (.setLayoutData (griddata))))

(defn glfactory ^GLDrawableFactory
  []
  (GLDrawableFactory/getFactory (GLProfile/getGL2ES2)))

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
    (.put names ^ints (into-array Integer/TYPE bufs))
    (.glDeleteBuffers gl (count bufs) ^IntBuffer names)))

(defmacro gl-polygon-mode [gl face mode] `(.glPolygonMode ~gl ~face ~mode))

(defmacro gl-get-attrib-location [gl shader name]                        `(.glGetAttribLocation ~gl ~shader ~name))
(defmacro gl-bind-buffer [gl type name]                                  `(.glBindBuffer ~gl ~type ~name))
(defmacro gl-buffer-data [gl type size data usage]                       `(.glBufferData ~gl ~type ~size ~data ~usage))
(defmacro gl-vertex-attrib-pointer [gl idx size type norm stride offset] `(.glVertexAttribPointer ~gl ~idx ~size ~type ~norm ~stride ~offset))
(defmacro gl-enable-vertex-attrib-array [gl idx]                         `(.glEnableVertexAttribArray ~gl ~idx))
(defmacro gl-disable-vertex-attrib-array [gl idx]                        `(.glDisableVertexAttribArray ~gl ~idx))
(defmacro gl-use-program [gl idx]                                        `(.glUseProgram ~gl ~idx))
(defmacro gl-enable [gl cap]                                             `(.glEnable ~gl ~cap))
(defmacro gl-disable [gl cap]                                            `(.glDisable ~gl ~cap))
(defmacro gl-cull-face [gl mode]                                         `(.glCullFace ~gl ~mode))

(defn text-renderer [font-name font-style font-size]
  (TextRenderer. (Font. font-name font-style font-size) true true))

(defn gl-clear [^GL2 gl r g b a]
  (.glDepthMask gl true)
  (.glEnable gl GL/GL_DEPTH_TEST)
  (.glClearColor gl r g b a)
  (.glClear gl (bit-or GL/GL_STENCIL_BUFFER_BIT GL/GL_COLOR_BUFFER_BIT GL/GL_DEPTH_BUFFER_BIT))
  (.glDisable gl GL/GL_DEPTH_TEST)
  (.glDepthMask gl false))

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

(defmacro with-context
  [ctx bindings & body]
  (assert (= 2 (count bindings)) "Bindings vector must provide names to bind to the GL2 and GLU objects")
  `(try
     (.makeCurrent ~ctx)
     (let [~(first bindings)  (.. ~ctx getGL getGL2)
           ~(second bindings) (GLU.)]
       ~@body)
     (finally
       (.release ~ctx))))

(defmacro gl-push-matrix [gl & body]
  `(try
     (.glPushMatrix ~gl)
     ~@body
     (finally
       (.glPopMatrix ~gl))))

(defn gl-load-matrix-4d [^GL2 gl ^Matrix4d mat]
  (let [fbuf (float-array [(.m00 mat) (.m10 mat) (.m20 mat) (.m30 mat)
                           (.m01 mat) (.m11 mat) (.m21 mat) (.m31 mat)
                           (.m02 mat) (.m12 mat) (.m22 mat) (.m32 mat)
                           (.m03 mat) (.m13 mat) (.m23 mat) (.m33 mat)])]
    (.glLoadMatrixf gl fbuf 0)))

(defn gl-mult-matrix-4d [^GL2 gl ^Matrix4d mat]
  (let [fbuf (float-array [(.m00 mat) (.m10 mat) (.m20 mat) (.m30 mat)
                           (.m01 mat) (.m11 mat) (.m21 mat) (.m31 mat)
                           (.m02 mat) (.m12 mat) (.m22 mat) (.m32 mat)
                           (.m03 mat) (.m13 mat) (.m23 mat) (.m33 mat)])]
    (.glMultMatrixf gl fbuf 0)))

(defmacro color
  ([r g b]        `(float-array [(/ ~r 255.0) (/ ~g 255.0) (/ ~b 255.0)]))
  ([r g b a]      `(float-array [(/ ~r 255.0) (/ ~g 255.0) (/ ~b 255.0) a])))

(defmacro gl-color-3f    [gl r g b]     `(.glColor3f ~gl ~r ~g ~b))
(defmacro gl-color-4d    [gl r g b a]   `(.glColor4d ~gl ~r ~g ~b ~a))
(defmacro gl-color-3dv+a [gl dv alpha]  `(gl-color-4d ~gl (first ~dv) (second ~dv) (nth ~dv 2) ~alpha))
(defmacro gl-color-3dv   [gl cv off]    `(.glColor3dv ~gl ~cv ~off))
(defmacro gl-color-3fv   [gl cv off]    `(.glColor3fv ~gl ~cv ~off))

(defmacro gl-vertex-2f   [gl x y]       `(.glVertex2f ~gl ~x ~y))
(defmacro gl-vertex-3d   [gl x y z]     `(.glVertex3d ~gl ~x ~y ~z))
(defmacro gl-vertex-3dv  [gl vtx off]   `(.glVertex3dv ~gl ~vtx ~off))

(defmacro gl-translate-f [gl x y z]     `(.glTranslatef ~gl ~x ~y ~z))

(defmacro gl-draw-arrays [gl t s c]     `(.glDrawArrays ~gl ~t ~s ~c))

(defmacro gl-uniform-matrix-4fv [gl idx cnt transpose val offset] `(.glUniformMatrix4fv ~gl ~idx ~cnt ~transpose ~val ~offset))

(defmacro glu-ortho [glu region]
  `(.gluOrtho2D ~glu (double (.left ~region)) (double (.right ~region)) (double (.bottom ~region)) (double (.top ~region))))

(defn ^"[I" viewport-array [viewport]
  (int-array [(:left viewport)
              (:top viewport)
              (:right viewport)
              (:bottom viewport)]))

(defmacro glu-pick-matrix [glu pick-rect viewport]
  `(let [pick-rect# ~pick-rect]
     (.gluPickMatrix ~glu
       (double (:x pick-rect#))
       (double (:y pick-rect#))
       (double (:width pick-rect#))
       (double (:height pick-rect#))
       (viewport-array ~viewport)
       (int 0))))

(defmacro do-gl
  [bindings & body]
  (assert (even? (count bindings)) "Bindings must contain an even number of forms")
  (let [bound-syms (map first (partition 2 bindings))]
    `(let ~bindings
       (doseq [b# ~(into [] bound-syms)]
         (when (satisfies? p/GlEnable b#)
           (p/enable b#)))
       (let [rval# (do ~@body)]
           (doseq [b# ~(into [] (reverse bound-syms))]
             (when (satisfies? p/GlDisable b#)
               (p/disable b#)))
           rval#))))

(defn overlay
  [ctx ^GL2 gl ^TextRenderer text-renderer ^String chars ^Float xloc ^Float yloc ^Float scalex ^Float scaley]
  (gl-push-matrix gl
    (.glScaled gl 1 -1 1)
    (.setColor text-renderer 1 1 1 1)
    (.begin3DRendering text-renderer)
    (.draw3D text-renderer chars xloc yloc scalex scaley)
    (.end3DRendering text-renderer)))

(defn select-buffer-names
  [hit-count ^IntBuffer select-buffer]
  "Returns a collection of names from a GL_SELECT buffer.
  Names are integers assigned during rendering with glPushName and glPopName.
  The select-buffer contains a series of 'hits' where each hit
  is [name-count min-z max-z & names+]. In our usage, names may not be
  nested so name-count must always be one."
  (loop [i 0
         ptr 0
         names []]
    (if (< i hit-count)
      (let [name-count (.get select-buffer ptr)
            name (.get select-buffer (+ ptr 3))]
        (assert (= 1 name-count) "Count of names in a hit record must be one")
        (recur (inc i)
               (+ ptr 3 name-count)
               (conj names name)))
      names)))
