(ns editor.gl.protocols)

(set! *warn-on-reflection* true)

(defprotocol GlBind
  (bind [this gl render-args] "Bind this object to the GPU context.")
  (unbind [this gl] "Unbind this object from the GPU context."))

(defprotocol ShaderVariables
  (get-attrib-location [this gl name])
  (set-uniform [this gl name val]))
