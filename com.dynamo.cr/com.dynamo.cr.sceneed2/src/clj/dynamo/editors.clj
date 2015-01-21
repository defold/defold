(ns dynamo.editors
  "This namespace provides the built in editor parts, along with support for building your own editors."
  (:require [schema.core :as s]
            [plumbing.core :refer [fnk]]
            [dynamo.camera :as c]
            [dynamo.gl :as gl]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [internal.node :as in]
            [internal.ui.editors :as ie]
            [internal.ui.handlers :refer [active-editor]]
            [internal.ui.scene-editor :as iuse])
  (:import  [internal.ui GenericEditor IDirtyable]
            [dynamo.types Camera AABB]
            [java.awt Font]
            [javax.media.opengl GL2 GLContext GLDrawableFactory]
            [com.jogamp.opengl.util.awt TextRenderer]
            [org.eclipse.swt.opengl GLCanvas]
            [org.eclipse.core.commands ExecutionEvent]))

(set! *warn-on-reflection* true)

(defn event->active-editor
  "Returns the Clojure implementation of the active editor from the event's application context."
  [^ExecutionEvent evt]
  (let [editor (active-editor (.getApplicationContext evt))]
    (if (instance? GenericEditor editor)
      (.getBehavior ^GenericEditor editor))))

(defn open-part
  "Creates an Eclipse view for the given behavior. The
behavior must support dynamo.types/MessageTarget.

Allowed options are:

:label - the visible label for the part
:closeable - if true, the part can be closed (defaults to true)
:id - the part ID by which Eclipse will know it"
  [behavior & {:as opts}]
  (ie/open-part behavior opts))

(n/defnode Renderer
  "This node type provides 3D rendering abilities. It should be mixed in to an editor or view node.

Inputs:

- view-camera `dynamo.types.Camera` - Defines the world-to-screen transformation
- renderables `[dynamo.types/RenderData]` - Sources of rendering data
- aabb        `dynamo.types.AABB` - The axis-aligned bounding box of the renderable contents

Properties
- context `GLContext` - The OpenGL context used to render in this space
- canvas  `GLCanvas` - The canvas on which this will render
- text-renderer `TextRenderer` - A helper object for rendering HUD text

Outputs
- render-data `dynamo.types/RenderData` - Depth sorted, collated render data. For internal use only.
- aabb - Passthrough of the aabb input. For internal use only.
"
  (input view-camera Camera)
  (input renderables [t/RenderData])
  (input aabb        AABB)

  (property context GLContext)
  (property canvas  GLCanvas)
  (property text-renderer TextRenderer)

  (output render-data t/RenderData iuse/produce-render-data)
  (output aabb AABB (fnk [aabb] aabb))

  t/Frame
  (frame [this]
    (iuse/paint-renderer
      (:context this)
      (:canvas this)
      this
      (first (n/get-node-inputs this :view-camera))
      (:text-renderer this)))

  (on :resize
    (let [canvas      (:canvas self)
          client      (.getClientArea ^GLCanvas canvas)
          viewport    (t/->Region 0 (.width client) 0 (.height client))
          aspect      (/ (double (.width client)) (.height client))
          camera-node (ds/node-feeding-into self :view-camera)
          camera      (n/get-node-value camera-node :camera)
          new-camera  (-> camera
                        (c/set-orthographic (:fov camera)
                                            aspect
                                            -100000
                                            100000)
                        (assoc :viewport viewport))]
      (ds/set-property camera-node :camera new-camera)))

  (on :reframe
    (let [camera-node (ds/node-feeding-into self :view-camera)
          camera      (n/get-node-value camera-node :camera)
          aabb        (n/get-node-value self :aabb)]
      (when aabb ;; there exists an aabb to center on
        (ds/set-property camera-node :camera (c/camera-orthographic-frame-aabb camera aabb))))))

(n/defnode SceneEditor
  "SceneEditor is the basis for all 2D orthographic and 3D perspective editors.
It provides rendering behavior (inherited from Renderer). It also acts as a Scope for
view-local nodes (e.g., view camera, controller, manipulator).

Inputs:
- controller  `dynamo.types/Node` - The node that handles UI gestures
- saveable `schema.core/Keyword` - Used to produce the `saveable` output. A content node connects to this so that pulling the content node's value causes the file to be saved as a side effect.
- presenter-registry `dynamo.types/Registry` - Property presenters that have been registered for this project.
- dirty `schema.core/Bool` - When true, causes the editor to be marked as dirty.

Outputs:
- saveable - Passthrough of the `saveable` output. Pulling this output causes the content node to save itself.
- presenter-registry - Passthrough of the presenter registry.

Messages:
- :create  - Builds the GUI components in respond to the editor part being opened.
- :init    - State initialization, sent after :create
- :save    - Sent by the GUI when the user wants to save the content
- :destroy - Clean up
"
  (inherits n/Scope)
  (inherits Renderer)

  (input controller `t/Node)

  (input  saveable  s/Keyword)
  (output saveable  s/Keyword (fnk [saveable] saveable))

  (input  presenter-registry t/Registry)
  (output presenter-registry t/Registry (fnk [presenter-registry] presenter-registry))

  (input dirty s/Bool)

  (property triggers n/Triggers (default [#'n/inject-new-nodes #'n/dispose-nodes #'iuse/send-view-scope-message #'iuse/mark-editor-dirty]))

  (on :init
    (let [tracker (:dirty-tracker event)]
      (when (in/get-inputs self (-> self :world-ref deref :graph) :dirty)
        (.markDirty ^IDirtyable tracker))
      (ds/set-property self :dirty-tracker tracker)))

  (on :create
    (let [canvas        (gl/glcanvas (:parent event))
          factory       (gl/glfactory)
          _             (.setCurrent canvas)
          context       (.createExternalGLContext factory)
          _             (.makeCurrent context)
          gl            (.. context getGL getGL2)]
      (.glPolygonMode gl GL2/GL_FRONT GL2/GL_FILL)
      (.release context)
      (iuse/pipe-events-to-node canvas :resize self)
      (iuse/start-event-pump canvas self)
      (ds/set-property self
        :context context
        :canvas canvas
        :text-renderer (gl/text-renderer Font/SANS_SERIF Font/BOLD 12))))

  (on :destroy
    (ds/delete self))

  (on :save
    (let [result (n/get-node-value self :saveable)]
      (when (= :ok result)
        (when-let [tracker (:dirty-tracker self)]
          (.markClean ^IDirtyable tracker))))))