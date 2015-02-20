(ns editor.scene-editor
  (:require [clojure.java.io :as io]
            [schema.core :as s]
            [internal.clojure :as clojure]
            [editor.input :as i]
            [editor.camera :as c]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [dynamo.file :as f]
            [dynamo.project :as p]
            [dynamo.geom :as g]
            [dynamo.gl :as gl]
            [dynamo.background :as background]
            [dynamo.grid :as grid]
            [internal.system :as is]
            [internal.transaction :as it]
            [internal.disposal :as disp]
            [internal.render.pass :as pass]
            [service.log :as log]
            [plumbing.core :refer [fnk defnk]]
            [dynamo.types :refer [IDisposable dispose]]
            )
  (:import  [com.defold.editor Start UIUtil]
            [java.io File]
            [java.lang Runnable System]
            [java.nio.file Paths]
            [java.awt Font]
            [java.awt.image BufferedImage]
            [javafx.animation AnimationTimer]
            [javafx.application Platform]
            [javafx.beans.value ChangeListener]
            [javafx.fxml FXMLLoader]
            [javafx.collections FXCollections ObservableList]
            [javafx.geometry BoundingBox]
            [javafx.scene Scene Node Parent]
            [javafx.stage Stage FileChooser]
            [javafx.scene.image Image ImageView WritableImage PixelWriter]
            [javafx.scene.input MouseEvent]
            [javafx.event ActionEvent EventHandler]
            [javafx.scene.control Button TitledPane TextArea TreeItem Menu MenuItem MenuBar Tab ProgressBar]
            [javafx.scene.layout AnchorPane StackPane HBox Priority]
            [javafx.embed.swing SwingFXUtils]
            [javax.vecmath Point3d Matrix4d Vector4d Matrix3d Vector3d]
            [javax.media.opengl GL GL2 GLContext GLProfile GLAutoDrawable GLDrawableFactory GLCapabilities]
            [javax.media.opengl.glu GLU]
            [com.jogamp.opengl.util.awt TextRenderer Screenshot]
            [dynamo.types Camera AABB Region]
            ))

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

(defn z-distance [camera obj]
  (let [p (->> (Point3d.)
            (g/world-space obj)
            (c/camera-project camera (:viewport camera)))]
    (long (* Integer/MAX_VALUE (.z p)))))

