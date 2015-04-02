(ns editor.properties-view
  (:require [camel-snake-kebab :as camel]
            [clojure.java.io :as io]
            [dynamo.graph :as g]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [editor.atlas :as atlas]
            [editor.core :as core]
            [editor.cubemap :as cubemap]
            [editor.graph-view :as graph-view]
            [editor.jfx :as jfx]
            [editor.image :as image]
            [editor.platformer :as platformer]
            [editor.project :as p]
            [editor.switcher :as switcher]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [editor.project :as project]
            [editor.scene :as scene]
            [internal.clojure :as clojure]
            [internal.disposal :as disp]
            [internal.graph.types :as gt]
            [service.log :as log])
  (:import [com.defold.editor Start]
           [com.jogamp.opengl.util.awt Screenshot]
           [javafx.application Platform]
           [javafx.collections FXCollections ObservableList]
           [javafx.embed.swing SwingFXUtils]
           [javafx.event ActionEvent EventHandler]
           [javafx.fxml FXMLLoader]
           [javafx.geometry Insets]
           [javafx.scene Scene Node Parent]
           [javafx.scene.control Button ColorPicker Label TextField TitledPane TextArea TreeItem TreeCell Menu MenuItem MenuBar Tab ProgressBar]
           [javafx.scene.image Image ImageView WritableImage PixelWriter]
           [javafx.scene.input MouseEvent]
           [javafx.scene.layout AnchorPane GridPane StackPane HBox Priority]
           [javafx.scene.paint Color]
           [javafx.stage Stage FileChooser]
           [javafx.util Callback]
           [java.io File]
           [java.nio.file Paths]
           [java.util.prefs Preferences]
           [javax.media.opengl GL GL2 GLContext GLProfile GLDrawableFactory GLCapabilities]))

; From https://github.com/mikera/clojure-utils/blob/master/src/main/clojure/mikera/cljutils/loops.clj
(defmacro doseq-indexed
  "loops over a set of values, binding index-sym to the 0-based index of each value"
  ([[val-sym values index-sym] & code]
  `(loop [vals# (seq ~values)
          ~index-sym (long 0)]
     (if vals#
       (let [~val-sym (first vals#)]
             ~@code
             (recur (next vals#) (inc ~index-sym)))
       nil))))

(def create-property-control! nil)

(defmulti create-property-control! (fn [t _] t))

(defmethod create-property-control! String [_ on-new-value]
  (let [text (TextField.)
        setter #(.setText text (str %))]
    (.setOnAction text (ui/event-handler event (on-new-value (.getText text))))
    [text setter]))

(defn- to-int [s]
  (try
    (Integer/parseInt s)
    (catch Throwable _
      nil)))

(defmethod create-property-control! t/Int [_ on-new-value]
  (let [text (TextField.)
        setter #(.setText text (str %))]
    (.setOnAction text (ui/event-handler event (on-new-value (to-int (.getText text)))))
    [text setter]))

(defn- to-double [s]
  (try
    (Double/parseDouble s)
    (catch Throwable _
      nil)))

(defmethod create-property-control! t/Vec3 [_ on-new-value]
  (let [x (TextField.)
        y (TextField.)
        z (TextField.)
        box (HBox.)
        setter (fn [vec]
                 (doseq-indexed [t [x y z] i]
                   (.setText t (str (nth vec i)))))
        handler (ui/event-handler event (on-new-value (mapv #(to-double (.getText %)) [x y z])))]

    (doseq [t [x y z]]
      (.setOnAction t handler)
      (HBox/setHgrow t Priority/SOMETIMES)
      (.setPrefWidth t 60)
      (.add (.getChildren box) t))
    [box setter]))

(defmethod create-property-control! t/Color [_ on-new-value]
 (let [color-picker (ColorPicker.)
       handler (ui/event-handler event
                                 (let [c (.getValue color-picker)]
                                   (on-new-value [(.getRed c) (.getGreen c) (.getBlue c) (.getOpacity c)])))
       setter #(.setValue color-picker (Color. (nth % 0) (nth % 1) (nth % 2) (nth % 3)))]
   (.setOnAction color-picker handler)
   [color-picker setter]))

(defmethod create-property-control! :default [_ on-new-value]
  (let [text (TextField.)
        setter #(.setText text (str %))]
    (.setDisable text true)
    [text setter]))

(defn- niceify-label
  [k]
  (-> k
    name
    camel/->Camel_Snake_Case_String
    (clojure.string/replace "_" " ")))

(defn- create-properties-row [grid node key property row]
  (when (not (false? (:visible property)))
    (let [label (Label. (niceify-label key))
          ; TODO: Possible to solve mutual references without an atom here?
          setter-atom (atom nil)
          on-new-value (fn [new-val]
                         (let [old-val (key (g/refresh node))]
                           (when-not (= new-val old-val)
                             (if (t/property-valid-value? property new-val)
                               (do
                                 ;; TODO Consider using the :validator-fn feature of atom to apply validation
                                 (@setter-atom new-val)
                                 ;; TODO Apply a label to this transaction for undo menu
                                 (ds/transact (g/set-property node key new-val)))
                               (@setter-atom old-val)))))
          [control setter] (create-property-control! (t/property-value-type property) on-new-value)]
      (reset! setter-atom setter)
      (setter (get node key))
      (GridPane/setConstraints label 1 row)
      (GridPane/setConstraints control 2 row)
      (.add (.getChildren grid) label)
      (.add (.getChildren grid) control))))

(defn setup [parent node]
  (let [properties (g/properties node)
        grid (GridPane.)]
    (.clear (.getChildren parent))
    (.setPadding grid (Insets. 10 10 10 10))
    (.setHgap grid 4)
    (doseq [[key p] properties]
      (let [row (/ (.size (.getChildren grid)) 2)]
        (create-properties-row grid node key p row)))

    (.add (.getChildren parent) grid)))
