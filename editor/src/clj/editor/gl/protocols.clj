(ns editor.gl.protocols)

(defprotocol GlBind
  (bind [this gl] "Bind this object to the GPU context.")
  (unbind [this gl] "Unbind this object from the GPU context."))
