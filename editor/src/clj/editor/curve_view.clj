(ns editor.curve-view
  (:require [clojure.set :as set]
            [dynamo.graph :as g]
            [editor.background :as background]
            [editor.camera :as c]
            [editor.scene-selection :as selection]
            [editor.colors :as colors]
            [editor.core :as core]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.grid :as grid]
            [editor.input :as i]
            [editor.math :as math]
            [editor.defold-project :as project]
            [util.profiler :as profiler]
            [editor.scene-cache :as scene-cache]
            [editor.scene-tools :as scene-tools]
            [editor.types :as types]
            [editor.ui :as ui]
            [editor.handler :as handler]
            [editor.workspace :as workspace]
            [editor.gl.pass :as pass]
            [editor.ui :as ui]
            [editor.scene :as scene]
            [service.log :as log])
  (:import [com.defold.editor Start UIUtil]
           [com.jogamp.opengl.util GLPixelStorageModes]
           [com.jogamp.opengl.util.awt TextRenderer]
           [editor.types Camera AABB Region Rect]
           [java.awt Font]
           [java.awt.image BufferedImage DataBufferByte DataBufferInt]
           [javafx.animation AnimationTimer]
           [javafx.application Platform]
           [javafx.beans.value ChangeListener]
           [javafx.collections FXCollections ObservableList]
           [javafx.embed.swing SwingFXUtils]
           [javafx.event ActionEvent EventHandler]
           [javafx.geometry BoundingBox Pos VPos HPos]
           [javafx.scene Scene Group Node Parent]
           [javafx.scene.control Tab Button]
           [javafx.scene.image Image ImageView WritableImage PixelWriter]
           [javafx.scene.input MouseEvent]
           [javafx.scene.layout AnchorPane Pane StackPane]
           [java.lang Runnable Math]
           [java.nio IntBuffer ByteBuffer ByteOrder]
           [javax.media.opengl GL GL2 GL2GL3 GLContext GLProfile GLAutoDrawable GLOffscreenAutoDrawable GLDrawableFactory GLCapabilities]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Point2i Point3d Quat4d Matrix4d Vector4d Matrix3d Vector3d]
           [sun.awt.image IntegerComponentRaster]
           [java.util.concurrent Executors]
           [com.defold.editor AsyncCopier]))

(set! *warn-on-reflection* true)

(defn gl-viewport [^GL2 gl viewport]
  (.glViewport gl (:left viewport) (:top viewport) (- (:right viewport) (:left viewport)) (- (:bottom viewport) (:top viewport))))

(defn- render! [^Region viewport ^GLAutoDrawable drawable camera renderables ^GLContext context ^GL2 gl]
  (let [render-args (scene/generic-render-args viewport camera)]
    (gl/gl-clear gl 0.0 0.0 0.0 1)
    (.glColor4f gl 1.0 1.0 1.0 1.0)
    (gl-viewport gl viewport)
    (doseq [pass pass/render-passes
            :let [render-args (assoc render-args :pass pass)]]
      (scene/setup-pass context gl pass camera viewport)
      (scene/batch-render gl render-args (get renderables pass) false :batch-key))))

(g/defnk produce-async-frame [^Region viewport ^GLAutoDrawable drawable camera renderables ^AsyncCopier async-copier]
  (when async-copier
    #_(profiler/profile "updatables" -1
                       ; Fixed dt for deterministic playback
                       (let [dt 1/60
                             context {:dt (if (= play-mode :playing) dt 0)}]
                         (doseq [updatable active-updatables
                                 :let [node-path (:node-path updatable)
                                       context (assoc context
                                                      :world-transform (:world-transform updatable))]]
                           ((get-in updatable [:updatable :update-fn]) context))))
    (profiler/profile "render" -1
                      (when-let [^GLContext context (.getContext drawable)]
                        (let [gl ^GL2 (.getGL context)]
                          (.beginFrame async-copier gl)
                          (render! viewport drawable camera renderables context gl)
                          (.endFrame async-copier gl)
                          :ok)))))

(g/defnode CurveView
  (inherits scene/SceneRenderer)

  (property image-view ImageView)
  (property viewport Region (default (types/->Region 0 0 0 0)))
  (property drawable GLAutoDrawable)
  (property async-copier AsyncCopier)
  (property tool-picking-rect Rect)
  (property selected-tool-renderables g/Any)

  (input camera-id g/NodeID :cascade-delete)
  (input grid-id g/NodeID :cascade-delete)
  (input background-id g/NodeID :cascade-delete)
  (input input-handlers Runnable :array)

  (output async-frame g/Keyword :cached produce-async-frame))

(defonce view-state (atom nil))

