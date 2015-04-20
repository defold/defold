(ns editor.scene
  (:require [dynamo.background :as background]
            [dynamo.geom :as geom]
            [dynamo.gl :as gl]
            [dynamo.graph :as g]
            [dynamo.grid :as grid]
            [dynamo.types :as t]
            [dynamo.types :refer [IDisposable dispose]]
            [editor.camera :as c]
            [editor.core :as core]
            [editor.input :as i]
            [editor.workspace :as workspace]
            [editor.math :as math]
            [internal.render.pass :as pass]
            [service.log :as log])
  (:import [com.defold.editor Start UIUtil]
           [com.jogamp.opengl.util.awt TextRenderer Screenshot]
           [dynamo.types Camera AABB Region]
           [java.awt Font]
           [java.awt.image BufferedImage]
           [javafx.animation AnimationTimer]
           [javafx.application Platform]
           [javafx.beans.value ChangeListener]
           [javafx.collections FXCollections ObservableList]
           [javafx.embed.swing SwingFXUtils]
           [javafx.event ActionEvent EventHandler]
           [javafx.geometry BoundingBox]
           [javafx.scene Scene Node Parent]
           [javafx.scene.control Tab]
           [javafx.scene.image Image ImageView WritableImage PixelWriter]
           [javafx.scene.input MouseEvent]
           [javafx.scene.layout AnchorPane Pane]
           [java.lang Runnable]
           [javax.media.opengl GL GL2 GLContext GLProfile GLAutoDrawable GLOffscreenAutoDrawable GLDrawableFactory GLCapabilities]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Point3d Quat4d Matrix4d Vector4d Matrix3d Vector3d]))

(set! *warn-on-reflection* true)

(def PASS_SHIFT        32)
(def INDEX_SHIFT       (+ PASS_SHIFT 4))
(def MANIPULATOR_SHIFT 62)

(defn vp-dims [^Region viewport]
  (let [w (- (.right viewport) (.left viewport))
        h (- (.bottom viewport) (.top viewport))]
    [w h]))

(defn vp-not-empty? [^Region viewport]
  (let [[w h] (vp-dims viewport)]
    (and (> w 0) (> h 0))))

(defn z-distance [camera viewport obj]
  (let [p (->> (Point3d.)
            (geom/world-space obj)
            (c/camera-project camera viewport))]
    (long (* Integer/MAX_VALUE (.z p)))))

(defn render-key [camera viewport obj]
  (+ (z-distance camera viewport obj)
     (bit-shift-left (or (:index obj) 0)        INDEX_SHIFT)
     (bit-shift-left (or (:manipulator? obj) 0) MANIPULATOR_SHIFT)))

(defn gl-viewport [^GL2 gl viewport]
  (.glViewport gl (:left viewport) (:top viewport) (- (:right viewport) (:left viewport)) (- (:bottom viewport) (:top viewport))))

(defn setup-pass
  ([context gl glu pass camera ^Region viewport]
    (setup-pass context gl glu pass camera viewport nil))
  ([context ^GL2 gl ^GLU glu pass camera ^Region viewport pick-rect]
    (.glMatrixMode gl GL2/GL_PROJECTION)
      (.glLoadIdentity gl)
      (when pick-rect
        (gl/glu-pick-matrix glu pick-rect viewport))
      (if (t/model-transform? pass)
        (gl/gl-mult-matrix-4d gl (c/camera-projection-matrix camera))
        (gl/glu-ortho glu viewport))
      (.glMatrixMode gl GL2/GL_MODELVIEW)
      (.glLoadIdentity gl)
      (when (t/model-transform? pass)
        (gl/gl-load-matrix-4d gl (c/camera-view-matrix camera)))
      (pass/prepare-gl pass gl glu)))

(defn render-node
  [^GL2 gl pass renderable render-args]
  (gl/gl-push-matrix
    gl
    (when (t/model-transform? pass)
      (gl/gl-mult-matrix-4d gl (:world-transform renderable)))
    (try
      (when (:render-fn renderable)
        ((:render-fn renderable) render-args))
      (catch Exception e
        (log/error :exception e
                   :pass pass
                   :render-fn (:render-fn renderable)
                   :message "skipping renderable")))))

(defrecord TextRendererRef [^TextRenderer text-renderer ^GLContext context]
  clojure.lang.IDeref
  (deref [this] text-renderer)
  IDisposable
  (dispose [this]
    (prn "disposing text-renderer")
    (when context (.makeCurrent context))
    (.dispose text-renderer)
    (when context (.release context))))

(defmethod print-method TextRendererRef
  [^TextRendererRef v ^java.io.Writer w]
  (.write w (str "<TextRendererRef@" (:text-renderer v) ">")))

