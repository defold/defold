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
            [editor.project :as project]
            [editor.scene-tools :as scene-tools]
            [editor.types :as types]
            [editor.workspace :as workspace]
            [editor.gl.pass :as pass]
            [service.log :as log]
            [editor.scene :as scene])
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
           [javax.media.opengl GL GL2 GL2GL3 GLContext GLProfile GLAutoDrawable GLOffscreenAutoDrawable GLDrawableFactory GLCapabilities]
           [javax.media.opengl.glu GLU]
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

(defn- select [controller op-seq mode]
  (let [select-fn (g/node-value controller :select-fn)
        selection (g/node-value controller :picking-selection)
        sel-filter-fn (case mode
                        :direct (fn [selection] selection)
                        :toggle (fn [selection]
                                  (let [selection-set (set selection)
                                        prev-selection-set (g/node-value controller :prev-selection-set)]
                                    (seq (set/union (set/difference prev-selection-set selection-set) (set/difference selection-set prev-selection-set))))))
        selection (or (not-empty (sel-filter-fn (map :node-id selection))) (filter #(not (nil? %)) [(:node-id (g/node-value controller :scene))]))]
    (select-fn selection op-seq)))

(def mac-toggle-modifiers #{:shift :meta})
(def other-toggle-modifiers #{:control})
(def toggle-modifiers (if system/mac? mac-toggle-modifiers other-toggle-modifiers))

(defn handle-selection-input [self action user-data]
  (let [start      (g/node-value self :start)
        current    (g/node-value self :current)
        op-seq     (g/node-value self :op-seq)
        mode       (g/node-value self :mode)
        cursor-pos [(:x action) (:y action) 0]]
    (case (:type action)
      :mouse-pressed (let [op-seq (gensym)
                           toggle (reduce #(or %1 %2) (map #(% action) toggle-modifiers))
                           mode (if toggle :toggle :direct)]
                       (g/transact
                         (concat
                           (g/set-property self :op-seq op-seq)
                           (g/set-property self :start cursor-pos)
                           (g/set-property self :current cursor-pos)
                           (g/set-property self :mode mode)
                           (g/set-property self :prev-selection-set (set (g/node-value self :selection)))))
                       (select self op-seq mode)
                       nil)
      :mouse-released (do
                        (g/transact
                          (concat
                            (g/set-property self :start nil)
                            (g/set-property self :current nil)
                            (g/set-property self :op-seq nil)
                            (g/set-property self :mode nil)
                            (g/set-property self :prev-selection-set nil)))
                        nil)
      :mouse-moved (if start
                     (do
                       (g/transact (g/set-property self :current cursor-pos))
                       (select self op-seq mode)
                       nil)
                     action)
      action)))

(g/defnode SelectionController
  (property select-fn Runnable)
  (property start types/Vec3)
  (property current types/Vec3)
  (property op-seq g/Any)
  (property mode (g/enum :direct :toggle))
  (property prev-selection-set g/Any)

  (input selection g/Any)
  (input picking-selection g/Any)
  (input scene g/Any)

  (output picking-rect Rect :cached (g/fnk [start current] (scene/calc-picking-rect start current)))
  (output renderable pass/RenderData :cached (g/fnk [start current] {pass/overlay [{:world-transform (Matrix4d. geom/Identity4d)
                                                                                    :render-fn render-selection-box
                                                                                    :user-data {:start start :current current}}]}))
  (output input-handler Runnable :cached (g/always handle-selection-input)))

(scene/defcontroller :selection
  (make [project resource-node view renderer]
        (let [view-graph  (g/node-id->graph-id view)]
          (g/make-nodes view-graph
                  [selection  [SelectionController :select-fn (fn [selection op-seq] (project/select! project selection op-seq))]]

                  (g/connect resource-node        :scene                     selection        :scene)
                  (g/connect selection            :renderable                renderer         :tool-renderables)
                  (g/connect selection            :input-handler             view             :input-handlers)
                  (g/connect selection            :picking-rect              renderer         :picking-rect)
                  (g/connect renderer             :picking-selection         selection        :picking-selection)
                  (g/connect view                 :selection                 selection        :selection)))))
