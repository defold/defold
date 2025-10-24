(ns editor.shaders
  (:require [editor.gl.shader :as shader]))

(def basic-color-local-space
  (shader/classpath-shader
    {:coordinate-space :coordinate-space-local
     :uniforms {"mtx_world_view_proj" :world-view-proj}}
    "shaders/basic-color.vp"
    "shaders/basic-color.fp"))

(def basic-color-world-space
  (shader/classpath-shader
    {:coordinate-space :coordinate-space-world
     :uniforms {"mtx_world_view_proj" :view-proj}}
    "shaders/basic-color.vp"
    "shaders/basic-color.fp"))

(def basic-selection-local-space
  (shader/classpath-shader
    {:coordinate-space :coordinate-space-local
     :uniforms {"mtx_world_view_proj" :world-view-proj}}
    "shaders/basic-selection.vp"
    "shaders/basic-selection.fp"))

(def basic-selection-world-space
  (shader/classpath-shader
    {:coordinate-space :coordinate-space-world
     :uniforms {"mtx_world_view_proj" :view-proj}}
    "shaders/basic-selection.vp"
    "shaders/basic-selection.fp"))

(def basic-selection-paged-local-space
  (shader/classpath-shader
    {:coordinate-space :coordinate-space-local
     :max-page-count shader/max-array-samplers
     :uniforms {"mtx_world_view_proj" :world-view-proj}}
    "shaders/basic-selection-paged.vp"
    "shaders/basic-selection-paged.fp"))

(def basic-selection-paged-world-space
  (shader/classpath-shader
    {:coordinate-space :coordinate-space-world
     :max-page-count shader/max-array-samplers
     :uniforms {"mtx_world_view_proj" :view-proj}}
    "shaders/basic-selection-paged.vp"
    "shaders/basic-selection-paged.fp"))

(def basic-texture-local-space
  (shader/classpath-shader
    {:coordinate-space :coordinate-space-local
     :uniforms {"mtx_world_view_proj" :world-view-proj}}
    "shaders/basic-texture.vp"
    "shaders/basic-texture.fp"))

(def basic-texture-world-space
  (shader/classpath-shader
    {:coordinate-space :coordinate-space-world
     :uniforms {"mtx_world_view_proj" :view-proj}}
    "shaders/basic-texture.vp"
    "shaders/basic-texture.fp"))

(def basic-texture-color-local-space
  (shader/classpath-shader
    {:coordinate-space :coordinate-space-local
     :uniforms {"mtx_world_view_proj" :world-view-proj}}
    "shaders/basic-texture-color.vp"
    "shaders/basic-texture-color.fp"))

(def basic-texture-color-world-space
  (shader/classpath-shader
    {:coordinate-space :coordinate-space-world
     :uniforms {"mtx_world_view_proj" :view-proj}}
    "shaders/basic-texture-color.vp"
    "shaders/basic-texture-color.fp"))

(def basic-texture-paged-local-space
  (shader/classpath-shader
    {:coordinate-space :coordinate-space-local
     :max-page-count shader/max-array-samplers
     :uniforms {"mtx_world_view_proj" :world-view-proj}}
    "shaders/basic-texture-paged.vp"
    "shaders/basic-texture-paged.fp"))

(def basic-texture-paged-world-space
  (shader/classpath-shader
    {:coordinate-space :coordinate-space-world
     :max-page-count shader/max-array-samplers
     :uniforms {"mtx_world_view_proj" :view-proj}}
    "shaders/basic-texture-paged.vp"
    "shaders/basic-texture-paged.fp"))

(def basic-texture-paged-color-local-space
  (shader/classpath-shader
    {:coordinate-space :coordinate-space-local
     :max-page-count shader/max-array-samplers
     :uniforms {"mtx_world_view_proj" :world-view-proj}}
    "shaders/basic-texture-paged-color.vp"
    "shaders/basic-texture-paged-color.fp"))

(def basic-texture-paged-color-world-space
  (shader/classpath-shader
    {:coordinate-space :coordinate-space-world
     :max-page-count shader/max-array-samplers
     :uniforms {"mtx_world_view_proj" :view-proj}}
    "shaders/basic-texture-paged-color.vp"
    "shaders/basic-texture-paged-color.fp"))

(def mesh-preview-local-space
  (shader/classpath-shader
    {:coordinate-space :coordinate-space-local
     :uniforms {"mtx_view" :view
                "mtx_proj" :projection}}
    "shaders/mesh-preview.vp"
    "shaders/mesh-preview.fp"))

(def mesh-selection-local-space
  (shader/classpath-shader
    {:coordinate-space :coordinate-space-local
     :uniforms {"mtx_view_proj" :view-proj}}
    "shaders/mesh-selection.vp"
    "shaders/mesh-selection.fp"))
