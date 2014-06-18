(ns dynamo.node
  (:require [schema.core :as s]
            [schema.macros :as sm]
            [plumbing.core :refer [fnk defnk]]
            [internal.graph.lgraph :as lg]
            [internal.graph.dgraph :as dg]
            [camel-snake-kebab :refer [->kebab-case]])
  (:import [java.awt.image BufferedImage]))

(def Icon s/Str)
(def NodeRef s/Int)

(def OutlineItem
  {:label    s/Str
   :icon     Icon
   :node-ref NodeRef
   :children [(s/recursive #'OutlineItem)]})

(defn list-of [x] [x])
(defn map-of  [kt vt] {kt vt})
(defn number  [& {:as opts}] {:schema s/Num})
(defn error-message [m] {:error-message m})
(defn non-negative-integer [] (assoc (number) :default 0 :such-that #(<= 0 %) :or-else (error-message "Value must be greater than or equal to zero")))
(defn isotropic-scale []  (assoc (number) :default 1.0 :such-that #(<= 0 %) :or-else (error-message "Value must be greater than zero")))

(sm/defrecord Quaternion [a :- s/Num, b :- s/Num, c :- s/Num, d :- s/Num])
(sm/defrecord Vector3 [x :- s/Num, y :- s/Num, z :- s/Num])

(def IsotropicallyScalable
  {:properties {:isotropic-scale (isotropic-scale)}})

(def Rotatable
  {:properties {:rotation Quaternion}})

(def Translatable
  {:properties {:translation Vector3}})

(def OutlineParent
  {:inputs  {}
   :transforms {:tree (with-meta (fn tree [this g children]
                                       {:label "my name" :icon "my type of icon" :node-ref (:id this) :children children})
                                 {::schema {:no false}})}})

(comment
  
  (fnk tree :- OutlineItem [this g children :- [OutlineItem]]
                           {:label "my name" :icon "my type of icon" :node-ref (:id this) :children children})
  
  )


(defn node-inputs [v] (into #{} (keys (:inputs v))))
(defn node-outputs [v] (into #{} (keys (:transforms v))))

(defn get-input [])
(defn refresh-inputs [g n i])


#_(defn refresh-inputs [g node-id input] "Update connections from input providers"
   (doseq [id (downstream g node-id input)]
     (do-something g id)))

(defprotocol Node
  (value [this g output default] "Produce the value for the named output. Supplies nil if the output cannot be produced.")
  (properties [this] "Produce a description of properties supported by this node."))

#_(defn make-atlas-node
   ([] (make-atlas-node 0 0))
   ([margin extrude-borders]
     (->AtlasNode {:assets [OutlineItem]} 
                  {:tree (fnk tree :- OutlineItem [this g assets :- [OutlineItem]] {:label "my name" ,,,})
                   :textureset (fnk pack :- BufferedImage [this g assets :- [BufferedImage]])} 
                  margin extrude-borders)))

(defn merge-behaviors [behaviors]
  (apply (partial merge-with merge)
         (map #(cond 
                 (symbol? %) (var-get (resolve %))
                 (var? %)    (var-get (resolve %))
                 (map? %)    %
                 :else       (throw (ex-info (str "Unacceptable behavior " %) :argument %)))
              behaviors)))

(defn- property-symbols [behavior]
  (->> (:properties behavior)
    (keys)
    (map name)
    (map symbol)))

(defn state-vector [behavior]
  (into [] (list* 'inputs 'transforms  (property-symbols behavior))))

(defn generate-type [name behavior]
  (let [t 'this#
        g 'g#
        o 'output#
        d 'default#]
    (list 'defrecord name (state-vector behavior)
          'Node
          `(value [~t ~g ~o ~d]
                  (cond
                    ~@(mapcat (fn [x] [(list '= o x) (list 'prn x)]) (keys (:outputs behavior)))
                    :else ~d))
          `(properties [~t]
                       ~(:properties behavior)))))

(defn- wire-up [selection input new-node output]
  (let [g           (first selection)
        target-node (first (second selection))
        g-new       (lg/add-labeled-node g (node-inputs new-node) (node-outputs new-node) new-node)
        new-node-id (dg/last-node-added g-new)]
      (-> g
        (lg/connect new-node-id output target-node input)
        (refresh-inputs target-node input))))

(defn- unwire [selection input source]
  (let [g           (first selection)
        target-node (first (second selection))
        g-new       (dg/for-graph g [l (lg/source-labels g target-node :input source)]
                                  (lg/disconnect g source l target-node input))]
    (refresh-inputs g target-node input))
  )

(defn input-mutators [input]
  (let [adder (symbol (str "add-to-" (name input)))
        remover (symbol (str "remove-from-" (name input)))]
    (list
      `(defn ~adder [~'selection ~'new-node ~'output]
         (wire-up ~'selection ~input ~'new-node ~'output))
      `(defn ~remover [~'selection ~'new-node]
         (unwire ~'selection ~input ~'new-node)))))

(defn generate-property-mutators [name behavior]
  [])

(defn generate-constructor [name behavior]
  (let [ctor        (symbol (str 'make- (->kebab-case (str name))))
        record-ctor (symbol (str 'map-> name))]
    (list 'defn ctor
          (str "Constructor for " name ", using default values for all properties.")
          []
          (list record-ctor behavior))))

(comment

  
  
  (defn make-atlas-node
    ([] (make-atlas-node 0 0))
    ([margin extrude-borders]
      (->AtlasNode {:assets [OutlineItem]} 
                   {:tree (fnk tree :- OutlineItem [this g assets :- [OutlineItem]] {:label "my name" ,,,})
                    :textureset (fnk pack :- BufferedImage [this g assets :- [BufferedImage]])} 
                   margin extrude-borders)))


  
  )

(defmacro defnode [name & behaviors]
  (let [behavior (merge-behaviors behaviors)]
    `(let []
       ~(generate-type name behavior)
       ~@(mapcat input-mutators (keys (:inputs behavior)))
       ~@(generate-property-mutators name behavior)
       ~(generate-constructor name behavior))))

(clojure.walk/macroexpand-all 
  '(defnode AtlasNode 
     Rotatable 
     Translatable 
     OutlineParent
     {:inputs {:assets [OutlineItem]}}))

#_(defnode AtlasNode 
      Rotatable 
      Translatable 
      OutlineParent
      {:inputs {:assets [OutlineItem]}})


