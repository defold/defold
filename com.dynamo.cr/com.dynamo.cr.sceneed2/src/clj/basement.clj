(ns basement)

(comment
  ;; This file explores alternate syntax for the "above the waterline" code.

  ;; in atlas.node?
  (Node Atlas <: OutlineNode, Renderable
        inputs:
        OutlineItem assets
        ImageSource images
        Animation animations

        properties:
        Int margin = 0
        Int extrude-borders = 0

        outputs:
        [cached] textureset = fn (pack-textures .margin .extrude-borders (consolidate .images .animations)))

  ;; in atlas.clj
  (node Atlas :extends Outline Renderable
        (inputs
         assets :many OutlineItem
         images :many ImageSource
         foo    :one Foo)

        (properties
         [margin :one Int :default 0 :must-be #(< 0 %)])

        (outputs
         [textureset :one TextureSet :using (pack-textures .margin .extrude-borders (consolidate .images .animations)) :cached])

        (produce textureset :one TextureSet :using (pack-textures .margin .extrude-borders (consolidate .images .animations)) :cached)

        (on-load
         (fn [this]
           (connect this :textureset (CompilerNode.) :texturesetc))))

  ;; in this version, each node type gets its own go-loop for
  ;; processing incoming events. (Possible event types are shown
  ;; below.)
  ;;
  ;; here, defnode is a macro that "compiles" this description into
  ;; the faceted behaviors we use today, plus an event-handling
  ;; function that runs a stateful go loop for each node.
  ;;
  ;; the go loop should look something like this pseudocode:
  ;;
  ;; (go-loop []
  ;;   (let [msg (<! sub-ch)]
  ;;     (when msg
  ;;       (let [nodeid (:node-id msg)
  ;;             proj   (:project-state msg)
  ;;             self   (lookup-node proj nodeid)
  ;;             txn    (case (:event msg)
  ;;                   'add-image   (do-add-image ...)
  ;;                   'post-create (do-post-create ...))]
  ;;         (transact proj txn)))))
  ;;
  ;; this means that processing a message results in a new transaction
  ;; to be applied to the project state. while processing the message,
  ;; the symbol 'self is bound to the node's current state. this makes
  ;; expressions like 'attach' read more nicely.
  ;;
  ;; sub-ch is a subscription to a pub/sub bus. the dispatch function
  ;; uses the node-id that the message addresses. thus, each node
  ;; effectively subscribes to its own channel.
  ;;
  ;; we can handle a large number
  ;; of nodes with go-loops because an idle loop does not consume a
  ;; thread. it only requires some memory for the automatically
  ;; generated state machine.
  (defnode Atlas
    (output 'textureset TextureSet
            [arglist]
            fn-tail)

    (input 'images [Image])
    (input 'animations [Animation])

    (property 'filename Path)
    (property 'margin Integer)
    (property 'extrude-borders Integer)

    (command 'add-animation ...)

    (command 'add-image
             (when-let [new-image (pickr ...)]
               (let [img (new ImageNode new-image)]
                 (attach img 'image self 'images)
                 (send self 'images-changed))))

    (on 'post-create
        (let [compiler (new AtlasCompiler)]
          (attach self 'textureset compiler 'textureset)
          (attach self 'filename compiler 'filename)))

    (on 'select ...)

    (on 'images-changed
        (invalidate 'textureset)
        (invalidate 'texturesetc)))


  ;; node events
  ;; selection
  :select
  :deselect

  ;; lifecycle
  :create
  :init
  :post-create
  :pre-destroy
  :destroy

  ;; tree manipulation
  :attach-to-parent
  :post-attach-to-parent
  :attach-child
  :post-attach-child
  :detach-child
  :post-detach-child
  :detach-from-parent
  :post-detach-from-parent

  ;; value changes
  :property-change
  :cache-invalidate

  ;; file interaction
  :load-source
  :save-source

  ;; clipboard (no need for specific copy or begin-drag ops, because everything is a value.)
  :can-paste?
  :accept-paste
  :can-drop?
  :accept-drop
  )
