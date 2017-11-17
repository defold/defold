(ns editor.gl.protocols)

(set! *warn-on-reflection* true)

(defprotocol GlBind
  (bind [this gl render-args] "Bind this object to the GPU context.")
  (unbind [this gl render-args] "Unbind this object from the GPU context."))
