(ns basement)

(comment

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
          (connect this :textureset (CompilerNode.) :texturesetc)))
      )


  )