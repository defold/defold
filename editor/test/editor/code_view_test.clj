(ns editor.code-view-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.code-view :as cv]
            [editor.lua :as lua]
            [editor.script :as script]
            [support.test-support :refer [with-clean-system tx-nodes]])
  (:import [org.eclipse.jface.text IDocument DocumentEvent TextUtilities]
           [org.eclipse.fx.text.ui TextPresentation]))


(with-clean-system
  (let [opts lua/lua
        source-viewer (cv/setup-source-viewer opts)
        [code-node viewer-node] (tx-nodes (g/make-node world script/ScriptNode)
                                          (g/make-node world cv/CodeView :source-viewer source-viewer))]
    (g/transact (g/set-property code-node :code "-- hi break \n"))
    (cv/setup-code-view viewer-node code-node 0)
    (g/node-value viewer-node :new-content)
    (g/node-value viewer-node :code)
    (let [document (->  (g/node-value viewer-node :source-viewer)
                          (.getDocument))
          document-len (.getLength document)
          text-widget (.getTextWidget  (g/node-value viewer-node :source-viewer))
          len (dec (.getCharCount text-widget))
          style-ranges (.getStyleRanges text-widget  (int 0) len false) ]
      (.getText text-widget (int 0) len)
      (map #(.toString %) style-ranges))

))

(pst )
