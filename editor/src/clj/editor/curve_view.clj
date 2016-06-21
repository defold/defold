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
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
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
            [editor.properties :as properties]
            [editor.camera :as camera]
            [util.id-vec :as iv]
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

; Line shader

(vtx/defvertex color-vtx
  (vec3 position)
  (vec4 color))

(shader/defshader line-vertex-shader
  (attribute vec4 position)
  (attribute vec4 color)
  (varying vec4 var_color)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_color color)))

(shader/defshader line-fragment-shader
  (varying vec4 var_color)
  (defn void main []
    (setq gl_FragColor var_color)))

(def line-shader (shader/make-shader ::line-shader line-vertex-shader line-fragment-shader))

(defn gl-viewport [^GL2 gl viewport]
  (.glViewport gl (:left viewport) (:top viewport) (- (:right viewport) (:left viewport)) (- (:bottom viewport) (:top viewport))))

(defn render-curves [^GL2 gl render-args renderables rcount]
  (let [camera (:camera render-args)
        viewport (:viewport render-args)
        #_scale-f #_(scale-factor camera viewport)]
    (doseq [renderable renderables
            :let [screen-tris (get-in renderable [:user-data :screen-tris])
                  world-lines (get-in renderable [:user-data :world-lines])]]
      (when world-lines
        (let [vcount (count world-lines)
              vertex-binding (vtx/use-with ::lines world-lines line-shader)]
          (gl/with-gl-bindings gl render-args [line-shader vertex-binding]
            (gl/gl-draw-arrays gl GL/GL_LINES 0 vcount))))
      (when screen-tris
        (let [vcount (count screen-tris)
              vertex-binding (vtx/use-with ::tris screen-tris line-shader)]
          (gl/with-gl-bindings gl render-args [line-shader vertex-binding]
            (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 vcount)))))))

(defn- render! [^Region viewport ^GLAutoDrawable drawable camera renderables ^GLContext context ^GL2 gl]
  (let [render-args (scene/generic-render-args viewport camera)]
    (gl/gl-clear gl 0.0 0.0 0.0 1)
    (.glColor4f gl 1.0 1.0 1.0 1.0)
    (gl-viewport gl viewport)
    (doseq [pass pass/render-passes
            :let [render-args (assoc render-args :pass pass)]]
      (scene/setup-pass context gl pass camera viewport)
      (scene/batch-render gl render-args (get renderables pass) false :batch-key))))

(g/defnk produce-async-frame [^Region viewport ^GLAutoDrawable drawable camera renderables ^AsyncCopier async-copier curve-renderables cp-renderables tool-renderables]
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
    (profiler/profile "render-curves" -1
                      (when-let [^GLContext context (.getContext drawable)]
                        (let [gl ^GL2 (.getGL context)]
                          (.beginFrame async-copier gl)
                          (render! viewport drawable camera (reduce (partial merge-with into) renderables (into [curve-renderables cp-renderables] tool-renderables)) context gl)
                          (scene-cache/prune-object-caches! gl)
                          (.endFrame async-copier gl)
                          :ok)))))

(defn- curve? [[_ p]]
  (let [v (:value p)]
    (when (satisfies? types/GeomCloud v)
      (< 1 (count (properties/curve-vals v))))))

