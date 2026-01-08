;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns jfx
  (:import [com.sun.javafx.application PlatformImpl]
           [java.util List Map Set]
           [javafx.css Styleable]
           [javafx.scene Node Parent Scene]
           [javafx.scene.control ChoiceBox ChoiceDialog ComboBox ContextMenu Control CustomMenuItem Dialog Labeled ListView Menu MenuButton MenuItem ScrollPane SplitPane Tab TabPane TableColumnBase TableView TextInputControl TitledPane ToolBar Tooltip TreeItem]
           [javafx.scene.control.cell ChoiceBoxListCell ChoiceBoxTableCell ChoiceBoxTreeCell ChoiceBoxTreeTableCell ComboBoxListCell ComboBoxTableCell ComboBoxTreeCell ComboBoxTreeTableCell]
           [javafx.scene.text Text]
           [javafx.stage Popup Window]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

;; The platform must be initialized before we can use certain JavaFX classes.
(PlatformImpl/startup (fn []))

(defprotocol InfoProps
  (info-props [this] "Returns a map of informative property values associated with the JavaFX object."))

(extend-protocol InfoProps
  Dialog
  (info-props [this]
    {:title (.getTitle ^Dialog this)})

  Labeled
  (info-props [this]
    {:text (.getText ^Labeled this)})

  MenuItem
  (info-props [this]
    {:text (.getText ^MenuItem this)})

  Scene
  (info-props [this]
    {:stylesheets (.getStylesheets ^Scene this)})

  Tab
  (info-props [this]
    {:text (.getText ^Tab this)})

  TableColumnBase
  (info-props [this]
    {:text (.getText ^TableColumnBase this)})

  TextInputControl
  (info-props [this]
    {:text (.getText ^TextInputControl this)})

  Tooltip
  (info-props [this]
    {:text (.getText ^Tooltip this)})

  Text
  (info-props [this]
    {:text (.getText ^Text this)})

  TreeItem
  (info-props [this]
    {:value (.getValue ^TreeItem this)}))

(defprotocol DataItems
  (data-items [this] "Returns a seq of non-JavaFX data items associated with the JavaFX object."))

(extend-protocol DataItems
  ChoiceBox
  (data-items [this]
    (.getItems ^ChoiceBox this))

  ChoiceBoxListCell
  (data-items [this]
    (.getItems ^ChoiceBoxListCell this))

  ChoiceBoxTableCell
  (data-items [this]
    (.getItems ^ChoiceBoxTableCell this))

  ChoiceBoxTreeCell
  (data-items [this]
    (.getItems ^ChoiceBoxTreeCell this))

  ChoiceBoxTreeTableCell
  (data-items [this]
    (.getItems ^ChoiceBoxTreeTableCell this))

  ChoiceDialog
  (data-items [this]
    (.getItems ^ChoiceDialog this))

  ComboBox
  (data-items [this]
    (.getItems ^ComboBox this))

  ComboBoxListCell
  (data-items [this]
    (.getItems ^ComboBoxListCell this))

  ComboBoxTableCell
  (data-items [this]
    (.getItems ^ComboBoxTableCell this))

  ComboBoxTreeCell
  (data-items [this]
    (.getItems ^ComboBoxTreeCell this))

  ComboBoxTreeTableCell
  (data-items [this]
    (.getItems ^ComboBoxTreeTableCell this))

  ListView
  (data-items [this]
    (.getItems ^ListView this))

  TableView
  (data-items [this]
    (.getItems ^TableView this)))

(defprotocol Related
  (related [this] "Return a sequence of [keyword javafx-object-or-seq] to recurse into."))

(extend-protocol Related
  ContextMenu
  (related [this]
    {:items (.getItems ^ContextMenu this)})

  Control
  (related [this]
    (let [^Control this this]
      {:tooltip (.getTooltip this)
       :context-menu (.getContextMenu this)
       :children (.getChildrenUnmodifiable this)}))

  CustomMenuItem
  (related [this]
    {:content (.getContent ^CustomMenuItem this)})

  Dialog
  (related [this]
    {:dialog-pane (.getDialogPane ^Dialog this)})

  Menu
  (related [this]
    {:items (.getItems ^Menu this)})

  MenuItem
  (related [this]
    {:graphic (.getGraphic ^MenuItem this)})

  MenuButton
  (related [this]
    {:items (.getItems ^MenuButton this)})

  Parent
  (related [this]
    {:children (.getChildrenUnmodifiable ^Parent this)})

  Popup
  (related [this]
    {:content (.getContent ^Popup this)})

  Scene
  (related [this]
    {:root (.getRoot ^Scene this)})

  ScrollPane
  (related [this]
    (let [^ScrollPane this this]
      {:content (.getContent this)
       :children (.getChildrenUnmodifiable this)}))

  SplitPane
  (related [this]
    {:items (.getItems ^SplitPane this)})

  Tab
  (related [this]
    (let [^Tab this this]
      {:graphic (.getGraphic this)
       :tooltip (.getTooltip this)
       :context-menu (.getContextMenu this)
       :content (.getContent this)}))

  TabPane
  (related [this]
    (let [^TabPane this this]
      {:tabs (.getTabs this)
       :children (.getChildrenUnmodifiable this)}))

  TableColumnBase
  (related [this]
    (let [^TableColumnBase this this]
      {:graphic (.getGraphic this)
       :context-menu (.getContextMenu this)
       :columns (.getColumns this)}))

  TableView
  (related [this]
    (let [^TableView this this]
      {:columns (.getColumns this)
       :children (.getChildrenUnmodifiable this)}))

  TitledPane
  (related [this]
    (let [^TitledPane this this]
      {:content (.getContent this)
       :children (.getChildrenUnmodifiable this)}))

  ToolBar
  (related [this]
    {:items (.getItems ^ToolBar this)})

  TreeItem
  (related [this]
    (let [^TreeItem this this]
      {:graphic (.getGraphic this)
       :children (.getChildren this)}))

  Window
  (related [this]
    {:scene (.getScene ^Window this)}))

(defn- styleable-props [^Styleable styleable]
  {:id (.getId styleable)
   :style (not-empty (.getStyle styleable))
   :style-classes (not-empty (vec (sort (.getStyleClass styleable))))
   :pseudo-classes (not-empty (into (sorted-set) (map str) (.getPseudoClassStates styleable)))})

(defn- node-props [^Node node]
  (let [layout-bounds (.getLayoutBounds node)]
    (cond-> {:layout-size [(.getWidth layout-bounds) (.getHeight layout-bounds)]}
            (.isDisable node) (assoc :disable true)
            (.isMouseTransparent node) (assoc :mouse-transparent true)
            (not (.isManaged node)) (assoc :managed false)
            (not (.isVisible node)) (assoc :visible false))))

(defn- to-coll [items]
  (cond
    (or (nil? items)
        (coll? items))
    items

    (instance? List items)
    (vec items)

    (instance? Map items)
    (into {} items)

    (instance? Set items)
    (set items)

    :else
    items))

(defn- info [obj]
  (cond-> {:type (type obj)}

          (instance? Styleable obj)
          (into (styleable-props obj))

          (instance? Node obj)
          (into (node-props obj))

          (satisfies? InfoProps obj)
          (into (info-props obj))

          (satisfies? DataItems obj)
          (assoc :data (to-coll (data-items obj)))))

(defn- cleanup-xform [value-fn]
  (let [item-fn (or value-fn identity)
        items-fn (if (nil? value-fn)
                   to-coll
                   (fn items-fn [items]
                     (into []
                           (keep #(not-empty (value-fn %)))
                           items)))]
    (keep (fn [[key val :as entry]]
            (cond
              (nil? val)
              nil

              (string? val)
              (when (pos? (count val))
                entry)

              (seqable? val)
              (when-some [items (not-empty (items-fn val))]
                [key items])

              :else
              (when-some [item (item-fn val)]
                [key item]))))))

(defn info-map [obj]
  (into {}
        (cleanup-xform nil)
        (info obj)))

(defn info-tree [obj]
  (cond-> (info-map obj)
          (satisfies? Related obj)
          (into (cleanup-xform info-tree)
                (related obj))))
