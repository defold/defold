(ns editor.gui-clipping
  (:require [dynamo.graph :as g])
  (:import [javax.media.opengl GL GL2]
           [clojure.lang ExceptionInfo]))

(set! *warn-on-reflection* true)

(defn- bit-range [v]
  (loop [v v
         r 0]
    (if (> v 0)
      (recur (bit-shift-right v 1) (inc r))
      r)))

(defn- bit-mask [bits]
  (dec (bit-shift-left 1 bits)))

(def ^:private clipping-state {:write-mask 0xff
                               :mask 0
                               :ref-val 0
                               :color-mask [false false false false]})
(def ^:private clipping-child-state {:write-mask 0
                                     :mask 0
                                     :ref-val 0
                                     :color-mask [true true true true]})

(def ^:private clipping-state-path [:renderable :user-data :clipping-state])
(def ^:private clipping-child-state-path [:renderable :user-data :clipping-child-state])

(defn- non-inv-states [index bit-range bit-field-offset parent-c]
  (let [state (assoc clipping-state
                     :mask (:mask parent-c 0)
                     :ref-val (bit-or (bit-shift-left (inc index) bit-field-offset)
                                      (:ref-val parent-c 0)))]
    [state
     (assoc clipping-child-state
            :mask (bit-or (bit-shift-left (bit-mask bit-range) bit-field-offset)
                          (:mask state))
            :ref-val (:ref-val state))]))

(defn- inv-states [offset bit-field-offset parent-c]
  (let [state (assoc clipping-state
                     :mask (:mask parent-c 0)
                     :ref-val (bit-or (bit-shift-left 1 (- 7 offset))
                                      (bit-and (bit-mask bit-field-offset)
                                               (:ref-val parent-c 0))))]
    [state
     (assoc clipping-child-state
            :mask (bit-or (:ref-val state) (:mask parent-c 0))
            :ref-val (:ref-val parent-c 0))]))

(defn- set-states [s [state child-state]]
  (-> s
    (update-in [:renderable :user-data] assoc :clipping-state state)
    (update-in [:renderable :batch-key] assoc :clipping-state state)
    (cond->
      child-state
      (update-in [:renderable :user-data] assoc :clipping-child-state child-state))))

(defn root-clippers [scene]
  (when (some (fn [s] (get-in s [:renderable :user-data :clipping])) (tree-seq (constantly true) :children scene))
    (filterv (fn [[s _]] (some? (get-in s [:renderable :user-data :clipping])))
             (tree-seq (fn [[s _]] (nil? (get-in s [:renderable :user-data :clipping])))
                       (fn [[s path]] (map-indexed (fn [i s] [s (conj (or path []) :children i)]) (:children s))) [scene nil]))))

(defn- inv-clipper? [scene]
  (get-in scene [:renderable :user-data :clipping :inverted]))

(defn- non-inv-clipper? [scene]
  (let [clipping (get-in scene [:renderable :user-data :clipping])]
    (and clipping (not (:inverted clipping)))))

(defn- visible-clipper? [scene]
  (get-in scene [:renderable :user-data :clipping :visible]))

(defn- ->visible-scene [scene]
  (let [clipping-state (-> (get-in scene [:renderable :user-data :clipping-state])
                         (assoc :write-mask 0 :color-mask [true true true true]))]
    (-> scene
     (update :renderable assoc :index 1)
     (update-in [:renderable :batch-key] assoc :clipping-state clipping-state)
     (update-in [:renderable :user-data] dissoc :clipping)
     (update-in [:renderable :user-data] assoc :clipping-state clipping-state)
     (dissoc scene :children :transform))))

(def update-scope)