(defn render-key [camera obj]
  (+ (z-distance camera obj)
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
  [^GLContext context ^GL2 gl ^GLU glu ^TextRenderer text-renderer pass renderable]
  (gl/gl-push-matrix
    gl
    (when (t/model-transform? pass)
      (gl/gl-mult-matrix-4d gl (:world-transform renderable)))
    (try
      (when (:render-fn renderable)
        ((:render-fn renderable) context gl glu text-renderer))
      (catch Exception e
        (log/error :exception e
                   :pass pass
                   :renderable renderable
                   :message "skipping renderable")))))

(defrecord DrawableRef [^GLAutoDrawable drawable]
  clojure.lang.IDeref
  (deref [this] drawable)
  IDisposable
  (dispose [this]
    (prn "disposing drawable")
    (.destroy drawable)))

(defmethod print-method DrawableRef
  [^DrawableRef v ^java.io.Writer w]
  (.write w (str "<DrawableRef@" (:drawable v) ">")))

(defrecord TextRendererRef [^TextRenderer text-renderer]
  clojure.lang.IDeref
  (deref [this] text-renderer)
  IDisposable
  (dispose [this]
    (prn "disposing text-renderer")
    (.dispose text-renderer)))

(defmethod print-method TextRendererRef
  [^TextRendererRef v ^java.io.Writer w]
  (.write w (str "<TextRendererRef@" (:text-renderer v) ">")))

(defnk produce-frame [^Region viewport ^DrawableRef drawable camera ^TextRendererRef text-renderer renderables]
  (when (vp-not-empty? viewport)
    (let [^GLContext context (.getContext ^GLAutoDrawable @drawable)]
      (.makeCurrent context)
      (let [gl ^GL2 (.getGL context)
            glu ^GLU (GLU.)]
        (try
          (.glClearColor gl 0.0 0.0 0.0 1.0)
          (gl/gl-clear gl 0.0 0.0 0.0 1)
          (.glColor4f gl 1.0 1.0 1.0 1.0)
          (gl-viewport gl viewport)
          (when-let [renderables (apply merge-with (fn [a b] (reverse (sort-by #(render-key camera %) (concat a b)))) renderables)]
            (doseq [pass pass/render-passes]
              (setup-pass context gl glu pass camera viewport)
              (doseq [node (get renderables pass)]
                (render-node context gl glu @text-renderer pass node))))
          (let [[w h] (vp-dims viewport)
                  buf-image (Screenshot/readToBufferedImage w h)]
              (.release context)
              buf-image))))))

(defnk produce-drawable [^Region viewport]
  (when (vp-not-empty? viewport)
    (let [[w h] (vp-dims viewport)
          profile (GLProfile/getGL2ES2)
          factory (GLDrawableFactory/getFactory profile)
          caps (GLCapabilities. profile)]
      (.setOnscreen caps false)
      (.setPBuffer caps true)
      (.setDoubleBuffered caps false)
      (->DrawableRef (.createOffscreenAutoDrawable factory nil caps nil w h nil))
      )))

(n/defnode SceneRenderer
  (input viewport Region)
  (input camera Camera)
  (input renderables [t/RenderData])

  (output drawable DrawableRef :cached produce-drawable)
  (output text-renderer TextRendererRef :cached (fnk [] (->TextRendererRef (gl/text-renderer Font/SANS_SERIF Font/BOLD 12))))
  (output frame BufferedImage produce-frame) ; TODO cache when the cache bug is fixed
  )

(n/defnode SceneEditor
  (inherits n/Scope)

  (property image-view ImageView)
  (property viewport Region (default (t/->Region 0 0 0 0)))
  (property repainter AnimationTimer)
  (property visible s/Bool (default true))

  (input frame BufferedImage)
  (input controller `t/Node)

  (output viewport Region (fnk [viewport] viewport))
  (output frame BufferedImage :cached (fnk [visible frame] (when visible frame)))

  (trigger stop-animation :deleted (fn [tx graph self label trigger]
                                     (.stop ^AnimationTimer (:repainter self))))

  (on :create
      (let [image-view (ImageView.)
            parent ^Parent (:parent event)
            tab ^Tab (:tab event)]
        (AnchorPane/setTopAnchor image-view 0.0)
        (AnchorPane/setBottomAnchor image-view 0.0)
        (AnchorPane/setLeftAnchor image-view 0.0)
        (AnchorPane/setRightAnchor image-view 0.0)
        (.add (.getChildren parent) image-view)
        (ds/set-property self :image-view image-view)
        (let [self-ref (t/node-ref self)
              event-handler (reify EventHandler (handle [this e]
                                                  (let [self @self-ref
                                                        controller (ds/node-feeding-into self :controller)]
                                                    (n/dispatch-message controller :input :action (i/action-from-jfx e)))))
              change-listener (reify ChangeListener (changed [this observable old-val new-val]
                                                      (let [self @self-ref
                                                            bb ^BoundingBox new-val
                                                            w (- (.getMaxX bb) (.getMinX bb))
                                                            h (- (.getMaxY bb) (.getMinY bb))]
                                                        (ds/transactional (ds/set-property self :viewport (t/->Region 0 w 0 h))))))]
          (.setOnMousePressed parent event-handler)
          (.setOnMouseReleased parent event-handler)
          (.setOnMouseClicked parent event-handler)
          (.setOnMouseMoved parent event-handler)
          (.setOnMouseDragged parent event-handler)
          (.setOnScroll parent event-handler)
          (.addListener (.boundsInParentProperty (.getParent parent)) change-listener)
          (let [repainter (proxy [AnimationTimer] []
                            (handle [now]
                              (let [self @self-ref
                                    image-view ^ImageView (:image-view self)
                                    frame (n/get-node-value self :frame)
                                    visible (:visible self)]
                                (when frame
                                  (.setImage image-view (SwingFXUtils/toFXImage frame (.getImage image-view)))))))]
            (ds/transactional (ds/set-property self :repainter repainter))
            (.start repainter))
          (.addListener (.selectedProperty tab) (reify ChangeListener (changed [this observable old-val new-val]
                                                                        (let [self @self-ref]
                                                                          (ds/transactional (ds/set-property self :visible new-val))))))
          )))

  t/IDisposable
  (dispose [self]
           (prn "Disposing SceneEditor")
           )
  )

(defn construct-scene-editor
  [project-node node]
  (let [editor (n/construct SceneEditor)]
    (ds/in (ds/add editor)
           (let [renderer     (ds/add (n/construct SceneRenderer))
                 background   (ds/add (n/construct background/Gradient))
                 grid         (ds/add (n/construct grid/Grid))
                 camera       (ds/add (n/construct c/CameraController :camera (c/make-camera :orthographic)))]
             (ds/connect background   :renderable      renderer     :renderables)
             (ds/connect grid         :renderable      renderer     :renderables)
             (ds/connect camera       :camera          grid         :camera)
             (ds/connect camera       :camera          renderer     :camera)
             (ds/connect camera       :self            editor       :controller)
             (ds/connect editor       :viewport        camera       :viewport)
             (ds/connect editor       :viewport        renderer     :viewport)
             (ds/connect renderer     :frame           editor       :frame)
             )
           editor)))