(g/defnk produce-curve-renderables [curves]
  (let [splines (mapv (fn [{:keys [node-id property curve]}] (->> curve
                                                               (mapv second)
                                                               (properties/->spline))) curves)
        steps 128
        scount (count splines)
        colors (map-indexed (fn [i s] (let [h (* (+ i 0.5) (/ 360.0 scount))
                                            s 1.0
                                            l 0.7]
                                        (colors/hsl->rgba h s l)))
                    splines)
        xs (->> (range steps)
             (map #(/ % (- steps 1)))
             (partition 2 1)
             (apply concat))
        curve-vs (mapcat (fn [spline color](let [[r g b a] color]
                                             (mapv #(let [[x y] (properties/spline-cp spline %)]
                                                      [x y 0.0 r g b a]) xs))) splines colors)
        curve-vcount (count curve-vs)
        world-lines (when (< 0 curve-vcount)
                      (let [vb (->color-vtx curve-vcount)]
                        (doseq [v curve-vs]
                          (conj! vb v))
                        (persistent! vb)))
        renderables [{:render-fn render-curves
                      :batch-key nil
                      :user-data {:world-lines world-lines}}]]
    (into {} (map #(do [% renderables]) [pass/transparent]))))

(g/defnk produce-cp-renderables [curves viewport camera sub-selection]
  (prn "c" curves)
  (prn "sub" sub-selection)
  (let [sub-sel (into {} (mapv (fn [] []) sub-selection))
        scale (camera/scale-factor camera viewport)
        splines (mapv (fn [{:keys [node-id property curve]}]
                        (let [subs (filterv (fn []) sub-selection)]
                          (->> curve
                           (mapv second)
                           (properties/->spline)))) curves)
        scount (count splines)
        colors (map-indexed (fn [i s] (let [h (* (+ i 0.5) (/ 360.0 scount))
                                            s 1.0
                                            l 0.7]
                                        (colors/hsl->rgba h s l)))
                    splines)
        cp-r 4.0
        quad (let [[v0 v1 v2 v3] (vec (for [x [(- cp-r) cp-r]
                                            y [(- cp-r) cp-r]]
                                        [x y 0.0]))]
               (geom/scale scale [v0 v1 v2 v2 v1 v3]))
        cp-vs (mapcat (fn [spline color] (let [[r g b a] color]
                                           (->> spline
                                             (mapcat (fn [[x y tx ty]] (geom/transl [x y 0.0] quad)))
                                             (map (fn [[x y z]] [x y z r g b a])))))
                      splines colors)
        cp-vcount (count cp-vs)
        screen-tris (when (< 0 cp-vcount)
                      (let [vb (->color-vtx cp-vcount)]
                        (doseq [v cp-vs]
                          (conj! vb v))
                        (persistent! vb)))
        renderables [{:render-fn render-curves
                     :batch-key nil
                     :user-data {:screen-tris screen-tris}}]]
    (into {} (map #(do [% renderables]) [pass/transparent]))))

(g/defnk produce-curves [selected-node-properties]
  (let [curves (mapcat (fn [p] (->> (:properties p)
                                 (filter curve?)
                                 (map (fn [[k p]] {:node-id (:node-id p)
                                                   :property k
                                                   :curve (iv/iv-mapv identity (:points (:value p)))}))))
                       selected-node-properties)
        ccount (count curves)
        hue-f (/ 360.0 ccount)
        curves (map-indexed (fn [i c] (assoc c :hue (* (+ i 0.5) hue-f))) curves)]
    curves))

(defn- handle-input [self action user-data]
  (prn "action" action)
  #_(let [viewport                   (g/node-value self :viewport)
         movements-enabled          (g/node-value self :movements-enabled)
         ui-state                   (g/node-value self :ui-state)
         {:keys [last-x last-y]}    @ui-state
         {:keys [x y type delta-y]} action
         movement                   (if (= type :mouse-pressed)
                                      (get movements-enabled (camera-movement action) :idle)
                                      (:movement @ui-state))
         camera                     (g/node-value self :camera)
         filter-fn                  (or (:filter-fn camera) identity)
         camera                     (cond-> camera
                                      (and (= type :scroll)
                                           (contains? movements-enabled :dolly))
                                      (dolly (* -0.002 delta-y))

                                      (and (= type :mouse-moved)
                                           (not (= :idle movement)))
                                      (cond->
                                        (= :dolly movement)
                                        (dolly (* -0.002 (- y last-y)))
                                        (= :track movement)
                                        (track viewport last-x last-y x y)
                                        (= :tumble movement)
                                        (tumble last-x last-y x y))

                                      true
                                      filter-fn)]
     (g/set-property! self :local-camera camera)
     (case type
       :scroll (if (contains? movements-enabled :dolly) nil action)
       :mouse-pressed (do
                        (swap! ui-state assoc :last-x x :last-y y :movement movement)
                        (if (= movement :idle) action nil))
       :mouse-released (do
                         (swap! ui-state assoc :last-x nil :last-y nil :movement :idle)
                         (if (= movement :idle) action nil))
       :mouse-moved (if (not (= :idle movement))
                      (do
                        (swap! ui-state assoc :last-x x :last-y y)
                        nil)
                      action)
       action)))

(g/defnode CurveController
  (output input-handler Runnable :cached (g/fnk [] handle-input)))

(defn- aabb-contains? [^AABB aabb ^Point3d p]
  (let [min-p (types/min-p aabb)
        max-p (types/max-p aabb)]
    (and (<= (.x min-p) (.x p) (.x max-p))
         (<= (.y min-p) (.y p) (.y max-p)))))

(g/defnode CurveView
  (inherits scene/SceneRenderer)

  (property image-view ImageView)
  (property viewport Region (default (types/->Region 0 0 0 0)))
  (property drawable GLAutoDrawable)
  (property async-copier AsyncCopier)
  (property tool-picking-rect Rect)

  (input camera-id g/NodeID :cascade-delete)
  (input grid-id g/NodeID :cascade-delete)
  (input background-id g/NodeID :cascade-delete)
  (input input-handlers Runnable :array)
  (input selected-node-properties g/Any)
  (input tool-renderables pass/RenderData :array)
  (input picking-rect Rect)
  (input sub-selection g/Any)

  (output async-frame g/Keyword :cached produce-async-frame)
  (output curves g/Any :cached produce-curves)
  (output curve-renderables g/Any :cached produce-curve-renderables)
  (output cp-renderables g/Any :cached produce-cp-renderables)
  (output picking-selection g/Any
          (g/fnk [curves picking-rect camera viewport]
                 (let [aabb (geom/rect->aabb picking-rect)]
                   (keep identity
                         (mapv (fn [c]
                                 (let [curve (:curve c)
                                       selected (->> curve
                                                  (filterv (fn [[idx cp]]
                                                             (let [[x y] cp
                                                                   p (doto (->> (Point3d. x y 0.0)
                                                                             (camera/camera-project camera viewport))
                                                                       (.setZ 0.0))]
                                                               (aabb-contains? aabb p))))
                                                  (mapv first))]
                                   (when (not (empty? selected))
                                     [(:node-id c) (:property c) selected])))
                               curves)))))
  (output selected-tool-renderables g/Any :cached (g/fnk [] {})))

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

(defn frame-selection [view animate?]
  (let [graph (g/node-id->graph-id view)
        camera (g/graph-value graph :camera)
        aabb (or (g/node-value view :selected-aabb)
                 (-> (geom/null-aabb)
                   (geom/aabb-incorporate 0.0 0.0 0.0)
                   (geom/aabb-incorporate 1.0 1.0 0.0)))
        viewport (g/node-value view :viewport)
        local-cam (g/node-value camera :local-camera)
        end-camera (c/camera-orthographic-frame-aabb local-cam viewport aabb)]
    (if animate?
      (let [duration 0.5]
        (ui/anim! duration
                  (fn [t] (let [t (- (* t t 3) (* t t t 2))
                                cam (c/interpolate local-cam end-camera t)]
                            (g/transact
                              (g/set-property camera :local-camera cam))))
                  (fn [])))
      (g/transact (g/set-property camera :local-camera end-camera)))))

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
                           (frame-selection view-id false)))))
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
          (scene-cache/drop-context! gl false)
          (.destroy drawable))))
    (g/transact (g/delete-node node-id))
    (ui/user-data! parent ::node-id nil)
    (ui/children! parent [])))