(defn- ctx-update-scope [ctx scenes bit-field-offset parent-s]
  (let [parent-c (get-in parent-s clipping-child-state-path)]
    (reduce (fn [ctx scene]
              (cond
                (inv-clipper? scene) (let [offset (:inv-offset ctx)
                                           new-scene (-> scene
                                                       (set-states (inv-states offset bit-field-offset parent-c)))
                                           sub-ctx (-> ctx
                                                     (assoc :scenes [])
                                                     (update :inv-offset inc)
                                                     (ctx-update-scope (:children scene) bit-field-offset new-scene))
                                           children (vec (cond-> (:scenes sub-ctx)
                                                          (visible-clipper? scene)
                                                          ((partial cons (->visible-scene new-scene)))))]
                                       (-> ctx
                                         (update :scenes conj (assoc new-scene :children children))
                                         (merge (select-keys sub-ctx [:inv-offset :non-inv-index]))))
                (non-inv-clipper? scene) (let [index (:non-inv-index ctx)
                                               bit-range (bit-range (:non-inv-count ctx))
                                               new-scene (-> scene
                                                           (set-states (non-inv-states index bit-range bit-field-offset parent-c)))
                                               children (-> (update-scope (:children new-scene) (+ bit-range bit-field-offset) (:inv-count ctx) new-scene)
                                                          (cond->
                                                            (visible-clipper? scene)
                                                            ((partial cons (->visible-scene new-scene))))
                                                          vec)]
                                           (-> ctx
                                             (update :scenes conj (assoc new-scene :children children))
                                             (update :non-inv-index inc)))
                true (let [new-scene (let [state (get-in parent-s clipping-child-state-path)]
                                       (cond-> scene
                                         state
                                         (set-states [state nil])))
                           sub-ctx (-> ctx
                                     (assoc :scenes [])
                                     (ctx-update-scope (:children scene) bit-field-offset new-scene))]
                       (-> ctx
                         (update :scenes conj (assoc new-scene :children (:scenes sub-ctx)))
                         (merge (select-keys sub-ctx [:inv-offset :non-inv-index]))))))
            ctx scenes)))

(defn- bit-consumption [ctx]
  (+ (bit-range (:non-inv-count ctx)) (:inv-count ctx)))

(defn- ctx-count-clippers [ctx scenes available-bits]
  (reduce (fn [ctx scene]
            (let [ctx (cond
                        (inv-clipper? scene) (try
                                               (-> ctx
                                                 (update :inv-count inc)
                                                 (ctx-count-clippers (:children scene) available-bits))
                                               (catch ExceptionInfo e
                                                 (throw (ex-info "bit overflow" {:source scene}))))
                        (non-inv-clipper? scene) (update ctx :non-inv-count inc)
                        true (ctx-count-clippers ctx (:children scene) available-bits))]
              (if (> (bit-consumption ctx) available-bits)
                (throw (ex-info "bit overflow" {:source scene}))
                ctx)))
          ctx scenes))

(defn- update-scope
  [scenes bit-field-offset inv-count parent-s]
  (try
    (-> {:scenes []
         :inv-count 0
         :inv-offset inv-count
         :non-inv-count 0
         :non-inv-index 0}
      (ctx-count-clippers scenes (- 8 bit-field-offset inv-count))
      (ctx-update-scope scenes bit-field-offset parent-s)
      :scenes)
    (catch ExceptionInfo e
      (if parent-s
        (throw (ex-info "bit overflow" {:source parent-s}))
        (let [source-id (get-in (ex-data e) [:source :node-id])]
          (update-scope (vec (take-while (fn [s] (not= (:node-id s) source-id)) scenes)) bit-field-offset inv-count parent-s))))))

(defn setup-states [scene]
  (let [new-roots (loop [all-roots [(root-clippers scene)]
                         clear? false
                         result []]
                    (if (empty? all-roots)
                      result
                      (let [roots (first all-roots)]
                        (if (empty? roots)
                          (recur (rest all-roots) clear? result)
                          (let [new-scenes (update-scope (mapv first roots) 0 0 nil)
                                new-roots (mapv (fn [[s p] new-s] [new-s p]) roots new-scenes)
                                new-root-count (count new-roots)]
                            (if (= new-root-count 0)
                              (if (> (count roots) 1)
                                (recur (into [(subvec roots 0 1) (subvec roots 1)] (rest all-roots)) true result)
                                (g/error-severe "bit overflow" {:type :bit-overflow :source-id (get-in (last roots) [0 :node-id])}))
                              (let [new-roots (cond-> new-roots
                                                clear?
                                                (assoc-in [0 0 :renderable :user-data :clipping-state :clear] true))]
                                (recur (into [(subvec roots new-root-count)] (rest all-roots)) true (into result new-roots)))))))))]
    (if (g/error? new-roots)
      new-roots
      (reduce (fn [s [r p]] (assoc-in s p r)) scene new-roots))))

