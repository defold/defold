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
            )
  (:import  [com.defold.editor Start UIUtil]
            [java.io File]
            [java.lang Runnable System]
            [java.nio.file Paths]
            [java.awt Font]
            [java.awt.image BufferedImage]
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

(defmacro on-app-thread [& body]
  `(if (Platform/isFxApplicationThread)
     (do
       ~@body)
     (Platform/runLater (reify Runnable
                                     (run [this]
                                       ~@body)))))

(defn z-distance [camera obj]
  (let [p (->> (Point3d.)
            (g/world-space obj)
            (c/camera-project camera (:viewport camera)))]
    (long (* Integer/MAX_VALUE (.z p)))))

(defn render-key [camera obj]
  (+ (z-distance camera obj)
     (bit-shift-left (or (:index obj) 0)        INDEX_SHIFT)
     (bit-shift-left (or (:manipulator? obj) 0) MANIPULATOR_SHIFT)))

(defn gl-viewport [^GL2 gl camera]
  (let [viewport ^Region (:viewport camera)]
    (.glViewport gl (:left viewport) (:top viewport) (- (:right viewport) (:left viewport)) (- (:bottom viewport) (:top viewport)))))

(defn make-drawable [w h]
  )

(defn setup-pass
  ([context gl glu pass camera]
    (setup-pass context gl glu pass camera nil))
  ([context ^GL2 gl ^GLU glu pass camera pick-rect]
    (let [viewport ^Region (:viewport camera)]
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
      (pass/prepare-gl pass gl glu))))

(defn render-node
  [context gl glu text-renderer pass renderable]
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

(defn create-drawable [w h]
  (let [profile (GLProfile/getGL2ES2)
        factory (GLDrawableFactory/getFactory profile)
        caps (GLCapabilities. profile)]
    (.setOnscreen caps false)
    (.setPBuffer caps true)
    (.setDoubleBuffered caps false)
    (.createOffscreenAutoDrawable factory nil caps nil w h nil)
    )
  )

(defn resize-image [self resize image w h]
  (if resize
    (let [image-view ^ImageView (:image-view self)
          new-image (WritableImage. w h)]
      (.setImage image-view new-image)
      new-image)
    image)
  )

(do
  (let [a 1])
  (let [b 2]
    b))

(defn resize-drawable [self resize ^GLAutoDrawable drawable w h]
  (if resize
    (let [old-w (.getWidth drawable)
          old-h (.getHeight drawable)]
     (let [context (.getContext drawable)]
       (when (and context (.isCurrent context)) (.release context))
       (.destroy drawable))
     (let [new-drawable (create-drawable w h)]
       (ds/transactional (ds/set-property self :drawable new-drawable))
       new-drawable))
    drawable))

(defn resize-camera [self resize camera w h]
  (if resize
    (let [viewport (t/->Region 0 w 0 h)
          aspect (/ (double w) h)
          camera-node (ds/node-feeding-into self :view-camera)
          [camera] (n/get-node-inputs self :camera)
          new-camera (-> camera
                       (c/set-orthographic (:fov camera) aspect -100000 100000)
                       (assoc :viewport viewport))]
      (ds/transactional (ds/set-property camera-node :camera new-camera))
      new-camera
      )
    camera))

(defnk produce-frame [self ^ImageView image-view ^Region viewport drawable camera text-renderer]
  (when (and viewport (.getContext drawable))
    (let [w (- (.right viewport) (.left viewport))
          h (- (.bottom viewport) (.top viewport))]
      (when (and (> w 0) (> h 0))
        (let [image (.getImage image-view)
            needs-resize (or (not= w (.getWidth image)) (not= h (.getHeight image)))
            drawable ^GLAutoDrawable (resize-drawable self needs-resize drawable w h)
            camera (resize-camera self needs-resize (first (n/get-node-inputs self :camera)) w h)
            context (.getContext drawable)]
        (.makeCurrent context)
        (let [gl ^GL2 (.getGL context)
              glu ^GLU (GLU.)]
          (try
            (.glClearColor gl 0.0 0.0 0.0 1.0)
            (gl/gl-clear gl 0.0 0.0 0.0 1)
            (.glColor4f gl 1.0 1.0 1.0 1.0)
            (gl-viewport gl camera)
            (when-let [renderables (n/get-node-value self :render-data)]
              (doseq [pass pass/render-passes]
                (setup-pass context gl glu pass camera)
                (doseq [node (get renderables pass)]
                  (render-node context gl glu text-renderer pass node))))
            (finally
              (let [buf-image (Screenshot/readToBufferedImage w h true)]
                (.release context)
                 (on-app-thread
                   (let [image (resize-image self needs-resize image w h)]
                     (SwingFXUtils/toFXImage buf-image image)))
                buf-image)))))))))

(defn render-to-image [self-ref]
  (let [self @self-ref
        image-view ^ImageView (:image-view self)
        image (.getImage image-view)
        parent ^javafx.scene.layout.Region (.getParent (.getParent (.getParent image-view)))
        w (.getWidth parent)
        h (.getHeight parent)
        needs-resize (or (not= w (.getWidth image)) (not= h (.getHeight image)))
        drawable ^GLAutoDrawable (resize-drawable self needs-resize (:drawable self) w h)
        image (resize-image self needs-resize image w h)
        camera (resize-camera self needs-resize (first (n/get-node-inputs self :camera)) w h)
        text-renderer (:text-renderer self)
        context (.getContext drawable)]
    (.makeCurrent context)
    (let [gl ^GL2 (.getGL context)
          glu ^GLU (GLU.)]
      (try
        (.glClearColor gl 0.0 0.0 0.0 1.0)
        (gl/gl-clear gl 0.0 0.0 0.0 1)
        (.glColor4f gl 1.0 1.0 1.0 1.0)
        (gl-viewport gl camera)
        (when-let [renderables (n/get-node-value self :render-data)]
          (doseq [pass pass/render-passes]
            (setup-pass context gl glu pass camera)
            (doseq [node (get renderables pass)]
              (render-node context gl glu text-renderer pass node))))
        (finally
          (let [buf-image (Screenshot/readToBufferedImage w h true)]
            (.release context)
            (SwingFXUtils/toFXImage buf-image image)
            ))))))

(defnk produce-render-data :- t/RenderData
  [self renderables :- [t/RenderData] camera :- Camera]
  (assert camera (str "No camera is available as an input to Scene Renderer (node:" (:_id self) "."))
    (apply merge-with (fn [a b] (reverse (sort-by #(render-key camera %) (concat a b)))) renderables))

(n/defnode SceneEditor
  (inherits n/Scope)

  (property drawable GLAutoDrawable)
  (property text-renderer TextRenderer)
  (property first-resize s/Bool (default true) (visible false))
  (property image-view ImageView)
  (property viewport (atom Region))

  (input view-camera Camera)
  (input renderables [t/RenderData])
  (input aabb        AABB)

  (output aabb AABB (fnk [aabb] aabb))
  (output viewport Region (fnk [viewport] viewport))
  (output glcontext GLContext (fnk [context] context))
  (output render-data t/RenderData produce-render-data)
  (output camera Camera (fnk [view-camera] view-camera))
  (output frame BufferedImage :on-update produce-frame)

  t/Frame
  (frame [self]
    #_(let [[camera] (n/get-node-inputs self :view-camera)]
       (render-to-image self
                       (:drawable self)
                       camera
                       (:text-renderer self)
                       (.getImage (:image-view self)))))

  (on :create
      (let [image-view (ImageView.)
            parent ^Parent (:parent event)
            w (max 1 (.getWidth parent))
            h (max 1 (.getHeight parent))
            image (WritableImage. w h)]
        (.setImage image-view image)
        (AnchorPane/setTopAnchor image-view 0.0)
        (AnchorPane/setBottomAnchor image-view 0.0)
        (AnchorPane/setLeftAnchor image-view 0.0)
        (AnchorPane/setRightAnchor image-view 0.0)
        (.add (.getChildren parent) image-view)
        (let [text-renderer (gl/text-renderer Font/SANS_SERIF Font/BOLD 12)
              drawable (create-drawable w h)
              self-ref (t/node-ref self)
              event-handler (reify EventHandler (handle [this e]
                                                  (let [self @self-ref
                                                        camera-node (ds/node-feeding-into self :view-camera)]
                                                    (n/dispatch-message camera-node :input :action (i/action-from-jfx e)))))
              change-listener (reify ChangeListener (changed [this observable old-val new-val]
                                                      (let [self @self-ref
                                                            bb ^BoundingBox new-val
                                                            w (- (.getMaxX bb) (.getMinX bb))
                                                            h (- (.getMaxY bb) (.getMinY bb))] 
                                                        (ds/transactional (ds/set-property self :viewport (t/->Region 0 w 0 h))))))]
          (ds/set-property self :drawable drawable)
          (ds/set-property self :text-renderer text-renderer)
          (ds/set-property self :image-view image-view)
          (.setOnMousePressed parent event-handler)
          (.setOnMouseReleased parent event-handler)
          (.setOnMouseClicked parent event-handler)
          (.setOnMouseMoved parent event-handler)
          (.setOnMouseDragged parent event-handler)
          (.setOnScroll parent event-handler)
          (.addListener (.boundsInParentProperty (.getParent parent)) change-listener)
          (n/dispatch-message self :repaint)
          )))

  t/IDisposable
  (dispose [self]
           (.dispose (:text-renderer self))
           (.destroy (:drawable self)))

  (on :repaint
      ; TODO hack to deal with resizing
      #_(defer (render-to-image (t/node-ref self)))
      )
  )

(defn construct-scene-editor
  [project-node node]
  (let [editor (n/construct SceneEditor)]
    (ds/in (ds/add editor)
           (let [background   (ds/add (n/construct background/Gradient))
                 grid         (ds/add (n/construct grid/Grid))
                 camera       (ds/add (n/construct c/CameraController :camera (c/make-camera :orthographic)))]
             (ds/connect background   :renderable      editor       :renderables)
             (ds/connect camera       :camera          grid         :camera)
             (ds/connect camera       :camera          editor       :view-camera)
             (ds/connect grid         :renderable      editor       :renderables)
             )
           editor)))
