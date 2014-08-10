(ns internal.ui.gl
  (:import [java.awt Font]
           [javax.media.opengl GLDrawableFactory GLProfile]
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