(defn- camera-filter-fn [camera]
  (let [^Point3d p (:position camera)
        y (.y p)
        z (.z p)]
    (assoc camera
           :position (Point3d. 0.5 y z)
           :focus-point (Vector4d. 0.5 y z 1.0)
           :fov-x 1.2)))

(defn make-view!
  ([project graph ^Parent parent opts]
    (reset! view-state {:project project :graph graph :parent parent :opts opts})
    (make-view! project graph parent opts false))
  ([project graph ^Parent parent opts reloading?]
    (let [[node-id] (g/tx-nodes-added
                      (g/transact (g/make-nodes graph [view-id    CurveView
                                                       #_controller #_CurveController
                                                       selection  [selection/SelectionController :select-fn (fn [selection op-seq] (project/sub-select! project selection))]
                                                       background background/Gradient
                                                       camera     [c/CameraController :local-camera (or (:camera opts) (c/make-camera :orthographic camera-filter-fn))]
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
                                                (g/connect background :renderable view-id :aux-renderables)
                                                #_(g/connect controller :input-handler view-id :input-handlers)

                                                (g/connect project :selected-node-properties view-id :selected-node-properties)
                                                (g/connect project :sub-selection view-id :sub-selection)

                                                (g/connect selection            :renderable                view-id          :tool-renderables)
                                                (g/connect selection            :input-handler             view-id          :input-handlers)
                                                (g/connect selection            :picking-rect              view-id          :picking-rect)
                                                (g/connect view-id              :picking-selection         selection        :picking-selection)
                                                #_(g/connect view-id              :selection                 selection        :selection))))
          ^Node pane (make-gl-pane node-id parent opts)]
      (ui/fill-control pane)
      (ui/children! parent [pane])
      (ui/user-data! parent ::node-id node-id)
      node-id)))

(defn- reload-curve-view []
  (when @view-state
    (let [{:keys [project graph ^Parent parent opts]} @view-state]
      (ui/run-now
        (destroy-view! parent)
        (make-view! project graph parent opts true)))))

(reload-curve-view)
