(ns dynamo.gl.protocols)

(defprotocol GlEnable
  (enable [this]))

(defprotocol GlDisable
  (disable [this]))