(defn- ->scope [index]
  {:root-layer nil :root-index index :index 1})

(defn- scope-inc [scope]
  (update scope :index inc))

(defn scene-key [scene]
  [(:node-id scene) (get-in scene [:renderable :user-data :clipping-state])])

(defn- render-key [scope layer index]
  (if scope
    [(:root-layer scope) (:root-index scope) (:index scope) layer index]
    [layer index 0 0 0]))

(defn render-keys [ctx scenes]
  (loop [ctx ctx
         scenes scenes
         index (:offset ctx)]
    (if-let [scene (first scenes)]
      (let [layer (get-in scene [:renderable :layer-index])]
        (cond
          (or (inv-clipper? scene) (non-inv-clipper? scene))
          (let [root? (nil? (:scope ctx))
                scope (if root?
                        (->scope index)
                        (scope-inc (:scope ctx)))
                index (if root? (inc index) index)
                sub-ctx (-> ctx
                          (update :render-keys assoc (scene-key scene) (render-key scope nil 0))
                          (assoc :offset 1)
                          (assoc :scope scope)
                          (assoc :layer-index layer)
                          (render-keys (:children scene)))
                new-ctx (-> ctx
                          (merge (select-keys sub-ctx [:render-keys]))
                          (cond->
                            (not root?)
                            (assoc :scope (scope-inc (:scope sub-ctx)))
                            ;; haxx for clipper visible entries
                            (and (= (:node-id scene) (get-in scene [:children 0 :node-id]))
                                 (let [layer (get-in scene [:children 0 :renderable :layer-index])]
                                   (and layer (>= layer 0))))
                            ((fn [ctx]
                               (let [child (get-in scene [:children 0])
                                     key (scene-key child)
                                     [_ _ _ layer index] (get-in ctx [:render-keys key])]
                                 (update ctx :render-keys assoc key (render-key (:scope sub-ctx) layer index)))))))]
            (recur new-ctx (rest scenes) index))

          true
          (let [layer (or layer (:layer-index ctx))
                sub-ctx (-> ctx
                          (update :render-keys assoc (scene-key scene) (render-key (:scope ctx) layer index))
                          (assoc :offset (inc index))
                          (assoc :layer-index layer)
                          (render-keys (:children scene)))
                new-ctx (merge ctx (select-keys sub-ctx [:offset :render-keys]))]
            (recur new-ctx (rest scenes) (:offset new-ctx)))))
      ctx)))

(defn scene->render-keys [scene]
  (let [ctx (-> {:render-keys {(scene-key scene) [nil nil nil nil nil]}
                 :offset 0
                 :scope nil
                 :layer-index nil}
              (render-keys (:children scene)))]
    (:render-keys ctx)))

(defn setup-gl [^GL2 gl state]
  (when state
    (.glEnable gl GL/GL_STENCIL_TEST)
    (.glStencilOp gl GL/GL_KEEP GL/GL_REPLACE GL/GL_REPLACE)
    (.glStencilFunc gl GL2/GL_EQUAL (:ref-val state) (:mask state))
    (.glStencilMask gl (:write-mask state))
    (when (:clear state)
      (.glClear gl GL/GL_STENCIL_BUFFER_BIT))
    (let [[c0 c1 c2 c3] (:color-mask state)]
      (.glColorMask gl c0 c1 c2 c3))))

(defn restore-gl [^GL2 gl state]
  (when state
    (.glDisable gl GL/GL_STENCIL_TEST)
    (.glColorMask gl true true true true)))
