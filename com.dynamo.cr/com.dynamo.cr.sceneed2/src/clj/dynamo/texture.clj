(ns dynamo.texture
  "Schema, behavior, and type information related to textures."
  (:require [dynamo.types :refer :all]
            [schema.core :as s]
            [plumbing.core :refer [defnk]]))

;; Value
(def TextureImage
  {:path     s/Str
   :contents bytes})

;; Transform produces value
(defnk image-from-resource :- TextureImage
  [this]
  {:path (:resource this)
   :contents (byte-array [1 2 3 4 5])})

;; Behavior
(def Image
  {:properties {:resource (resource)}
   :transforms {:image #'image-from-resource}
   :cached     #{:image}})

(defn animation-frames [])

(def Animation
  {:inputs     {:images (as-schema [Image])}
   :properties {:fps (non-negative-integer :default 30)}
   :transforms {:frames #'animation-frames}})

(doseq [[v doc]
       {#'Animation
        "*behavior* - Used by [[dynamo.node/defnode]] to define the behavior for an animation resource.

Inputs:

* `:images` - vector of [[Image]]

Properties:

* `:fps` - non-negative integer, default 30

Transforms:

* `:frames` - see [[animation-frames]]"

        #'animation-frames
        "Returns the frames of the animation."

        #'Image
        "*behavior* - Used by [[dynamo.node/defnode]] to define the behavior for an image resource. Cached labels: `:image`.

Properties:

* `:resource` - string path to resource ([[dynamo.types/resource]])

Transforms:

* `:image` - returns `{:path path :contents byte-array}`, see [[image-from-resource]]"

        #'image-from-resource
        "Returns `{:path path :contents byte-array}` from an image resource."

        #'TextureImage
        "*schema* - schema for texture image resources. Requires `{:path ([[dynamo.types/string]]), :contents ([[dynamo.types/bytes]])}`."
          }]
  (alter-meta! v assoc :doc doc))