(defn- update-image-view! [^ImageView image-view dt]
  #_(profiler/begin-frame)
  (let [view-id (ui/user-data image-view ::view-id)
        view-graph (g/node-id->graph-id view-id)]
    ;; Explicitly invalidate frame while there are active updatables
    #_(when (not (empty? (g/node-value view-id :active-updatables)))
       (g/invalidate! [[view-id :async-frame]]))
    #_(scene-cache/prune-object-caches! nil)
    (try
      (g/node-value view-id :async-frame)
      (catch Exception e
        (.setImage image-view nil)
        (throw e)))))

(defn make-gl-pane [view-id parent opts]
  (let [image-view (doto (ImageView.)
                     (.setScaleY -1.0))
        pane (proxy [com.defold.control.Region] []
               (layoutChildren []
                 (let [this ^com.defold.control.Region this
                       w (.getWidth this)
                       h (.getHeight this)]
                   (.setFitWidth image-view w)
                   (.setFitHeight image-view h)
                   (proxy-super layoutInArea ^Node image-view 0.0 0.0 w h 0.0 HPos/CENTER VPos/CENTER)
                   (when (and (> w 0) (> h 0))
                     (let [viewport (types/->Region 0 w 0 h)]
                       (g/transact (g/set-property view-id :viewport viewport))
                       (if-let [view-id (ui/user-data image-view ::view-id)]
                         (let [drawable ^GLOffscreenAutoDrawable (g/node-value view-id :drawable)]
                           (doto drawable
                             (.setSize w h))
                           (let [context (scene/make-current drawable)]
                             (doto ^AsyncCopier (g/node-value view-id :async-copier)
                               (.setSize ^GL2 (.getGL context) w h))
                             (.release context)))
                         (do
                           (scene/register-event-handler! parent view-id)
                           (ui/user-data! image-view ::view-id view-id)
                           (let [^Tab tab      (:tab opts)
                                 repainter     (ui/->timer "refresh-curve-view"
                                                (fn [dt]
                                                  (when (.isSelected tab)
                                                    (update-image-view! image-view dt))))]
                             (ui/user-data! parent ::repainter repainter)
                             (ui/on-close tab
                                          (fn [e]
                                            (ui/timer-stop! repainter)
                                            #_(scene-view-dispose view-id)
                                            #_(scene-cache/drop-context! nil true)))
                             (ui/timer-start! repainter))
                           (let [drawable (scene/make-drawable w h)]
                             (g/transact
                              (concat
                               (g/set-property view-id :drawable drawable)
                               (g/set-property view-id :async-copier (scene/make-copier image-view drawable viewport)))))
                           #_(frame-selection view-id false)))))
                   (proxy-super layoutChildren))))]
    (.add (.getChildren pane) image-view)
    (g/set-property! view-id :image-view image-view)
    pane))

(defn destroy-view! [parent]
  (when-let [repainter (ui/user-data parent ::repainter)]
    (ui/timer-stop! repainter)
    (ui/user-data! parent ::repainter nil))
  (when-let [node-id (ui/user-data parent ::node-id)]
    (when-let [scene (g/node-by-id node-id)]
      (when-let [^GLAutoDrawable drawable (g/node-value node-id :drawable)]
        (let [gl (.getGL drawable)]
          (when-let [^AsyncCopier copier (g/node-value node-id :async-copier)]
            (.dispose copier gl))
        (.destroy drawable))))
    (g/transact (g/delete-node node-id))
    (ui/user-data! parent ::node-id nil)
    (ui/children! parent [])))

(defn make-view!
  ([graph ^Parent parent opts]
    (reset! view-state {:graph graph :parent parent :opts opts})
    (make-view! graph parent opts false))
  ([graph ^Parent parent opts reloading?]
    (let [[node-id] (g/tx-nodes-added
                      (g/transact (g/make-nodes graph [view-id    CurveView
                                                       background background/Gradient
                                                       camera     [c/CameraController :local-camera (or (:camera opts) (c/make-camera :orthographic))]
                                                       grid       grid/Grid]
                                                (g/update-property camera :movements-enabled disj :tumble) ; TODO - pass in to constructor
                                                (g/set-graph-value graph :camera camera)

                                                (g/connect camera :_node-id view-id :camera-id)
                                                (g/connect grid :_node-id view-id :grid-id)
                                                (g/connect camera :camera view-id :camera)
                                                (g/connect camera :camera grid :camera)
                                                (g/connect camera :input-handler view-id :input-handlers)
                                                (g/connect view-id :viewport camera :viewport)
                                                (g/connect grid :renderable view-id :aux-renderables)
                                                (g/connect background :_node-id view-id :background-id)
                                                (g/connect background :renderable view-id :aux-renderables))))
          ^Node pane (make-gl-pane node-id parent opts)]
      (ui/fill-control pane)
      (ui/children! parent [pane])
      (ui/user-data! parent ::node-id node-id)
      node-id)))

(defn- reload-curve-view []
  (when @view-state
    (let [{:keys [graph ^Parent parent opts]} @view-state]
      (ui/run-now
        (destroy-view! parent)
        (make-view! graph parent opts true)))))

(reload-curve-view)
