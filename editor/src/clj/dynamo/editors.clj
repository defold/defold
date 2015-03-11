(ns dynamo.editors
  "Not used at present. Kept here for reference until we're done integrating the javafx test branch with the main body of the project."
)

(comment
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
    (property first-resize s/Bool (default true) (visible false))

    (output render-data t/RenderData iuse/produce-render-data)
    (output aabb AABB (fnk [aabb] aabb))
    (output glcontext GLContext (fnk [context] context))

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
          (g/set-property camera-node :camera new-camera)
          (when (:first-resize self)
            (g/set-property self :first-resize false)
            (ds/send-after self {:type :reframe}))))

    (on :reframe
        (let [camera-node (ds/node-feeding-into self :view-camera)
              camera      (n/get-node-value camera-node :camera)
              aabb        (n/get-node-value self :aabb)]
          (when aabb ;; there exists an aabb to center on
            (g/set-property camera-node :camera (c/camera-orthographic-frame-aabb camera aabb))))))

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

    (trigger view-scope :input-connections iuse/send-view-scope-message)

    #_(input outline-tree t/OutlineItem)
    #_(property outline-widget-tree s/Any)
    #_(output update-outline-view s/Any (fnk [outline-widget-tree outline-tree]
                                             (when (and outline-widget-tree outline-tree)
                                               (outline/set-outline-tree-gui-data outline-widget-tree outline-tree))))

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
          (texture/initialize gl)
          (g/set-property self
                           ;; :outline-widget-tree (outline/outline-tree-gui)
                           :context context
                           :canvas canvas
                           :text-renderer (gl/text-renderer Font/SANS_SERIF Font/BOLD 12))))

    (on :destroy
        #_(when-let [widget-tree (:outline-widget-tree self)]
            (outline/close-outline-tree-gui-data widget-tree))
        (when (:context self)
          (texture/unload-all (.. ^GLContext (:context self) getGL)))

        (ds/delete self))

    (on :save
        (n/get-node-value self :saveable))))
