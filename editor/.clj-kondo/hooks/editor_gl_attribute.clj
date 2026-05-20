(ns hooks.editor-gl-attribute
  (:require [clj-kondo.hooks-api :as api]))

(defn- base-location-node []
  (api/token-node (with-meta 'base-location {:tag 'long})))

(defn- gl-node []
  (api/token-node (with-meta 'gl {:tag 'com.jogamp.opengl.GL2})))

(defn- long-node [value]
  (api/list-node
    [(api/token-node 'long)
     (api/token-node value)]))

(defn- call-node [set-attribute-node arity]
  (api/list-node
    [set-attribute-node
     (gl-node)
     (base-location-node)
     (api/token-node 'value-array)
     (long-node arity)]))

(defn def-assign-attribute-fn [{:keys [node]}]
  (let [[_ name-node set-attribute-1-node set-attribute-2-node set-attribute-3-node set-attribute-4-node] (:children node)]
    {:node
     (api/list-node
       [(api/token-node 'defn)
        name-node
        (api/vector-node
          [(api/token-node 'value-array)
           (api/token-node 'vector-type)
           (gl-node)
           (base-location-node)])
        (api/list-node
          [(api/token-node 'case)
           (api/token-node 'vector-type)
           (api/token-node :vector-type-scalar)
           (call-node set-attribute-1-node 0)
           (api/token-node :vector-type-vec2)
           (call-node set-attribute-2-node 0)
           (api/token-node :vector-type-vec3)
           (call-node set-attribute-3-node 0)
           (api/token-node :vector-type-vec4)
           (call-node set-attribute-4-node 0)])])}))
