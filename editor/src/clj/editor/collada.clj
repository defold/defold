(ns editor.collada
  (:require [clojure.java.io :as io]
            [clojure.xml :as xml]
            [clojure.string :as str]
            [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [clojure.data.json :as json]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.gl.texture :as texture]
            [editor.defold-project :as project]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [editor.gl.pass :as pass]
            [editor.geom :as geom]
            [editor.render :as render])
  (:import [com.dynamo.graphics.proto Graphics$Cubemap Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.dynamo.mesh.proto Mesh$MeshDesc]
           [com.jogamp.opengl.util.awt TextRenderer]
           [editor.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d]
           [java.text DecimalFormat DecimalFormatSymbols ParseException]))

(set! *warn-on-reflection* true)

(defn- ->decimal-format [separator]
  (doto (DecimalFormat.)
    (.setDecimalFormatSymbols (doto (DecimalFormatSymbols.)
                                (.setDecimalSeparator separator)
                                (.setExponentSeparator "e")))))

(defn- elements [node tag]
  (filter #(= tag (:tag %)) (:content node)))

(defn- element [node tag]
  (first (elements node tag)))

(defn- element-in [node tags]
  (loop [tags tags
         node node]
    (if-let [tag (first tags)]
      (recur (rest tags) (element node tag))
      node)))

(defn- attr
  ([node key]
    (attr node key nil))
  ([node key default]
    (get-in node [:attrs key] default)))

(defn- attr-int
  ([node key]
    (attr-int node key 0))
  ([node key default]
    (or (some-> (attr node key)
          Integer/parseInt)
        default)))

(defn- attr-double
  [node key default ^DecimalFormat decimal-format]
  (if-let [^String a (attr node key)]
    (.doubleValue (.parse decimal-format a))
    default))

(defn- input [inputs semantic]
  (first (filter #(= semantic (attr % :semantic)) inputs)))

(defn- source [sources input]
  (when input
    (let [source-key (subs (attr input :source) 1)]
      (get sources source-key))))

(defn- source->floats [source ^DecimalFormat decimal-format]
  (when-let [floats (first (-> source (element :float_array) (:content)))]
    (mapv #(-> (.parse decimal-format %)
             (.floatValue))
          (str/split (str/triml floats) #"\s+"))))

(def ^:private defaults {"POSITION" [0 0 0]
                         "NORMAL" [0 0 1]
                         "TEXCOORD" [0 0]})

(defn- extract [semantic sources comp-count tri-count]
  (get sources semantic
       (let [default (defaults semantic)]
         (-> (repeat (* comp-count tri-count) default)
           flatten
           vec))))

(defn ->mesh [stream]
  (let [collada (xml/parse stream)
        unit (element-in collada [:asset :unit])
        decimal-format (->decimal-format (if (-> (attr unit :meter)
                                               (str/includes? ","))
                                           \,
                                           \.))
        meter (attr-double unit :meter 1.0 decimal-format)
        geometry (element-in collada [:library_geometries :geometry])
        name (attr geometry :name "Unnamed")
        mesh (element geometry :mesh)
        sources-map (->> (elements mesh :source)
                      (map #(do [(get-in % [:attrs :id]) %]) )
                      (into {}))
        vertices-map (->> (elements mesh :vertices)
                       (map #(do [(get-in % [:attrs :id]) %]))
                       (into {}))
        triangles (or (element mesh :triangles) (element mesh :polylist) (element mesh :polygons))
        tri-inputs (elements triangles :input)
        tri-count (attr-int triangles :count 0)
        tri-indices (reduce (fn [res s]
                              (reduce (fn [res s] (conj res (Integer/parseInt s)))
                                      res (str/split (str/triml s) #"\s+")))
                            []
                            (mapcat :content (elements triangles :p)))
        index-stride (inc (reduce (fn [res input] (max res (attr-int input :offset 0))) 0 tri-inputs))
        sources (loop [inputs tri-inputs
                       result {}]
                  (if-let [input (first inputs)]
                    (let [semantic (attr input :semantic)
                          offset (attr-int input :offset 0)]
                      (if (= semantic "VERTEX")
                        (let [new-inputs (->> (-> (get vertices-map (subs (attr input :source) 1))
                                                (elements :input))
                                           (map (fn [i] (assoc-in i [:attrs :offset] (str offset)))))]
                          (recur (concat new-inputs (rest inputs)) result))
                        (let [input-source (source sources-map input)
                              stride (-> (element-in input-source [:technique_common :accessor])
                                       (attr-int :stride 0))
                              factor (if (= semantic "POSITION") meter 1.0)
                              default (get defaults semantic)
                              indices (->> (drop offset tri-indices)
                                        (take-nth index-stride))
                              values (->> (source->floats input-source decimal-format)
                                       (map * (repeat factor))
                                       (partition stride)
                                       (map vec)
                                       vec)
                              values (-> (mapv (partial get values) indices)
                                       flatten
                                       vec)
                              result (assoc result semantic values)]
                          (recur (rest inputs) result))))
                    result))]
    {:primitive-type :triangles
     :components [{:name name
                   :positions (extract "POSITION" sources 3 tri-count)
                   :normals (extract "NORMAL" sources 3 tri-count)
                   :texcoord0 (->> (extract "TEXCOORD" sources 2 tri-count)
                                (partition 2)
                                (mapcat (fn [[u v]] [u (- 1.0 v)]))
                                vec)
                   :primitive-count (* tri-count 3)}]}))
