(ns editor.luart
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.reflect :as reflect])
  (:import [net.sandius.rembulan LuaObject StateContext Table Variable ByteString]
           [net.sandius.rembulan.compiler CompilerChunkLoader]
           [net.sandius.rembulan.env RuntimeEnvironments]
           [net.sandius.rembulan.exec CallException CallPausedException DirectCallExecutor]
           [net.sandius.rembulan.impl StateContexts NonsuspendableFunctionException DefaultTable]
           [net.sandius.rembulan.lib StandardLibrary]
           [net.sandius.rembulan.load ChunkLoader LoaderException]
           [net.sandius.rembulan.runtime LuaFunction AbstractFunctionAnyArg]))

(defn make-state ^StateContext [] (StateContexts/newDefaultInstance))
(defn make-env ^Table [^StateContext state] (.installInto (StandardLibrary/in (RuntimeEnvironments/system)) state))
(defn make-loader ^CompilerChunkLoader [^String root-class-prefix] (CompilerChunkLoader/of (.getContextClassLoader (Thread/currentThread)) root-class-prefix))
(defn load-text-chunk [^Table up-env ^CompilerChunkLoader loader ^String chunk-name ^String chunk] (.loadTextChunk loader (Variable. up-env) chunk-name chunk))
(defn direct-executor ^DirectCallExecutor [] (DirectCallExecutor/newExecutor))
(defn exec
  ([^StateContext state lua-function] (exec state lua-function []))
  ([^StateContext state lua-function args] (into [] (.call (direct-executor) state lua-function (into-array Object args)))))

(defn make-table ^Table [] (DefaultTable.))

(defn table-set! [^Table env name value]
  (.rawset env name value))

(defn target []
  (println "And, here we are, in clojure-land!!")
  [1 2])



(def array-of-objects-type (Class/forName "[Ljava.lang.Object;"))

(defmulti lua->clj class)
(defmethod lua->clj net.sandius.rembulan.ByteString [bs] (.decode bs))
(defmethod lua->clj java.lang.Long [n] n)
(defmethod lua->clj java.lang.Boolean [b] b)
(defmethod lua->clj java.lang.Double [d] d)
(defmethod lua->clj :default [v] v)

(defmulti clj->lua class)
(defmethod clj->lua String [s] s)
(defmethod clj->lua java.lang.Long [n] n)
(defmethod clj->lua java.lang.Boolean [b] b)
(defmethod clj->lua java.lang.Double [d] d)
(defmethod clj->lua clojure.lang.Ratio [r] (.doubleValue r))
(defmethod clj->lua :default [v] v)

(defn- next-lua-tbl-key [lua-tbl key]
  (try
    (if (nil? key)
      (.initialKey lua-tbl)
      (.successorKeyOf lua-tbl key))
    (catch IllegalArgumentException e
      nil)))

(defmethod lua->clj net.sandius.rembulan.Table [lua-tbl]
  (loop [tbl {}
         key nil]
    (if-let [key (next-lua-tbl-key lua-tbl key)]
      (do
        #_(println :lua-table->clj :clj-key (lua->clj key) :val (.rawget lua-tbl key) :val-type (type (.rawget lua-tbl key)))
        #_(def _last-val (.rawget lua-tbl key))
        (recur (assoc tbl (lua->clj key) (lua->clj (.rawget lua-tbl key))) key))
      tbl)))

(defmethod clj->lua clojure.lang.IPersistentMap [m]
  (reduce-kv (fn [lua-table k v]
;;               (println "translating" k " -> " v)
               (table-set! lua-table (clj->lua k) (clj->lua v))
               lua-table)
             (make-table)
             m))

(defn fill-return-buffer! [execution-context result]
  (cond
    (nil? result)
    nil

    (sequential? result)
    (do #_(println "result was seq")
        (.. execution-context getReturnBuffer (setToContentsOf (into-array Object (map clj->lua result)))))

    :else
    (.. execution-context getReturnBuffer (setTo (clj->lua result)))))

(defn wrap-fun [fun]
  (proxy [AbstractFunctionAnyArg] []
    (invoke
      ([execution-context]
       (fill-return-buffer! execution-context (fun)))
      ([execution-context arg1-or-args]
       (if (instance? array-of-objects-type arg1-or-args)
         (fill-return-buffer! execution-context (apply fun (map lua->clj (into [] arg1-or-args))))
         (fill-return-buffer! execution-context (fun (lua->clj arg1-or-args)))))
      ([execution-context arg1 arg2]
       (fill-return-buffer! execution-context (fun (lua->clj arg1) (lua->clj arg2))))
      ([execution-context arg1 arg2 arg3]
       (fill-return-buffer! execution-context (fun (lua->clj arg1) (lua->clj arg2) (lua->clj arg3))))
      ([execution-context arg1 arg2 arg3 arg4]
       (fill-return-buffer! execution-context (fun (lua->clj arg1) (lua->clj arg2) (lua->clj arg3) (lua->clj arg4))))
      ([execution-context arg1 arg2 arg3 arg4 arg5]
       (fill-return-buffer! execution-context (fun (lua->clj arg1) (lua->clj arg2) (lua->clj arg3) (lua->clj arg4) (lua->clj arg5)))))
    (resume [_execution-context _suspended-state]
      (throw (NonsuspendableFunctionException.)))))

