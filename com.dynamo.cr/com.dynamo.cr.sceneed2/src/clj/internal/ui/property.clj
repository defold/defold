(ns internal.ui.property
  (:require [schema.core :as s]
            [plumbing.core :refer [defnk]]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [dynamo.ui :as ui]
            [internal.node :as in])
  (:import [org.eclipse.swt SWT]
           [org.eclipse.swt.custom StackLayout]
           [org.eclipse.swt.widgets Composite]
           [org.eclipse.ui ISelectionListener]
           [org.eclipse.ui.forms.widgets FormToolkit]
           [org.eclipse.swt.layout GridData GridLayout]
           [com.dynamo.cr.properties Messages]))

(set! *warn-on-reflection* true)

;; TODO: This should actually **aggregate** something.
(defnk aggregate-properties [properties] (first properties))

(defn- refresh-property-page [node]
  (let [content (in/get-node-value node :content)]
    (.setText (:label node) (pr-str content))))

(n/defnode PropertyView
  (input properties [t/Properties])
  (output content s/Any aggregate-properties)

  (on :create
    (let [parent       (:parent event)
          toolkit      (FormToolkit. (.getDisplay ^Composite parent))
          form         (doto (.createScrolledForm toolkit parent)
                         (.setText "Properties"))
          body         (doto (.getBody form)
                         (.setLayout (GridLayout.)))
          stack        (StackLayout.)
          composite    (doto (.createComposite toolkit body)
                         (.setLayoutData (GridData. SWT/FILL SWT/FILL true true))
                         (.setLayout stack))
          no-selection (doto (.createComposite toolkit composite)
                         (.setLayout (GridLayout.)))
          label        (.createLabel toolkit no-selection Messages/FormPropertySheetViewer_NO_PROPERTIES)]
      (set! (. stack topControl) no-selection)
      (.layout composite)
      (.reflow form true)
      (ds/set-property self
        :form         form
        :body         body
        :stack        stack
        :composite    composite
        :no-selection no-selection
        :label        label)))

  ISelectionListener
  (selectionChanged [this part selection]
    (ds/transactional
      (doseq [n @selection]
        (ds/connect {:_id n} :properties this :properties)))
    (ui/after 100 (refresh-property-page (ds/refresh this)))))

(defn implementation-for
  [scope]
  (ds/transactional
    (ds/in scope
      (ds/add (make-property-view)))))

(defn get-control [property-view-node]
  (:form (ds/refresh property-view-node)))
