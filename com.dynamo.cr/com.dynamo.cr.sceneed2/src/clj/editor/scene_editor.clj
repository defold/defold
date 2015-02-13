(ns editor.scene-editor
  (:require [clojure.java.io :as io]
            [schema.core :as s]
            [internal.clojure :as clojure]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [dynamo.file :as f]
            [dynamo.project :as p]
            [internal.system :as is]
            [internal.transaction :as it]
            [internal.disposal :as disp]
            [service.log :as log]
            )
  (:import  [com.defold.editor Start UIUtil]
            [java.io File]
            [java.nio.file Paths]
            [javafx.application Platform]
            [javafx.fxml FXMLLoader]
            [javafx.collections FXCollections ObservableList]
            [javafx.scene Scene Node Parent]
            [javafx.stage Stage FileChooser]
            [javafx.scene.image Image ImageView WritableImage PixelWriter]
            [javafx.scene.input MouseEvent]
            [javafx.event ActionEvent EventHandler]
            [javafx.scene.control Button TitledPane TextArea TreeItem Menu MenuItem MenuBar Tab ProgressBar]
            [javafx.scene.layout AnchorPane StackPane HBox Priority]
            [javafx.embed.swing SwingFXUtils]
            [javax.media.opengl GL GL2 GLContext GLProfile GLDrawableFactory GLCapabilities]
            [com.jogamp.opengl.util.awt Screenshot]))

(n/defnode SceneEditor
  (inherits n/Scope)
  (on :create
      (let [image-view (ImageView.)
            image (WritableImage. 400 400)
            parent (:parent event)]
        (.setImage image-view image)
        (AnchorPane/setTopAnchor image-view 0.0)
        (AnchorPane/setBottomAnchor image-view 0.0)
        (AnchorPane/setLeftAnchor image-view 0.0)
        (AnchorPane/setRightAnchor image-view 0.0)
        (.add (.getChildren parent) image-view)
        (let [profile (GLProfile/getGL2ES2)
              factory (GLDrawableFactory/getFactory profile)
              caps (GLCapabilities. profile)]
          (.setOnscreen caps false)
          (.setPBuffer caps true)
          (.setDoubleBuffered caps false)
          (let [drawable (.createOffscreenAutoDrawable factory nil caps nil 400 400 nil)
                context (.getContext drawable)]
            (.makeCurrent context)
            (let [gl (.getGL context)]
              (.glClear gl GL/GL_COLOR_BUFFER_BIT)
              (.glColor4f gl 1.0 1.0 1.0 1.0)
              (.glBegin gl GL/GL_TRIANGLES)
              (.glVertex2f gl 0.0 0.0)
              (.glVertex2f gl 1.0 0.0)
              (.glVertex2f gl 0.0 1.0)
              (.glEnd gl)
              (.glFlush gl)
              (let [buf-image (Screenshot/readToBufferedImage 400 400 true)]
                (.flush buf-image)
                (SwingFXUtils/toFXImage buf-image image)))))))
  t/IDisposable
  (dispose [this]
           (println "Dispose SceneEditor")))

(defn construct-scene-editor
  [project-node editor-site node]
  (let [editor (n/construct SceneEditor)]
    editor))
