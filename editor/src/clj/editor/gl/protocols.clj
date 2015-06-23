(ns editor.gl.protocols)

(defprotocol GlBind
  (bind [this gl] "Bind this object to the GPU context.")
  (unbind [this gl] "Unbind this object from the GPU context."))

(defprotocol GlEnable
  (enable [this gl] "Enable this object during the render of a specific frame.")
  (disable [this gl] "Disable this object after using it in a frame."))
