(ns dynamo.gl.protocols)

(defprotocol GlInit
  (init [this]))

(defprotocol GlEnable
  (enable [this]))

(defprotocol GlDisable
  (disable [this]))