(ns user
  (:require [clojure.java.io :as io]
            [clojure.java.io :refer [file]]
            [clojure.pprint :refer [pprint]]
            [clojure.repl :refer :all]
            [dynamo.file :as f]
            [dynamo.geom :as g]
            [dynamo.image :refer :all]
            [dynamo.node :as n]
            [dynamo.project :as p]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [dynamo.ui :as ui]
;            [dynamo.ui.widgets :as widgets]
            [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]
            [internal.java :as j]
            [internal.node :as in]
            [internal.system :as is]
            [schema.core :as s]
            [camel-snake-kebab :refer :all])
  (:import [java.awt Dimension]
           [javafx.application Platform]
           [javafx.fxml FXMLLoader]
           [javafx.scene Scene Node Parent]
           [javafx.stage Stage FileChooser]
           [javafx.embed.swing JFXPanel]
           [javax.swing JFrame JPanel]
           [javax.vecmath Matrix4d Matrix3d Point3d Vector4d Vector3d]))

(defonce force-toolkit-init (javafx.embed.swing.JFXPanel.))

(defn method->function [m]
  (list (symbol (.getName m)) (into ['this] (.getParameterTypes m))))

(defn all-types [cls]
  (cons cls (supers cls)))

(defn skeletor [iface]
  (->> iface
    all-types
    (mapcat #(.getDeclaredMethods %))
    (map method->function)))

(defn properties-of
  [class]
  (filter
   #(and (.endsWith (.getName %) "Property") (not (.startsWith (.getName %) "impl_")))
   (seq (.getMethods class))))

(def swizzlers
  {javafx.beans.property.ObjectProperty 'identity
   javafx.beans.property.BooleanProperty 'boolean})

(defn swizzle-property
  [p]
  (let [type (.getReturnType p)
        nm   (.getName p)
        nm   (subs nm 0 (- (count nm) (count "Property")))]
    (cond
      (.startsWith nm "on")   [(symbol nm)  'wrap-handler]
      :else                   [(symbol nm)  (swizzlers type 'identity)]))) ()

(defmacro tap [x] `(do (prn ~(str "**** " &form " ") ~x) ~x))

(defn macro-pretty-print
  [x]
  (clojure.pprint/write (macroexpand x) :dispatch clojure.pprint/code-dispatch))

(defn the-world       [] (-> is/the-system deref :world))
(defn the-world-state [] (-> (the-world) :state deref))
(defn the-cache       [] (-> (the-world-state) :cache))
(defn the-graph       [] (-> (the-world-state) :graph))
(defn nodes           [] (-> (the-graph) :nodes vals))

(defn nodes-and-classes
  []
  (let [gnodes (:nodes (the-graph))]
    (sort (map (fn [n v] [n (class v)]) (keys gnodes) (vals gnodes)))))

(defn node
  [id]
  (dg/node (the-graph) id))

(defn node-type
  [id]
  (-> (node id) t/node-type :name))

(defn nodes-of-type
  [type-name]
  (filter #(= type-name (node-type (:_id %))) (nodes)))

(def image-nodes (partial nodes-of-type "ImageResourceNode"))

(defn node-for-file
  [file-name]
  (filter #(= file-name (t/local-path (:filename %))) (filter (comp not nil? :filename) (nodes))))

(defn inputs-to
  ([id]
    (group-by first
      (for [a (dg/arcs-to (the-graph) id)]
        [(get-in a [:target-attributes :label]) (:source a) (get-in a [:source-attributes :label]) ])))
  ([id label]
    (lg/sources (the-graph) id label)))

(defn outputs-from
  ([id]
    (group-by first
      (for [a (dg/arcs-from (the-graph) id)]
       [(get-in a [:source-attributes :label]) (:target a) (get-in a [:target-attributes :label])])))
  ([id label]
    (lg/targets (the-graph) id label)))

(defn get-value
  [id label & {:as opt-map}]
  (n/get-node-value (merge (node id) opt-map) label))

(defn cache-peek
  [id label]
  (if-let [cache-key (some-> (the-world-state) :cache-keys (get-in [id label]))]
    (if-let [x (get (the-cache) cache-key)]
      x
      :value-not-cached)
    :output-not-cacheable))

(defn decache
  [id label]
  (if-let [cache-key (some-> (the-world-state) :cache-keys (get-in [id label]))]
    (if (get (the-cache) cache-key)
      (dosync
        (alter (:state (the-world)) update-in [:cache] clojure.core.cache/evict cache-key)
        :ok)
      :value-not-cached)
    :output-not-cacheable))

(defn projects-in-memory
  []
  (keep #(when (.endsWith (.getName (second %)) "Project__") (node (first %))) (nodes-and-classes)))

(defn images-from-dir
  [d]
  (map load-image (filter #(.endsWith (.getName %) ".png") (file-seq (file d)))))

(defn test-resource-dir
  [& [who]]
  (let [who (or who (System/getProperty "user.name"))]
    (str
     (cond
       (= who "mtnygard")   "/Users/mtnygard/src/defold"
       (= who "ben")        "/Users/ben/projects/eleiko")
     "/com.dynamo.cr/com.dynamo.cr.sceneed2/test/resources")))

(images-from-dir (test-resource-dir))

(defn image-panel [img]
  (doto (proxy [JPanel] []
          (paint [g]
            (.drawImage g img 0 0 nil)))
    (.setPreferredSize (new Dimension
                            (.getWidth img)
                            (.getHeight img)))))

(defn imshow [img]
  (let [panel (image-panel img)]
    (doto (JFrame.) (.add panel) .pack .show)))

(defn protobuf-fields
  [protobuf-msg-cls]
  (for [fld (.getFields (internal.java/invoke-no-arg-class-method protobuf-msg-cls "getDescriptor"))]
    {:name (.getName fld)
     :type (let [t (.getJavaType fld)]
             (if (not= com.google.protobuf.Descriptors$FieldDescriptor$JavaType/MESSAGE t)
               (str t)
               (.. fld getMessageType getName)))
     :resource? (.getExtension (.getOptions fld) com.dynamo.proto.DdfExtensions/resource)}))

(defn load-stage [fxml]
  (let [root (FXMLLoader/load (.toURL (io/file fxml)))
        stage (Stage.)
        scene (Scene. root)]
    (.setUseSystemMenuBar (.lookup root "#menu-bar") true)
    (.setTitle stage "GUI playground")
    (.setScene stage scene)
    (.show stage)
    root))

(comment

  (map swizzle-property (properties-of javafx.scene.control.Button))


  (def classes-of-interest
    (ui/run-wait
     #(list
       javafx.scene.input.ClipboardContent
       javafx.animation.Animation
       javafx.animation.Timeline
       javafx.animation.Transition
       javafx.animation.FadeTransition
       javafx.animation.FillTransition
       javafx.animation.ParallelTransition
       javafx.animation.PathTransition
       javafx.animation.PauseTransition
       javafx.animation.RotateTransition
       javafx.animation.ScaleTransition
       javafx.animation.SequentialTransition
       javafx.animation.StrokeTransition
       javafx.animation.TranslateTransition
       javafx.animation.AnimationTimer
       javafx.application.Application
       javafx.application.Preloader
       javafx.application.Application$Parameters
       javafx.scene.media.AudioClip
       javafx.scene.media.AudioEqualizer
       javafx.scene.chart.Axis$TickMark
       javafx.scene.layout.Background
       javafx.scene.layout.BackgroundFill
       javafx.scene.layout.BackgroundImage
       javafx.scene.layout.BackgroundPosition
       javafx.scene.layout.BackgroundSize
       javafx.beans.binding.Bindings
       javafx.scene.layout.Border
       javafx.scene.layout.BorderImage
       javafx.scene.layout.BorderStroke
       javafx.scene.layout.BorderStrokeStyle
       javafx.scene.layout.BorderWidths
       javafx.geometry.Bounds
       javafx.geometry.BoundingBox
       javafx.scene.input.Clipboard
       javafx.scene.input.Dragboard
       javafx.embed.swing.JFXPanel
       javafx.scene.layout.ConstraintsBase
       javafx.scene.layout.ColumnConstraints
       javafx.scene.layout.RowConstraints
       javafx.scene.layout.CornerRadii
       javafx.css.CssMetaData
       javafx.css.FontCssMetaData
       javafx.scene.Cursor
       javafx.scene.ImageCursor
       javafx.scene.input.DataFormat
       javafx.geometry.Dimension2D
       javafx.stage.DirectoryChooser
       javafx.util.Duration
       javafx.scene.effect.Effect
       javafx.scene.effect.Blend
       javafx.scene.effect.Bloom
       javafx.scene.effect.BoxBlur
       javafx.scene.effect.ColorAdjust
       javafx.scene.effect.ColorInput
       javafx.scene.effect.DisplacementMap
       javafx.scene.effect.DropShadow
       javafx.scene.effect.GaussianBlur
       javafx.scene.effect.Glow
       javafx.scene.effect.ImageInput
       javafx.scene.effect.InnerShadow
       javafx.scene.effect.Lighting
       javafx.scene.effect.MotionBlur
       javafx.scene.effect.PerspectiveTransform
       javafx.scene.effect.Reflection
       javafx.scene.effect.SepiaTone
       javafx.scene.effect.Shadow
       javafx.scene.media.EqualizerBand
       javafx.event.Event
       javafx.event.ActionEvent
       javafx.scene.media.MediaMarkerEvent
       javafx.scene.control.CheckBoxTreeItem$TreeModificationEvent
       javafx.scene.input.InputEvent
       javafx.scene.input.ContextMenuEvent
       javafx.scene.input.DragEvent
       javafx.scene.input.GestureEvent
       javafx.scene.input.RotateEvent
       javafx.scene.input.ScrollEvent
       javafx.scene.input.SwipeEvent
       javafx.scene.input.ZoomEvent
       javafx.scene.input.InputMethodEvent
       javafx.scene.input.KeyEvent
       javafx.scene.input.MouseEvent
       javafx.scene.input.MouseDragEvent
       javafx.scene.input.TouchEvent
       javafx.scene.control.ListView$EditEvent
       javafx.scene.media.MediaErrorEvent
       javafx.scene.control.ScrollToEvent
       javafx.scene.control.SortEvent
       javafx.scene.control.TableColumn$CellEditEvent
       javafx.scene.transform.TransformChangedEvent
       javafx.scene.control.TreeItem$TreeModificationEvent
       javafx.scene.control.TreeTableColumn$CellEditEvent
       javafx.scene.control.TreeTableView$EditEvent
       javafx.scene.control.TreeView$EditEvent
       javafx.scene.web.WebErrorEvent
       javafx.scene.web.WebEvent
       javafx.stage.WindowEvent
       javafx.concurrent.WorkerStateEvent
       javafx.event.EventType
       javafx.stage.FileChooser
       javafx.stage.FileChooser$ExtensionFilter
       javafx.scene.effect.FloatMap
       javafx.scene.control.FocusModel
       javafx.scene.control.TableFocusModel
       javafx.scene.control.TableView$TableViewFocusModel
       javafx.scene.control.TreeTableView$TreeTableViewFocusModel
       javafx.scene.text.Font
       javafx.concurrent.Task
       javafx.collections.FXCollections
       javafx.fxml.FXMLLoader
       javafx.scene.canvas.GraphicsContext
       javafx.application.HostServices
       javafx.scene.image.Image
       javafx.scene.image.WritableImage
       javafx.scene.control.IndexRange
       javafx.scene.input.InputMethodTextRun
       javafx.geometry.Insets
       javafx.animation.Interpolator
       javafx.print.JobSettings
       javafx.scene.input.KeyCombination
       javafx.scene.input.KeyCharacterCombination
       javafx.scene.input.KeyCodeCombination
       javafx.scene.input.KeyCombination$Modifier
       javafx.animation.KeyFrame
       javafx.animation.KeyValue
       javafx.scene.effect.Light
       javafx.scene.effect.Light$Distant
       javafx.scene.effect.Light$Point
       javafx.scene.effect.Light$Spot
       javafx.collections.ListChangeListener$Change
       javafx.collections.MapChangeListener$Change
       javafx.scene.paint.Material
       javafx.scene.paint.PhongMaterial
       javafx.scene.media.Media
       javafx.scene.media.MediaPlayer
       javafx.scene.control.MenuItem
       javafx.scene.control.CheckMenuItem
       javafx.scene.control.CustomMenuItem
       javafx.scene.control.SeparatorMenuItem
       javafx.scene.control.Menu
       javafx.scene.control.RadioMenuItem
       javafx.scene.shape.Mesh
       javafx.scene.shape.TriangleMesh
       javafx.scene.input.Mnemonic
       javafx.scene.Node
       javafx.scene.Camera
       javafx.scene.ParallelCamera
       javafx.scene.PerspectiveCamera
       javafx.scene.canvas.Canvas
       javafx.scene.image.ImageView
       javafx.scene.LightBase
       javafx.scene.AmbientLight
       javafx.scene.PointLight
       javafx.scene.media.MediaView
       javafx.scene.Parent
       javafx.scene.Group
       javafx.scene.control.PopupControl$CSSBridge
       javafx.scene.layout.Region
       javafx.scene.chart.Axis
       javafx.scene.chart.CategoryAxis
       javafx.scene.chart.ValueAxis
       javafx.scene.chart.NumberAxis
       javafx.scene.chart.Chart
       javafx.scene.chart.PieChart
       javafx.scene.chart.XYChart
       javafx.scene.chart.AreaChart
       javafx.scene.chart.BarChart
       javafx.scene.chart.BubbleChart
       javafx.scene.chart.LineChart
       javafx.scene.chart.ScatterChart
       javafx.scene.chart.StackedAreaChart
       javafx.scene.chart.StackedBarChart
       javafx.scene.control.Control
       javafx.scene.control.Accordion
       javafx.scene.control.ChoiceBox
       javafx.scene.control.ComboBoxBase
       javafx.scene.control.ColorPicker
       javafx.scene.control.ComboBox
       javafx.scene.control.DatePicker
       javafx.scene.web.HTMLEditor
       javafx.scene.control.Labeled
       javafx.scene.control.ButtonBase
       javafx.scene.control.Button
       javafx.scene.control.CheckBox
       javafx.scene.control.Hyperlink
       javafx.scene.control.MenuButton
       javafx.scene.control.SplitMenuButton
       javafx.scene.control.ToggleButton
       javafx.scene.control.RadioButton
       javafx.scene.control.Cell
       javafx.scene.control.DateCell
       javafx.scene.control.IndexedCell
       javafx.scene.control.ListCell
       javafx.scene.control.cell.CheckBoxListCell
       javafx.scene.control.cell.ChoiceBoxListCell
       javafx.scene.control.cell.ComboBoxListCell
       javafx.scene.control.cell.TextFieldListCell
       javafx.scene.control.TableCell
       javafx.scene.control.cell.CheckBoxTableCell
       javafx.scene.control.cell.ChoiceBoxTableCell
       javafx.scene.control.cell.ComboBoxTableCell
       javafx.scene.control.cell.ProgressBarTableCell
       javafx.scene.control.cell.TextFieldTableCell
       javafx.scene.control.TableRow
       javafx.scene.control.TreeCell
       javafx.scene.control.cell.CheckBoxTreeCell
       javafx.scene.control.cell.ChoiceBoxTreeCell
       javafx.scene.control.cell.ComboBoxTreeCell
       javafx.scene.control.cell.TextFieldTreeCell
       javafx.scene.control.TreeTableCell
       javafx.scene.control.cell.CheckBoxTreeTableCell
       javafx.scene.control.cell.ChoiceBoxTreeTableCell
       javafx.scene.control.cell.ComboBoxTreeTableCell
       javafx.scene.control.cell.ProgressBarTreeTableCell
       javafx.scene.control.cell.TextFieldTreeTableCell
       javafx.scene.control.TreeTableRow
       javafx.scene.control.Label
       javafx.scene.control.TitledPane
       javafx.scene.control.ListView
       javafx.scene.control.MenuBar
       javafx.scene.control.Pagination
       javafx.scene.control.ProgressIndicator
       javafx.scene.control.ProgressBar
       javafx.scene.control.ScrollBar
       javafx.scene.control.ScrollPane
       javafx.scene.control.Separator
       javafx.scene.control.Slider
       javafx.scene.control.SplitPane
       javafx.scene.control.TableView
       javafx.scene.control.TabPane
       javafx.scene.control.TextInputControl
       javafx.scene.control.TextArea
       javafx.scene.control.TextField
       javafx.scene.control.PasswordField
       javafx.scene.control.ToolBar
       javafx.scene.control.TreeTableView
       javafx.scene.control.TreeView
       javafx.scene.layout.Pane
       javafx.scene.layout.AnchorPane
       javafx.scene.layout.BorderPane
       javafx.scene.layout.FlowPane
       javafx.scene.layout.GridPane
       javafx.scene.layout.HBox
       javafx.scene.layout.StackPane
       javafx.scene.text.TextFlow
       javafx.scene.layout.TilePane
       javafx.scene.layout.VBox
       javafx.scene.web.WebView
       javafx.scene.shape.Shape
       javafx.scene.shape.Arc
       javafx.scene.shape.Circle
       javafx.scene.shape.CubicCurve
       javafx.scene.shape.Ellipse
       javafx.scene.shape.Line
       javafx.scene.shape.Path
       javafx.scene.shape.Polygon
       javafx.scene.shape.Polyline
       javafx.scene.shape.QuadCurve
       javafx.scene.shape.Rectangle
       javafx.scene.shape.SVGPath
       javafx.scene.text.Text
       javafx.scene.shape.Shape3D
       javafx.scene.shape.Box
       javafx.scene.shape.Cylinder
       javafx.scene.shape.MeshView
       javafx.scene.shape.Sphere
       javafx.scene.SubScene
       javafx.embed.swing.SwingNode
       javafx.print.PageLayout
       javafx.print.PageRange
       javafx.scene.paint.Paint
       javafx.scene.paint.Color
       javafx.scene.paint.ImagePattern
       javafx.scene.paint.LinearGradient
       javafx.scene.paint.RadialGradient
       javafx.util.Pair
       javafx.print.Paper
       javafx.print.PaperSource
       javafx.css.ParsedValue
       javafx.scene.shape.PathElement
       javafx.scene.shape.ArcTo
       javafx.scene.shape.ClosePath
       javafx.scene.shape.CubicCurveTo
       javafx.scene.shape.HLineTo
       javafx.scene.shape.LineTo
       javafx.scene.shape.MoveTo
       javafx.scene.shape.QuadCurveTo
       javafx.scene.shape.VLineTo
       javafx.scene.input.PickResult
       javafx.scene.chart.PieChart$Data
       javafx.scene.image.PixelFormat
       javafx.scene.image.WritablePixelFormat
       javafx.application.Platform
       javafx.geometry.Point2D
       javafx.geometry.Point3D
       javafx.scene.web.PopupFeatures
       javafx.application.Preloader$ErrorNotification
       javafx.application.Preloader$ProgressNotification
       javafx.application.Preloader$StateChangeNotification
       javafx.print.Printer
       javafx.print.PrinterAttributes
       javafx.print.PrinterJob
       javafx.print.PrintResolution
       javafx.scene.web.PromptData
       javafx.css.PseudoClass
       javafx.geometry.Rectangle2D
       javafx.scene.control.ResizeFeaturesBase
       javafx.scene.control.TableView$ResizeFeatures
       javafx.scene.control.TreeTableView$ResizeFeatures
       javafx.scene.Scene
       javafx.scene.SceneAntialiasing
       javafx.scene.control.SelectionModel
       javafx.scene.control.MultipleSelectionModel
       javafx.scene.control.TableSelectionModel
       javafx.scene.control.TableView$TableViewSelectionModel
       javafx.scene.control.TreeTableView$TreeTableViewSelectionModel
       javafx.scene.control.SingleSelectionModel
       javafx.concurrent.Service
       javafx.concurrent.ScheduledService
       javafx.collections.SetChangeListener$Change
       javafx.beans.binding.SetExpression
       javafx.beans.binding.SetBinding
       javafx.scene.control.SkinBase
       javafx.scene.SnapshotParameters
       javafx.scene.SnapshotResult
       javafx.scene.control.SplitPane$Divider
       javafx.scene.paint.Stop
       javafx.util.StringConverter
       javafx.css.StyleConverter
       javafx.scene.layout.CornerRadiiConverter
       javafx.scene.control.Tab
       javafx.scene.control.TableColumn$CellDataFeatures
       javafx.scene.control.TableColumnBase
       javafx.scene.control.TableColumn
       javafx.scene.control.TreeTableColumn
       javafx.scene.control.TablePositionBase
       javafx.scene.control.TablePosition
       javafx.scene.control.TreeTablePosition
       javafx.scene.control.ToggleGroup
       javafx.scene.input.TouchPoint
       javafx.scene.media.Track
       javafx.scene.media.AudioTrack
       javafx.scene.media.SubtitleTrack
       javafx.scene.media.VideoTrack
       javafx.scene.transform.Transform
       javafx.scene.transform.Affine
       javafx.scene.transform.Rotate
       javafx.scene.transform.Scale
       javafx.scene.transform.Shear
       javafx.scene.transform.Translate
       javafx.scene.control.TreeItem
       javafx.scene.control.CheckBoxTreeItem
       javafx.scene.control.TreeTableColumn$CellDataFeatures
       javafx.scene.web.WebEngine
       javafx.scene.web.WebHistory
       javafx.scene.web.WebHistory$Entry
       javafx.beans.binding.When
       javafx.stage.Window
       javafx.stage.PopupWindow
       javafx.stage.Popup
       javafx.scene.control.PopupControl
       javafx.scene.control.ContextMenu
       javafx.scene.control.Tooltip
       javafx.stage.Stage
       javafx.scene.chart.XYChart$Data
       javafx.scene.chart.XYChart$Series)))

  (def props-all
    (apply merge-with concat (for [c classes-of-interest
                                  p (properties-of c)]
                              {c (swizzle-property p)})))

  )


(remove #(java.lang.reflect.Modifier/isAbstract (.getModifiers %))
        [javafx.scene.input.ClipboardContent
         javafx.animation.Animation
         javafx.animation.Timeline
         javafx.animation.Transition
         javafx.animation.FadeTransition
         javafx.animation.FillTransition
         javafx.animation.ParallelTransition
         javafx.animation.PathTransition
         javafx.animation.PauseTransition
         javafx.animation.RotateTransition
         javafx.animation.ScaleTransition
         javafx.animation.SequentialTransition
         javafx.animation.StrokeTransition
         javafx.animation.TranslateTransition
         javafx.animation.AnimationTimer
         javafx.application.Application
         javafx.application.Preloader
         javafx.application.Application$Parameters
         javafx.scene.media.AudioClip
         javafx.scene.media.AudioEqualizer
         javafx.scene.chart.Axis$TickMark
         javafx.scene.layout.Background
         javafx.scene.layout.BackgroundFill
         javafx.scene.layout.BackgroundImage
         javafx.scene.layout.BackgroundPosition
         javafx.scene.layout.BackgroundSize
         javafx.beans.binding.Bindings
         javafx.scene.layout.Border
         javafx.scene.layout.BorderImage
         javafx.scene.layout.BorderStroke
         javafx.scene.layout.BorderStrokeStyle
         javafx.scene.layout.BorderWidths
         javafx.geometry.Bounds
         javafx.geometry.BoundingBox
         javafx.scene.input.Clipboard
         javafx.scene.input.Dragboard
         javafx.embed.swing.JFXPanel
         javafx.scene.layout.ConstraintsBase
         javafx.scene.layout.ColumnConstraints
         javafx.scene.layout.RowConstraints
         javafx.scene.layout.CornerRadii
         javafx.css.CssMetaData
         javafx.css.FontCssMetaData
         javafx.scene.Cursor
         javafx.scene.ImageCursor
         javafx.scene.input.DataFormat
         javafx.geometry.Dimension2D
         javafx.stage.DirectoryChooser
         javafx.util.Duration
         javafx.scene.effect.Effect
         javafx.scene.effect.Blend
         javafx.scene.effect.Bloom
         javafx.scene.effect.BoxBlur
         javafx.scene.effect.ColorAdjust
         javafx.scene.effect.ColorInput
         javafx.scene.effect.DisplacementMap
         javafx.scene.effect.DropShadow
         javafx.scene.effect.GaussianBlur
         javafx.scene.effect.Glow
         javafx.scene.effect.ImageInput
         javafx.scene.effect.InnerShadow
         javafx.scene.effect.Lighting
         javafx.scene.effect.MotionBlur
         javafx.scene.effect.PerspectiveTransform
         javafx.scene.effect.Reflection
         javafx.scene.effect.SepiaTone
         javafx.scene.effect.Shadow
         javafx.scene.media.EqualizerBand
         javafx.event.Event
         javafx.event.ActionEvent
         javafx.scene.media.MediaMarkerEvent
         javafx.scene.control.CheckBoxTreeItem$TreeModificationEvent
         javafx.scene.input.InputEvent
         javafx.scene.input.ContextMenuEvent
         javafx.scene.input.DragEvent
         javafx.scene.input.GestureEvent
         javafx.scene.input.RotateEvent
         javafx.scene.input.ScrollEvent
         javafx.scene.input.SwipeEvent
         javafx.scene.input.ZoomEvent
         javafx.scene.input.InputMethodEvent
         javafx.scene.input.KeyEvent
         javafx.scene.input.MouseEvent
         javafx.scene.input.MouseDragEvent
         javafx.scene.input.TouchEvent
         javafx.scene.control.ListView$EditEvent
         javafx.scene.media.MediaErrorEvent
         javafx.scene.control.ScrollToEvent
         javafx.scene.control.SortEvent
         javafx.scene.control.TableColumn$CellEditEvent
         javafx.scene.transform.TransformChangedEvent
         javafx.scene.control.TreeItem$TreeModificationEvent
         javafx.scene.control.TreeTableColumn$CellEditEvent
         javafx.scene.control.TreeTableView$EditEvent
         javafx.scene.control.TreeView$EditEvent
         javafx.scene.web.WebErrorEvent
         javafx.scene.web.WebEvent
         javafx.stage.WindowEvent
         javafx.concurrent.WorkerStateEvent
         javafx.event.EventType
         javafx.stage.FileChooser
         javafx.stage.FileChooser$ExtensionFilter
         javafx.scene.effect.FloatMap
         javafx.scene.control.FocusModel
         javafx.scene.control.TableFocusModel
         javafx.scene.control.TableView$TableViewFocusModel
         javafx.scene.control.TreeTableView$TreeTableViewFocusModel
         javafx.scene.text.Font
         javafx.concurrent.Task
         javafx.collections.FXCollections
         javafx.fxml.FXMLLoader
         javafx.scene.canvas.GraphicsContext
         javafx.application.HostServices
         javafx.scene.image.Image
         javafx.scene.image.WritableImage
         javafx.scene.control.IndexRange
         javafx.scene.input.InputMethodTextRun
         javafx.geometry.Insets
         javafx.animation.Interpolator
         javafx.print.JobSettings
         javafx.scene.input.KeyCombination
         javafx.scene.input.KeyCharacterCombination
         javafx.scene.input.KeyCodeCombination
         javafx.scene.input.KeyCombination$Modifier
         javafx.animation.KeyFrame
         javafx.animation.KeyValue
         javafx.scene.effect.Light
         javafx.scene.effect.Light$Distant
         javafx.scene.effect.Light$Point
         javafx.scene.effect.Light$Spot
         javafx.collections.ListChangeListener$Change
         javafx.collections.MapChangeListener$Change
         javafx.scene.paint.Material
         javafx.scene.paint.PhongMaterial
         javafx.scene.media.Media
         javafx.scene.media.MediaPlayer
         javafx.scene.control.MenuItem
         javafx.scene.control.CheckMenuItem
         javafx.scene.control.CustomMenuItem
         javafx.scene.control.SeparatorMenuItem
         javafx.scene.control.Menu
         javafx.scene.control.RadioMenuItem
         javafx.scene.shape.Mesh
         javafx.scene.shape.TriangleMesh
         javafx.scene.input.Mnemonic
         javafx.scene.Node
         javafx.scene.Camera
         javafx.scene.ParallelCamera
         javafx.scene.PerspectiveCamera
         javafx.scene.canvas.Canvas
         javafx.scene.image.ImageView
         javafx.scene.LightBase
         javafx.scene.AmbientLight
         javafx.scene.PointLight
         javafx.scene.media.MediaView
         javafx.scene.Parent
         javafx.scene.Group
         javafx.scene.control.PopupControl$CSSBridge
         javafx.scene.layout.Region
         javafx.scene.chart.Axis
         javafx.scene.chart.CategoryAxis
         javafx.scene.chart.ValueAxis
         javafx.scene.chart.NumberAxis
         javafx.scene.chart.Chart
         javafx.scene.chart.PieChart
         javafx.scene.chart.XYChart
         javafx.scene.chart.AreaChart
         javafx.scene.chart.BarChart
         javafx.scene.chart.BubbleChart
         javafx.scene.chart.LineChart
         javafx.scene.chart.ScatterChart
         javafx.scene.chart.StackedAreaChart
         javafx.scene.chart.StackedBarChart
         javafx.scene.control.Control
         javafx.scene.control.Accordion
         javafx.scene.control.ChoiceBox
         javafx.scene.control.ComboBoxBase
         javafx.scene.control.ColorPicker
         javafx.scene.control.ComboBox
         javafx.scene.control.DatePicker
         javafx.scene.web.HTMLEditor
         javafx.scene.control.Labeled
         javafx.scene.control.ButtonBase
         javafx.scene.control.Button
         javafx.scene.control.CheckBox
         javafx.scene.control.Hyperlink
         javafx.scene.control.MenuButton
         javafx.scene.control.SplitMenuButton
         javafx.scene.control.ToggleButton
         javafx.scene.control.RadioButton
         javafx.scene.control.Cell
         javafx.scene.control.DateCell
         javafx.scene.control.IndexedCell
         javafx.scene.control.ListCell
         javafx.scene.control.cell.CheckBoxListCell
         javafx.scene.control.cell.ChoiceBoxListCell
         javafx.scene.control.cell.ComboBoxListCell
         javafx.scene.control.cell.TextFieldListCell
         javafx.scene.control.TableCell
         javafx.scene.control.cell.CheckBoxTableCell
         javafx.scene.control.cell.ChoiceBoxTableCell
         javafx.scene.control.cell.ComboBoxTableCell
         javafx.scene.control.cell.ProgressBarTableCell
         javafx.scene.control.cell.TextFieldTableCell
         javafx.scene.control.TableRow
         javafx.scene.control.TreeCell
         javafx.scene.control.cell.CheckBoxTreeCell
         javafx.scene.control.cell.ChoiceBoxTreeCell
         javafx.scene.control.cell.ComboBoxTreeCell
         javafx.scene.control.cell.TextFieldTreeCell
         javafx.scene.control.TreeTableCell
         javafx.scene.control.cell.CheckBoxTreeTableCell
         javafx.scene.control.cell.ChoiceBoxTreeTableCell
         javafx.scene.control.cell.ComboBoxTreeTableCell
         javafx.scene.control.cell.ProgressBarTreeTableCell
         javafx.scene.control.cell.TextFieldTreeTableCell
         javafx.scene.control.TreeTableRow
         javafx.scene.control.Label
         javafx.scene.control.TitledPane
         javafx.scene.control.ListView
         javafx.scene.control.MenuBar
         javafx.scene.control.Pagination
         javafx.scene.control.ProgressIndicator
         javafx.scene.control.ProgressBar
         javafx.scene.control.ScrollBar
         javafx.scene.control.ScrollPane
         javafx.scene.control.Separator
         javafx.scene.control.Slider
         javafx.scene.control.SplitPane
         javafx.scene.control.TableView
         javafx.scene.control.TabPane
         javafx.scene.control.TextInputControl
         javafx.scene.control.TextArea
         javafx.scene.control.TextField
         javafx.scene.control.PasswordField
         javafx.scene.control.ToolBar
         javafx.scene.control.TreeTableView
         javafx.scene.control.TreeView
         javafx.scene.layout.Pane
         javafx.scene.layout.AnchorPane
         javafx.scene.layout.BorderPane
         javafx.scene.layout.FlowPane
         javafx.scene.layout.GridPane
         javafx.scene.layout.HBox
         javafx.scene.layout.StackPane
         javafx.scene.text.TextFlow
         javafx.scene.layout.TilePane
         javafx.scene.layout.VBox
         javafx.scene.web.WebView
         javafx.scene.shape.Shape
         javafx.scene.shape.Arc
         javafx.scene.shape.Circle
         javafx.scene.shape.CubicCurve
         javafx.scene.shape.Ellipse
         javafx.scene.shape.Line
         javafx.scene.shape.Path
         javafx.scene.shape.Polygon
         javafx.scene.shape.Polyline
         javafx.scene.shape.QuadCurve
         javafx.scene.shape.Rectangle
         javafx.scene.shape.SVGPath
         javafx.scene.text.Text
         javafx.scene.shape.Shape3D
         javafx.scene.shape.Box
         javafx.scene.shape.Cylinder
         javafx.scene.shape.MeshView
         javafx.scene.shape.Sphere
         javafx.scene.SubScene
         javafx.embed.swing.SwingNode
         javafx.print.PageLayout
         javafx.print.PageRange
         javafx.scene.paint.Paint
         javafx.scene.paint.Color
         javafx.scene.paint.ImagePattern
         javafx.scene.paint.LinearGradient
         javafx.scene.paint.RadialGradient
         javafx.util.Pair
         javafx.print.Paper
         javafx.print.PaperSource
         javafx.css.ParsedValue
         javafx.scene.shape.PathElement
         javafx.scene.shape.ArcTo
         javafx.scene.shape.ClosePath
         javafx.scene.shape.CubicCurveTo
         javafx.scene.shape.HLineTo
         javafx.scene.shape.LineTo
         javafx.scene.shape.MoveTo
         javafx.scene.shape.QuadCurveTo
         javafx.scene.shape.VLineTo
         javafx.scene.input.PickResult
         javafx.scene.chart.PieChart$Data
         javafx.scene.image.PixelFormat
         javafx.scene.image.WritablePixelFormat
         javafx.application.Platform
         javafx.geometry.Point2D
         javafx.geometry.Point3D
         javafx.scene.web.PopupFeatures
         javafx.application.Preloader$ErrorNotification
         javafx.application.Preloader$ProgressNotification
         javafx.application.Preloader$StateChangeNotification
         javafx.print.Printer
         javafx.print.PrinterAttributes
         javafx.print.PrinterJob
         javafx.print.PrintResolution
         javafx.scene.web.PromptData
         javafx.css.PseudoClass
         javafx.geometry.Rectangle2D
         javafx.scene.control.ResizeFeaturesBase
         javafx.scene.control.TableView$ResizeFeatures
         javafx.scene.control.TreeTableView$ResizeFeatures
         javafx.scene.Scene
         javafx.scene.SceneAntialiasing
         javafx.scene.control.SelectionModel
         javafx.scene.control.MultipleSelectionModel
         javafx.scene.control.TableSelectionModel
         javafx.scene.control.TableView$TableViewSelectionModel
         javafx.scene.control.TreeTableView$TreeTableViewSelectionModel
         javafx.scene.control.SingleSelectionModel
         javafx.concurrent.Service
         javafx.concurrent.ScheduledService
         javafx.collections.SetChangeListener$Change
         javafx.beans.binding.SetExpression
         javafx.beans.binding.SetBinding
         javafx.scene.control.SkinBase
         javafx.scene.SnapshotParameters
         javafx.scene.SnapshotResult
         javafx.scene.control.SplitPane$Divider
         javafx.scene.paint.Stop
         javafx.util.StringConverter
         javafx.css.StyleConverter
         javafx.scene.layout.CornerRadiiConverter
         javafx.scene.control.Tab
         javafx.scene.control.TableColumn$CellDataFeatures
         javafx.scene.control.TableColumnBase
         javafx.scene.control.TableColumn
         javafx.scene.control.TreeTableColumn
         javafx.scene.control.TablePositionBase
         javafx.scene.control.TablePosition
         javafx.scene.control.TreeTablePosition
         javafx.scene.control.ToggleGroup
         javafx.scene.input.TouchPoint
         javafx.scene.media.Track
         javafx.scene.media.AudioTrack
         javafx.scene.media.SubtitleTrack
         javafx.scene.media.VideoTrack
         javafx.scene.transform.Transform
         javafx.scene.transform.Affine
         javafx.scene.transform.Rotate
         javafx.scene.transform.Scale
         javafx.scene.transform.Shear
         javafx.scene.transform.Translate
         javafx.scene.control.TreeItem
         javafx.scene.control.CheckBoxTreeItem
         javafx.scene.control.TreeTableColumn$CellDataFeatures
         javafx.scene.web.WebEngine
         javafx.scene.web.WebHistory
         javafx.scene.web.WebHistory$Entry
         javafx.beans.binding.When
         javafx.stage.Window
         javafx.stage.PopupWindow
         javafx.stage.Popup
         javafx.scene.control.PopupControl
         javafx.scene.control.ContextMenu
         javafx.scene.control.Tooltip
         javafx.stage.Stage
         javafx.scene.chart.XYChart$Data
         javafx.scene.chart.XYChart$Series])

(def concrete-javafx-classes [javafx.scene.input.ClipboardContent javafx.animation.Timeline javafx.animation.FadeTransition javafx.animation.FillTransition javafx.animation.ParallelTransition javafx.animation.PathTransition javafx.animation.PauseTransition javafx.animation.RotateTransition javafx.animation.ScaleTransition javafx.animation.SequentialTransition javafx.animation.StrokeTransition javafx.animation.TranslateTransition javafx.scene.media.AudioClip javafx.scene.media.AudioEqualizer javafx.scene.chart.Axis$TickMark javafx.scene.layout.Background javafx.scene.layout.BackgroundFill javafx.scene.layout.BackgroundImage javafx.scene.layout.BackgroundPosition javafx.scene.layout.BackgroundSize javafx.beans.binding.Bindings javafx.scene.layout.Border javafx.scene.layout.BorderImage javafx.scene.layout.BorderStroke javafx.scene.layout.BorderStrokeStyle javafx.scene.layout.BorderWidths javafx.geometry.BoundingBox javafx.scene.input.Clipboard javafx.scene.input.Dragboard javafx.embed.swing.JFXPanel javafx.scene.layout.ColumnConstraints javafx.scene.layout.RowConstraints javafx.scene.layout.CornerRadii javafx.scene.ImageCursor javafx.scene.input.DataFormat javafx.geometry.Dimension2D javafx.stage.DirectoryChooser javafx.util.Duration javafx.scene.effect.Blend javafx.scene.effect.Bloom javafx.scene.effect.BoxBlur javafx.scene.effect.ColorAdjust javafx.scene.effect.ColorInput javafx.scene.effect.DisplacementMap javafx.scene.effect.DropShadow javafx.scene.effect.GaussianBlur javafx.scene.effect.Glow javafx.scene.effect.ImageInput javafx.scene.effect.InnerShadow javafx.scene.effect.Lighting javafx.scene.effect.MotionBlur javafx.scene.effect.PerspectiveTransform javafx.scene.effect.Reflection javafx.scene.effect.SepiaTone javafx.scene.effect.Shadow javafx.scene.media.EqualizerBand javafx.event.Event javafx.event.ActionEvent javafx.scene.media.MediaMarkerEvent javafx.scene.control.CheckBoxTreeItem$TreeModificationEvent javafx.scene.input.InputEvent javafx.scene.input.ContextMenuEvent javafx.scene.input.DragEvent javafx.scene.input.GestureEvent javafx.scene.input.RotateEvent javafx.scene.input.ScrollEvent javafx.scene.input.SwipeEvent javafx.scene.input.ZoomEvent javafx.scene.input.InputMethodEvent javafx.scene.input.KeyEvent javafx.scene.input.MouseEvent javafx.scene.input.MouseDragEvent javafx.scene.input.TouchEvent javafx.scene.control.ListView$EditEvent javafx.scene.media.MediaErrorEvent javafx.scene.control.ScrollToEvent javafx.scene.control.SortEvent javafx.scene.control.TableColumn$CellEditEvent javafx.scene.transform.TransformChangedEvent javafx.scene.control.TreeItem$TreeModificationEvent javafx.scene.control.TreeTableColumn$CellEditEvent javafx.scene.control.TreeTableView$EditEvent javafx.scene.control.TreeView$EditEvent javafx.scene.web.WebErrorEvent javafx.scene.web.WebEvent javafx.stage.WindowEvent javafx.concurrent.WorkerStateEvent javafx.event.EventType javafx.stage.FileChooser javafx.stage.FileChooser$ExtensionFilter javafx.scene.effect.FloatMap javafx.scene.control.TableView$TableViewFocusModel javafx.scene.control.TreeTableView$TreeTableViewFocusModel javafx.scene.text.Font javafx.collections.FXCollections javafx.fxml.FXMLLoader javafx.scene.canvas.GraphicsContext javafx.application.HostServices javafx.scene.image.Image javafx.scene.image.WritableImage javafx.scene.control.IndexRange javafx.scene.input.InputMethodTextRun javafx.geometry.Insets javafx.print.JobSettings javafx.scene.input.KeyCharacterCombination javafx.scene.input.KeyCodeCombination javafx.scene.input.KeyCombination$Modifier javafx.animation.KeyFrame javafx.animation.KeyValue javafx.scene.effect.Light$Distant javafx.scene.effect.Light$Point javafx.scene.effect.Light$Spot javafx.scene.paint.PhongMaterial javafx.scene.media.Media javafx.scene.media.MediaPlayer javafx.scene.control.MenuItem javafx.scene.control.CheckMenuItem javafx.scene.control.CustomMenuItem javafx.scene.control.SeparatorMenuItem javafx.scene.control.Menu javafx.scene.control.RadioMenuItem javafx.scene.shape.TriangleMesh javafx.scene.input.Mnemonic javafx.scene.ParallelCamera javafx.scene.PerspectiveCamera javafx.scene.canvas.Canvas javafx.scene.image.ImageView javafx.scene.AmbientLight javafx.scene.PointLight javafx.scene.media.MediaView javafx.scene.Group javafx.scene.control.PopupControl$CSSBridge javafx.scene.layout.Region javafx.scene.chart.CategoryAxis javafx.scene.chart.NumberAxis javafx.scene.chart.PieChart javafx.scene.chart.AreaChart javafx.scene.chart.BarChart javafx.scene.chart.BubbleChart javafx.scene.chart.LineChart javafx.scene.chart.ScatterChart javafx.scene.chart.StackedAreaChart javafx.scene.chart.StackedBarChart javafx.scene.control.Accordion javafx.scene.control.ChoiceBox javafx.scene.control.ColorPicker javafx.scene.control.ComboBox javafx.scene.control.DatePicker javafx.scene.web.HTMLEditor javafx.scene.control.Button javafx.scene.control.CheckBox javafx.scene.control.Hyperlink javafx.scene.control.MenuButton javafx.scene.control.SplitMenuButton javafx.scene.control.ToggleButton javafx.scene.control.RadioButton javafx.scene.control.Cell javafx.scene.control.DateCell javafx.scene.control.IndexedCell javafx.scene.control.ListCell javafx.scene.control.cell.CheckBoxListCell javafx.scene.control.cell.ChoiceBoxListCell javafx.scene.control.cell.ComboBoxListCell javafx.scene.control.cell.TextFieldListCell javafx.scene.control.TableCell javafx.scene.control.cell.CheckBoxTableCell javafx.scene.control.cell.ChoiceBoxTableCell javafx.scene.control.cell.ComboBoxTableCell javafx.scene.control.cell.ProgressBarTableCell javafx.scene.control.cell.TextFieldTableCell javafx.scene.control.TableRow javafx.scene.control.TreeCell javafx.scene.control.cell.CheckBoxTreeCell javafx.scene.control.cell.ChoiceBoxTreeCell javafx.scene.control.cell.ComboBoxTreeCell javafx.scene.control.cell.TextFieldTreeCell javafx.scene.control.TreeTableCell javafx.scene.control.cell.CheckBoxTreeTableCell javafx.scene.control.cell.ChoiceBoxTreeTableCell javafx.scene.control.cell.ComboBoxTreeTableCell javafx.scene.control.cell.ProgressBarTreeTableCell javafx.scene.control.cell.TextFieldTreeTableCell javafx.scene.control.TreeTableRow javafx.scene.control.Label javafx.scene.control.TitledPane javafx.scene.control.ListView javafx.scene.control.MenuBar javafx.scene.control.Pagination javafx.scene.control.ProgressIndicator javafx.scene.control.ProgressBar javafx.scene.control.ScrollBar javafx.scene.control.ScrollPane javafx.scene.control.Separator javafx.scene.control.Slider javafx.scene.control.SplitPane javafx.scene.control.TableView javafx.scene.control.TabPane javafx.scene.control.TextArea javafx.scene.control.TextField javafx.scene.control.PasswordField javafx.scene.control.ToolBar javafx.scene.control.TreeTableView javafx.scene.control.TreeView javafx.scene.layout.Pane javafx.scene.layout.AnchorPane javafx.scene.layout.BorderPane javafx.scene.layout.FlowPane javafx.scene.layout.GridPane javafx.scene.layout.HBox javafx.scene.layout.StackPane javafx.scene.text.TextFlow javafx.scene.layout.TilePane javafx.scene.layout.VBox javafx.scene.web.WebView javafx.scene.shape.Arc javafx.scene.shape.Circle javafx.scene.shape.CubicCurve javafx.scene.shape.Ellipse javafx.scene.shape.Line javafx.scene.shape.Path javafx.scene.shape.Polygon javafx.scene.shape.Polyline javafx.scene.shape.QuadCurve javafx.scene.shape.Rectangle javafx.scene.shape.SVGPath javafx.scene.text.Text javafx.scene.shape.Box javafx.scene.shape.Cylinder javafx.scene.shape.MeshView javafx.scene.shape.Sphere javafx.scene.SubScene javafx.embed.swing.SwingNode javafx.print.PageLayout javafx.print.PageRange javafx.scene.paint.Color javafx.scene.paint.ImagePattern javafx.scene.paint.LinearGradient javafx.scene.paint.RadialGradient javafx.util.Pair javafx.print.Paper javafx.print.PaperSource javafx.css.ParsedValue javafx.scene.shape.ArcTo javafx.scene.shape.ClosePath javafx.scene.shape.CubicCurveTo javafx.scene.shape.HLineTo javafx.scene.shape.LineTo javafx.scene.shape.MoveTo javafx.scene.shape.QuadCurveTo javafx.scene.shape.VLineTo javafx.scene.input.PickResult javafx.scene.chart.PieChart$Data javafx.application.Platform javafx.geometry.Point2D javafx.geometry.Point3D javafx.scene.web.PopupFeatures javafx.application.Preloader$ErrorNotification javafx.application.Preloader$ProgressNotification javafx.application.Preloader$StateChangeNotification javafx.print.Printer javafx.print.PrinterAttributes javafx.print.PrinterJob javafx.print.PrintResolution javafx.scene.web.PromptData javafx.geometry.Rectangle2D javafx.scene.control.ResizeFeaturesBase javafx.scene.control.TableView$ResizeFeatures javafx.scene.control.TreeTableView$ResizeFeatures javafx.scene.Scene javafx.scene.SceneAntialiasing javafx.scene.SnapshotParameters javafx.scene.SnapshotResult javafx.scene.control.SplitPane$Divider javafx.scene.paint.Stop javafx.css.StyleConverter javafx.scene.layout.CornerRadiiConverter javafx.scene.control.Tab javafx.scene.control.TableColumn$CellDataFeatures javafx.scene.control.TableColumn javafx.scene.control.TreeTableColumn javafx.scene.control.TablePosition javafx.scene.control.TreeTablePosition javafx.scene.control.ToggleGroup javafx.scene.input.TouchPoint javafx.scene.media.AudioTrack javafx.scene.media.SubtitleTrack javafx.scene.media.VideoTrack javafx.scene.transform.Affine javafx.scene.transform.Rotate javafx.scene.transform.Scale javafx.scene.transform.Shear javafx.scene.transform.Translate javafx.scene.control.TreeItem javafx.scene.control.CheckBoxTreeItem javafx.scene.control.TreeTableColumn$CellDataFeatures javafx.scene.web.WebEngine javafx.scene.web.WebHistory javafx.scene.web.WebHistory$Entry javafx.beans.binding.When javafx.stage.Window javafx.stage.Popup javafx.scene.control.PopupControl javafx.scene.control.ContextMenu javafx.scene.control.Tooltip javafx.stage.Stage javafx.scene.chart.XYChart$Data javafx.scene.chart.XYChart$Series])

(defn last-part [n]
  (subs n (inc (.lastIndexOf n "."))))

(let [by-package (group-by #(.getPackage %) concrete-javafx-classes)]
  (doseq [p (keys by-package)
          :let [ns-name (symbol (.getName p))]]
    (spit (str ns-name ".clj")
          (list* 'ns ns-name
                 '(:require [dynamo.ui.widgets :refer :all])
                 (for [c (by-package p)
                       :let [cls (symbol (.getName c))
                             func (symbol (->kebab-case (last-part (.getName c))))]]
                   (list 'defobject func cls))))))

(spit "foo.txt" "abacab")

(load-file "javafx.animation.clj")
(load-file "javafx.util.clj")


(ns-unmap 'javafx.animation 'key-frame)
