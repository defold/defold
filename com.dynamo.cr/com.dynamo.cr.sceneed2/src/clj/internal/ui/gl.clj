(ns internal.ui.gl
  (:import [java.awt Font]
           [javax.media.opengl GL GL2 GLDrawableFactory GLProfile]
           [javax.media.opengl.glu GLU]
           [org.eclipse.swt SWT]
           [org.eclipse.swt.layout GridData]
           [org.eclipse.swt.opengl GLData GLCanvas]
           [com.jogamp.opengl.util.awt TextRenderer]))

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

(defn glcanvas [parent]
  (doto (GLCanvas. parent (bit-or SWT/NO_REDRAW_RESIZE SWT/NO_BACKGROUND) (gldata))
    (.setLayoutData (griddata))))

(defn glfactory []
  (GLDrawableFactory/getFactory (GLProfile/getGL2ES1)))

(defn text-renderer [font-name font-style font-size]
  (TextRenderer. (Font. font-name font-style font-size) true true))

(defn gl-clear [gl r g b a]
  (.glDepthMask gl true)
  (.glEnable gl GL/GL_DEPTH_TEST)
  (.glClearColor gl r g b a)
  (.glClear gl (+ GL/GL_COLOR_BUFFER_BIT GL/GL_DEPTH_BUFFER_BIT))
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

(defmacro with-context [ctx bindings & body]
  (assert (= 2 (count bindings)) "Bindings vector must provide names to bind to the GL2 and GLU objects")
  `(try
     (.makeCurrent ~ctx)
     (let [~(first bindings) (.. ~ctx getGL getGL2)
           ~(second bindings) (GLU.)]
       ~@body)
     (finally
       (.release ~ctx))))

(defmacro gl-color-3fv [gl cv off] `(.glColor3fv ~gl ~cv ~off))
(defmacro gl-vertex-2f [gl x y] `(.glVertex2f ~gl ~x ~y))
