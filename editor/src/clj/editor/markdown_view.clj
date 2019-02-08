(ns editor.markdown-view
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [editor.view :as view]
            [editor.workspace :as workspace])
  (:import (org.commonmark.node Document Node)
           (javafx.css Styleable)
           (javafx.geometry HPos VPos)
           (javafx.scene.control Hyperlink ScrollPane Tab)
           (javafx.scene.image ImageView)
           (javafx.scene.layout ColumnConstraints GridPane Priority)
           (javafx.scene.text Text TextAlignment TextFlow)))

(set! *warn-on-reflection* true)

(defprotocol JavaFXNodeConversion
  (make-javafx-nodes [md-node prev-md-node context]))

(defn- md-node-children [^Node md-node]
  (take-while some?
              (iterate #(some-> ^Node % .getNext)
                       (.getFirstChild md-node))))

(defn- fx-node-children
  ^"[Ljavafx.scene.Node;" [^Node md-node context]
  (let [md-node-children (md-node-children md-node)]
    (ui/node-array
      (mapcat (fn [x y]
                (prn x y)
                (make-javafx-nodes x y context))
              md-node-children
              (cons nil md-node-children)))))

(defn add-style-class! [^String style-class ^Styleable node]
  (assert (string? style-class))
  (let [styles (.getStyleClass node)]
    (when-not (.contains styles style-class)
      (.add styles style-class))
    node))

(extend-type org.commonmark.node.Node
  JavaFXNodeConversion
  (make-javafx-nodes [md-node _prev-md-node _context]
    [(Text. (str "<" (.getSimpleName (class md-node)) ">"))]))

(extend-type org.commonmark.node.Code
  JavaFXNodeConversion
  (make-javafx-nodes [md-node _prev-md-node context]
    (map (partial add-style-class! "md-code")
         (fx-node-children md-node context))))

(extend-type org.commonmark.node.Emphasis
  JavaFXNodeConversion
  (make-javafx-nodes [md-node _prev-md-node context]
    (map (partial add-style-class! "md-emphasis")
         (fx-node-children md-node context))))

(extend-type org.commonmark.node.Heading
  JavaFXNodeConversion
  (make-javafx-nodes [md-node _prev-md-node context]
    (map (partial add-style-class! (str "md-heading-l" (.getLevel md-node)))
         (fx-node-children md-node context))))

(extend-type org.commonmark.node.HtmlInline
  JavaFXNodeConversion
  (make-javafx-nodes [md-node _prev-md-node _context]
    (prn "HtmlInline" (.getLiteral md-node))
    [(Text. (.getLiteral md-node))]))

(extend-type org.commonmark.node.Image
  JavaFXNodeConversion
  (make-javafx-nodes [md-node _prev-md-node {:keys [resolve-url] :as _context}]
    [(ImageView. ^String (resolve-url (.getDestination md-node)))]))

(extend-type org.commonmark.node.Link
  JavaFXNodeConversion
  (make-javafx-nodes [md-node _prev-md-node {:keys [on-link!] :as _context}]
    (let [href (.getDestination md-node)]
      [(doto (Hyperlink. (.getTitle md-node))
         (.setOnAction (ui/event-handler event
                         (on-link! href))))])))

(defn- make-ordered-list-numeral [^long index]
  (doto (Text. (str (inc index) \.))
    (.setTextAlignment TextAlignment/RIGHT)
    (GridPane/setRowIndex (int index))
    (GridPane/setColumnIndex (int 0))
    (GridPane/setHalignment HPos/RIGHT)
    (GridPane/setValignment VPos/TOP)))

(defn- make-list-item [^long index ^Node md-node context]
  (prn "make-list-entry" md-node)
  (doto (TextFlow. (fx-node-children md-node context))
    (GridPane/setRowIndex (int index))
    (GridPane/setColumnIndex (int 1))
    (GridPane/setHalignment HPos/LEFT)
    (GridPane/setValignment VPos/TOP)))

(extend-type org.commonmark.node.OrderedList
  JavaFXNodeConversion
  (make-javafx-nodes [md-node _prev-md-node context]
    (let [grid-pane (GridPane.)
          column-constraints (.getColumnConstraints grid-pane)]
      (.add column-constraints (doto (ColumnConstraints. 40)
                                 (.setHgrow Priority/NEVER)))
      (.add column-constraints (doto (ColumnConstraints.)
                                 (.setHgrow Priority/ALWAYS)))
      (ui/children! grid-pane
                    (ui/node-array
                      (apply concat
                             (map-indexed (fn [index md-node-child]
                                            [(make-ordered-list-numeral index)
                                             (make-list-item index md-node-child context)])
                                          (md-node-children md-node)))))
      [grid-pane])))

(extend-type org.commonmark.node.Paragraph
  JavaFXNodeConversion
  (make-javafx-nodes [md-node prev-md-node context]
    (cond->> (fx-node-children md-node context)
             (some? prev-md-node) (cons (Text. "\n\n")))))

(extend-type org.commonmark.node.SoftLineBreak
  JavaFXNodeConversion
  (make-javafx-nodes [_md-node _prev-md-node _context]
    nil))

(extend-type org.commonmark.node.Text
  JavaFXNodeConversion
  (make-javafx-nodes [md-node _prev-md-node _context]
    [(Text. (.getLiteral md-node))]))

;; -----------------------------------------------------------------------------

(defn resolve-url [workspace href]
  (str (.toURI (io/as-file (workspace/resolve-workspace-resource workspace href)))))

(g/defnk produce-repaint-view [^Document document ^TextFlow document-view node-id+resource]
  (let [resource (second node-id+resource)
        workspace (resource/workspace resource)
        context {:on-link! (partial prn "Clicked")
                 :resolve-url (partial resolve-url workspace)}]
    (.setAll (.getChildren document-view)
             (fx-node-children document context))))

(g/defnode MarkdownViewNode
  (inherits view/WorkbenchView)
  (property document-view TextFlow (dynamic visible (g/constantly false)))
  (input document Document)
  (output repaint-view g/Any :cached produce-repaint-view))

(defn- make-view [graph parent resource-node opts]
  (let [^Tab tab (:tab opts)
        document-view (TextFlow.)
        [view-node] (g/tx-nodes-added
                      (g/transact
                        (g/make-nodes graph [view-node [MarkdownViewNode :document-view document-view]]
                          (g/connect resource-node :document view-node :document))))
        repainter (ui/->timer "repaint-markdown-view"
                              (fn [_ _elapsed-time]
                                (when (and (.isSelected tab) (not (ui/ui-disabled?)))
                                  (g/node-value view-node :repaint-view))))]
    (ui/children! parent
                  [(doto (ScrollPane. document-view)
                     (.setFitToWidth true)
                     (ui/fill-control))])
    (ui/on-closed! tab (fn [_] (ui/timer-stop! repainter)))
    (ui/timer-start! repainter)
    view-node))

(defn register-view-types [workspace]
  (workspace/register-view-type workspace
                                :id :markdown
                                :label "Markdown"
                                :make-view-fn make-view))
