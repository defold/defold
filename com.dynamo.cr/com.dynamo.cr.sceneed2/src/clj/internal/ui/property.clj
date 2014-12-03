(ns internal.ui.property
  (:require [dynamo.node :as n]
            [dynamo.system :as ds])
  (:import [org.eclipse.swt SWT]
           [org.eclipse.swt.custom StackLayout]
           [org.eclipse.swt.widgets Composite]
           [org.eclipse.ui.forms.widgets FormToolkit]
           [org.eclipse.swt.layout GridData GridLayout]
           [com.dynamo.cr.properties Messages]))

(set! *warn-on-reflection* true)

(n/defnode PropertyView
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
                         (.setLayout (GridLayout.)))]
      (.createLabel toolkit no-selection Messages/FormPropertySheetViewer_NO_PROPERTIES)
      (set! (. stack topControl) no-selection)
      (.layout composite)
      (.reflow form true)
      (ds/set-property self
        :form         form
        :body         body
        :stack        stack
        :composite    composite
        :no-selection no-selection))))

(defn implementation-for
  [scope]
  (ds/transactional
    (ds/in scope
      (ds/add (make-property-view)))))

(defn get-control [property-view-node]
  (:form (ds/refresh property-view-node)))
