(ns editor.scene-selection
  (:require [clojure.set :as set]
            [dynamo.graph :as g]
            [editor.system :as system]
            [editor.background :as background]
            [editor.colors :as colors]
            [editor.camera :as c]
            [editor.core :as core]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.grid :as grid]
            [editor.input :as i]
            [editor.math :as math]
            [editor.types :as types]
            [editor.workspace :as workspace]
            [editor.gl.pass :as pass]
            [schema.core :as s]
            [service.log :as log])
  (:import [com.defold.editor Start UIUtil]
           [com.jogamp.opengl.util GLPixelStorageModes]
           [com.jogamp.opengl.util.awt TextRenderer]
           [editor.types Camera AABB Region Rect]
           [java.awt Font]
           [java.awt.image BufferedImage DataBufferByte]
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
           [java.lang Runnable Math]
           [java.nio IntBuffer ByteBuffer ByteOrder]
           [com.jogamp.opengl GL GL2 GL2GL3 GLContext GLProfile GLAutoDrawable GLOffscreenAutoDrawable GLDrawableFactory GLCapabilities]
           [com.jogamp.opengl.glu GLU]
           [javax.vecmath Point2i Point3d Quat4d Matrix4d Vector4d Matrix3d Vector3d]))

(set! *warn-on-reflection* true)

(defn render-selection-box [^GL2 gl render-args renderables count]
  (let [user-data (:user-data (first renderables))
        start (:start user-data)
        current (:current user-data)]
    (when (and start current)
     (let [min-fn (fn [v1 v2] (map #(Math/min ^Double %1 ^Double %2) v1 v2))
           max-fn (fn [v1 v2] (map #(Math/max ^Double %1 ^Double %2) v1 v2))
           min-p (reduce min-fn [start current])
           min-x (nth min-p 0)
           min-y (nth min-p 1)
           max-p (reduce max-fn [start current])
           max-x (nth max-p 0)
           max-y (nth max-p 1)
           z 0.0
           c (double-array (map #(/ % 255.0) [131 188 212]))]
       (.glColor3d gl (nth c 0) (nth c 1) (nth c 2))
       (.glBegin gl GL2/GL_LINE_LOOP)
       (.glVertex3d gl min-x min-y z)
       (.glVertex3d gl min-x max-y z)
       (.glVertex3d gl max-x max-y z)
       (.glVertex3d gl max-x min-y z)
       (.glEnd gl)

       (.glBegin gl GL2/GL_QUADS)
       (.glColor4d gl (nth c 0) (nth c 1) (nth c 2) 0.2)
       (.glVertex3d gl min-x, min-y, z);
       (.glVertex3d gl min-x, max-y, z);
       (.glVertex3d gl max-x, max-y, z);
       (.glVertex3d gl max-x, min-y, z);
       (.glEnd gl)))))

(defn- select [controller op-seq mode toggle?]
  (let [select-fn (g/node-value controller :select-fn)
        selection (g/node-value controller :picking-selection)
        mode-filter-fn (case mode
                        :single (fn [selection] (if-let [sel (first selection)] [sel] []))
                        :multi  identity)
        toggle-filter-fn (if toggle?
                           (fn [selection]
                             (let [selection-set (set selection)
                                   prev-selection (g/node-value controller :prev-selection)
                                   prev-selection-set (set prev-selection)]
                               (into [] (concat (filter (complement selection-set) prev-selection) (filter (complement prev-selection-set) selection)))))
                           identity)
        sel-filter-fn (comp toggle-filter-fn mode-filter-fn)
        selection (or (not-empty (sel-filter-fn selection))
                      (filter #(not (nil? %)) [(g/node-value controller :root-id)]))]
    (select-fn selection op-seq)))

(def mac-toggle-modifiers #{:shift :meta})
(def other-toggle-modifiers #{:control})
(def toggle-modifiers (if system/mac? mac-toggle-modifiers other-toggle-modifiers))

(def ^Integer min-pick-size 10)

(defn distance
  [[x0 y0 z0] [x1 y1 z1]]
  (.distance (Point3d. x0 y0 z0) (Point3d. x1 y1 z1)))

(defn handle-selection-input [self action user-data]
  (let [start      (g/node-value self :start)
        current    (g/node-value self :current)
        op-seq     (g/node-value self :op-seq)
        mode       (g/node-value self :mode)
        toggle?    (g/node-value self :toggle?)
        cursor-pos [(:x action) (:y action) 0]]
    (case (:type action)
      :mouse-pressed (let [op-seq (gensym)
                           toggle? (true? (some true? (map #(% action) toggle-modifiers)))
                           mode :single]
                       (g/transact
                         (concat
                           (g/set-property self :op-seq op-seq)
                           (g/set-property self :start cursor-pos)
                           (g/set-property self :current cursor-pos)
                           (g/set-property self :mode mode)
                           (g/set-property self :toggle? toggle?)
                           (g/set-property self :prev-selection (g/node-value self :selection))))
                       (select self op-seq mode toggle?)
                       nil)
      :mouse-released (do
                        (g/transact
                          (concat
                            (g/set-property self :start nil)
                            (g/set-property self :current nil)
                            (g/set-property self :op-seq nil)
                            (g/set-property self :mode nil)
                            (g/set-property self :toggle? nil)
                            (g/set-property self :prev-selection nil)))
                        nil)
      :mouse-moved (if start
                     (let [new-mode (if (and (= :single mode) (< min-pick-size (distance start cursor-pos)))
                                      :multi
                                      mode)]
                       (g/transact
                         (concat
                           (when (not= new-mode mode) (g/set-property self :mode new-mode))
                           (g/set-property self :current cursor-pos)))
                       (select self op-seq new-mode toggle?)
                       nil)
                     action)
      action)))

(defn- imin [^Integer v1 ^Integer v2] (Math/min v1 v2))
(defn- imax [^Integer v1 ^Integer v2] (Math/max v1 v2))

(defn calc-picking-rect [start current]
  (let [ps [start current]
        min-p (Point2i. (reduce imin (map first ps)) (reduce imin (map second ps)))
        max-p (Point2i. (reduce imax (map first ps)) (reduce imax (map second ps)))
        dims (doto (Point2i. max-p) (.sub min-p))
        center (doto (Point2i. min-p) (.add (Point2i. (/ (.x dims) 2) (/ (.y dims) 2))))]
    (Rect. nil (.x center) (.y center) (Math/max (.x dims) min-pick-size) (Math/max (.y dims) min-pick-size))))

(g/deftype SelectionMode (s/enum :single :multi))

(g/defnode SelectionController
  (property select-fn Runnable)
  (property start types/Vec3)
  (property current types/Vec3)
  (property op-seq g/Any)
  (property mode SelectionMode)
  (property toggle? g/Bool)
  (property prev-selection g/Any)

  (input selection g/Any)
  (input picking-selection g/Any)
  (input root-id g/NodeID)

  (output picking-rect Rect :cached (g/fnk [start current] (calc-picking-rect start current)))
  (output renderable pass/RenderData :cached (g/fnk [start current] {pass/overlay [{:world-transform (Matrix4d. geom/Identity4d)
                                                                                    :render-fn render-selection-box
                                                                                    :user-data {:start start :current current}}]}))
  (output input-handler Runnable :cached (g/constantly handle-selection-input)))
