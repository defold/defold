(ns editor.image
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.project :as project]
            [editor.workspace :as workspace])
  (:import [java.awt.image BufferedImage]
           [javax.imageio ImageIO]))

(g/defnode ImageNode
  (inherits project/ResourceNode)

  (output content BufferedImage :cached (g/fnk [resource] (ImageIO/read (io/input-stream resource)))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace :ext ["jpg" "png"] :node-type ImageNode :view-types [:default]))