(g/defnk produce-drawable [self ^Region viewport]
  (when (vp-not-empty? viewport)
    (let [[w h]   (vp-dims viewport)
          profile (GLProfile/getDefault)
          factory (GLDrawableFactory/getFactory profile)
          caps    (GLCapabilities. profile)]
      (.setOnscreen caps false)
      (.setPBuffer caps true)
      (.setDoubleBuffered caps false)
      (let [^GLOffscreenAutoDrawable drawable (:gl-drawable self)
            drawable (if drawable
                       (do (.setSize drawable w h) drawable)
                       (.createOffscreenAutoDrawable factory nil caps nil w h nil))]
        (g/transact (g/set-property self :gl-drawable drawable))
        drawable))))

(g/defnk produce-frame [^Region viewport ^GLAutoDrawable drawable camera ^TextRendererRef text-renderer renderables]
  (when (and drawable (vp-not-empty? viewport))
    (let [^GLContext context (.getContext drawable)]
      (.makeCurrent context)
      (let [gl ^GL2 (.getGL context)
            glu ^GLU (GLU.)
            render-args {:gl gl :glu glu :camera camera :viewport viewport :text-renderer @text-renderer}]
        (try
          (.glClearColor gl 0.0 0.0 0.0 1.0)
          (gl/gl-clear gl 0.0 0.0 0.0 1)
          (.glColor4f gl 1.0 1.0 1.0 1.0)
          (gl-viewport gl viewport)
          (when-let [renderables (apply merge-with (fn [a b] (reverse (sort-by #(render-key camera viewport %) (concat a b)))) renderables)]
            (doseq [pass pass/render-passes
                    :let [render-args (assoc render-args :pass pass)]]
              (setup-pass context gl glu pass camera viewport)
              (doseq [renderable (get renderables pass)]
                (render-node gl pass renderable render-args))))
          (let [[w h] (vp-dims viewport)
                  buf-image (Screenshot/readToBufferedImage w h)]
              (.release context)
              buf-image))))))

(g/defnode SceneRenderer
  (property name t/Keyword (default :renderer))
  (property gl-drawable GLAutoDrawable)

  (input viewport Region)
  (input camera Camera)
  (input renderables [t/RenderData])

  (output drawable GLAutoDrawable :cached produce-drawable)
  (output text-renderer TextRendererRef :cached (g/fnk [^GLAutoDrawable drawable] (->TextRendererRef (gl/text-renderer Font/SANS_SERIF Font/BOLD 12) (if drawable (.getContext drawable) nil))))
  (output frame BufferedImage :cached produce-frame))

(defn dispatch-input [input-handlers action]
  (reduce (fn [action input-handler]
            (let [node (first input-handler)
                  label (second input-handler)]
              (when action
                ((g/node-value node label) node action))))
          action input-handlers))

(defn- apply-transform [^Matrix4d transform renderables]
  (let [apply-tx (fn [renderable]
                   (let [^Matrix4d world-transform (or (:world-transform renderable) (doto (Matrix4d.) (.setIdentity)))]
                     (assoc renderable :world-transform (doto (Matrix4d. transform)
                                                          (.mul world-transform)))))]
    (into {} (map (fn [[pass lst]] [pass (map apply-tx lst)]) renderables))))

(defn scene->renderables [scene]
  (if (seq? scene)
    (map scene->renderables scene)
    (let [{:keys [id transform renderables children]} scene
          renderables (reduce #(merge-with concat %1 %2) renderables (map scene->renderables children))]
      (if transform
        (apply-transform transform renderables)
        renderables))))

(g/defnode SceneView
  (inherits core/Scope)

  (property image-view ImageView)
  (property viewport Region (default (t/->Region 0 0 0 0)))
  (property repainter AnimationTimer)
  (property visible t/Bool (default true))

  (input frame BufferedImage)
  (input scene t/Any)
  (input input-handlers [Runnable])

  (output renderables t/Any :cached (g/fnk [scene] (scene->renderables scene)))
;;  (output viewport Region (g/fnk [viewport] viewport))
  (output image WritableImage :cached (g/fnk [frame ^ImageView image-view] (when frame (SwingFXUtils/toFXImage frame (.getImage image-view)))))
  (output aabb AABB :cached (g/fnk [scene] (:aabb scene)))

  (trigger stop-animation :deleted (fn [tx graph self label trigger]
                                     (.stop ^AnimationTimer (:repainter self))
                                     nil))
  t/IDisposable
  (dispose [self]
           (prn "Disposing SceneEditor")
           (when-let [^GLAutoDrawable drawable (:gl-drawable self)]
             (.destroy drawable))
           ))

(defn make-scene-view [scene-graph ^Parent parent]
  (let [image-view (ImageView.)]
    (.add (.getChildren ^Pane parent) image-view)
    (let [view (g/make-node! scene-graph SceneView :image-view image-view)]
      (let [self-ref (g/node-id view)
            event-handler (reify EventHandler (handle [this e]
                                                (let [now (g/now)
                                                      self (g/node-by-id now self-ref)]
                                                  (dispatch-input (g/sources-of now self :input-handlers) (i/action-from-jfx e)))))
            change-listener (reify ChangeListener (changed [this observable old-val new-val]
                                                    (let [bb ^BoundingBox new-val
                                                          w (- (.getMaxX bb) (.getMinX bb))
                                                          h (- (.getMaxY bb) (.getMinY bb))]
                                                      (g/transact (g/set-property self-ref :viewport (t/->Region 0 w 0 h))))))]
        (.setOnMousePressed parent event-handler)
        (.setOnMouseReleased parent event-handler)
        (.setOnMouseClicked parent event-handler)
        (.setOnMouseMoved parent event-handler)
        (.setOnMouseDragged parent event-handler)
        (.setOnScroll parent event-handler)
        (.addListener (.boundsInParentProperty (.getParent parent)) change-listener)
        (let [repainter (proxy [AnimationTimer] []
                          (handle [now]
                            (let [self                  (g/node-by-id (g/now) self-ref)
                                  image-view ^ImageView (:image-view self)
                                  visible               (:visible self)]
                              (when (and visible)
                                (let [image (g/node-value self :image)]
                                  (when (not= image (.getImage image-view)) (.setImage image-view image)))))))]
          (g/transact (g/set-property view :repainter repainter))
          (.start repainter)))
      (g/refresh view))))

(g/defnode PreviewView
  (inherits core/Scope)

  (property width t/Num)
  (property height t/Num)
  (input scene t/Any)
  (input frame BufferedImage)
  (input input-handlers [Runnable])
  (output renderables t/Any :cached (g/fnk [scene] (scene->renderables scene)))
  (output image WritableImage :cached (g/fnk [frame] (when frame (SwingFXUtils/toFXImage frame nil))))
  (output viewport Region (g/fnk [width height] (t/->Region 0 width 0 height)))
  (output aabb AABB :cached (g/fnk [scene] (:aabb scene))))

(defn make-preview-view [graph width height]
  (g/make-node! graph PreviewView :width width :height height))

(defn setup-view [view opts]
  (let [view-graph (g/node->graph-id view)]
    (g/make-nodes view-graph
                  [renderer   SceneRenderer
                   background background/Gradient
                   camera     [c/CameraController :camera (or (:camera opts) (c/make-camera :orthographic)) :reframe true]
                   grid       grid/Grid]
                  (g/update-property camera  :movements-enabled disj :tumble) ; TODO - pass in to constructor

                  ; Needed for scopes
;                  (g/connect renderer :self view :nodes)
;                  (g/connect camera :self view :nodes)

                  (g/set-graph-value view-graph :renderer renderer)
                  (g/set-graph-value view-graph :camera   camera)

                  (g/connect background      :renderable    renderer        :renderables)
                  (g/connect camera          :camera        renderer        :camera)
                  (g/connect camera          :input-handler view            :input-handlers)
                  (g/connect view            :aabb          camera          :aabb)
                  (g/connect view            :viewport      camera          :viewport)
                  (g/connect view            :viewport      renderer        :viewport)
                  (g/connect view            :renderables   renderer        :renderables)
                  (g/connect renderer        :frame         view            :frame)

                  (g/connect grid   :renderable renderer :renderables)
                  (g/connect camera :camera     grid     :camera)
                  (when (not (:grid opts))
                    (g/delete-node grid)
                    #_(let [grid (g/make-node! view-graph grid/Grid)]
                       (g/connect grid   :renderable renderer :renderables)
                       (g/connect camera :camera     grid     :camera))))))

(defn make-view [graph ^Parent parent resource-node opts]
  (let [view (make-scene-view graph parent)]
    (g/transact
      (concat
        (setup-view view opts)
        (g/connect resource-node :scene view :scene)))
    view))

(defn make-preview [graph resource-node opts width height]
  (let [view (make-preview-view graph width height)]
    (g/transact
      (concat
        (setup-view view (dissoc opts :grid))
        (g/connect resource-node :scene view :scene)))
    view))

(defn register-view-types [workspace]
                               (workspace/register-view-type workspace
                                                             :id :scene
                                                             :make-view-fn make-view
                                                             :make-preview-fn make-preview))

(g/defnode SceneNode
  (property position t/Vec3 (default [0 0 0]))
  (property rotation t/Vec3 (default [0 0 0]))
  (property scale t/Num (default 1.0)) ; TODO - non-uniform scale

  (output position Vector3d :cached (g/fnk [position] (Vector3d. (double-array position))))
  (output rotation Quat4d :cached (g/fnk [rotation] (math/euler->quat rotation)))
  (output transform Matrix4d :cached (g/fnk [^Vector3d position ^Quat4d rotation ^double scale] (Matrix4d. rotation position scale)))
  (output scene t/Any :cached (g/fnk [self transform] {:id (g/node-id self) :transform transform}))
  (output aabb AABB :cached (g/fnk [] (geom/null-aabb))))