(defmethod clj->lua clojure.lang.Keyword [kw]
  (string/replace (name kw) "-" "_"))

(defmethod clj->lua :default [wat]
;;  (println :default (type wat) wat)
  (cond
    (fn? wat)
    (wrap-fun wat)

    :else
    (assert false "unknown type")))

(defn arg-target [arg]
  (println "arg-target:" (type arg) arg))

(def editor-table
  {:target2 target
   :arg-target arg-target
   :nice true
   :graph {:project_id (bit-shift-left 1 56)}
   :a_string "caaaaaakes"})

(defn main []
  (let [state (make-state)
        env (make-env state)
        loader (make-loader "test")
        main (load-text-chunk env loader "main" (slurp "main.lua") #_"print('hello, world')")
        scope (make-table)]
    (table-set! env "editor" (clj->lua editor-table))
    (exec state main)

    (let [call-me (.rawget env "call_me")]
      (exec state call-me ["sir"]))))

(def gl-command-list (atom []))

(def noop (constantly nil))

(def render-table-constants
  {:BUFFER_DEPTH_BIT 0x00000100
   :BUFFER_STENCIL_BIT 0x00000400
   :BUFFER_COLOR_BIT 0x00004000

   :STATE_DEPTH_TEST 0x0B71
   :STATE_STENCIL_TEST 0x0B90
   :STATE_BLEND 0x0BE2
   :STATE_CULL_FACE 0x0B44
   :STATE_ALPHA_TEST 0x0BC0

   :BLEND_ZERO                           0
   :BLEND_ONE                            1
   :BLEND_SRC_COLOR                      0x0300
   :BLEND_ONE_MINUS_SRC_COLOR            0x0301
   :BLEND_SRC_ALPHA                      0x0302
   :BLEND_ONE_MINUS_SRC_ALPHA            0x0303
   :BLEND_DST_ALPHA                      0x0304
   :BLEND_ONE_MINUS_DST_ALPHA            0x0305})

(def render-table
  (merge
    render-table-constants
    {:predicate
     (fn [tbl] tbl)

     :set-depth-mask
     (fn [mask]
       (swap! gl-command-list conj [:set-depth-mask mask])
       nil)

     :set-stencil-mask
     (fn [mask]
       (swap! gl-command-list conj [:set-stencil-mask mask])
       nil)

     :clear
     (fn [clear-list]
       (swap! gl-command-list conj [:clear clear-list])
       nil)

     :draw
     (fn [pred]
       (swap! gl-command-list conj [:draw pred])
       nil)
     :draw-debug3d noop
     :draw-debug2d noop   

     :set-viewport noop
     :set-view noop
     :set-projection noop

     :get-window-width (fn [] 1024)
     :get-window-height (fn [] 768)

     :get-width (fn [] 1024)
     :get-height (fn [] 768)

     :disable-state
     (fn [state]
       (swap! gl-command-list conj [:disable-state state])
       nil)

     :enable-state
     (fn [state]
       (swap! gl-command-list conj [:enable-state state])
       nil)

     :set-blend-func
     (fn [source-factor dest-factor]
       (swap! gl-command-list conj [:set-blend-func source-factor dest-factor])
       nil)
     }))

(defn- complete-matrix [m]
  m)

(def vmath-table
  {:vector4 (fn [x y z w] {:x x :y y :z z})

   :matrix4
   (fn ([]
        (complete-matrix
          {:m11 0 :m12 0 :m13 0 :m14 0
            :m21 0 :m22 0 :m23 0 :m24 0
            :m31 0 :m32 0 :m33 0 :m34 0
            :m41 0 :m42 0 :m43 0 :m44 0}))
     ([other] other))
   
   :matrix4_orthographic
   (fn [left right bottom top near far]
     (complete-matrix
       {:m11 (/ 2 (- right left)) :m12 0 :m13 0 :m14 (- (/ (+ right left) (- right left)))
        :m21 0 :m22 (/ 2 (- top bottom)) :m23 0 :m24 (- (/ (+ top bottom) (- top bottom)))
        :m31 0 :m32 0 :m33 (- (/ 2 (- far near))) :m34 (- (/ (+ far near) (- far near)))
        :m41 0 :m42 0 :m43 0 :m44 1}))
   })

(def sys-table
  {:get-config (fn [setting default] default)
   })

(defn rs []
  (let [state (make-state)
        env (doto (make-env state)
              (table-set! "render" (clj->lua render-table))
              (table-set! "vmath" (clj->lua vmath-table))
              (table-set! "sys" (clj->lua sys-table))
              )
        loader (make-loader "editor")
        main (load-text-chunk env loader "render_script" (slurp "default.render_script"))]
    (exec state main)

    (let [script-init (.rawget env "init")
          script-update (.rawget env "update")
          self (make-table)]
      (reset! gl-command-list [])
      (exec state script-init [self])
      (exec state script-update [self])
      #_(clojure.pprint/pprint @gl-command-list)
      @gl-command-list
      )))

