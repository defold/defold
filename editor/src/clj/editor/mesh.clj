(ns editor.mesh
  (:require [clojure.java.io :as io]
            [clojure.xml :as xml]
            [clojure.string :as str]
            [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [clojure.data.json :as json]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
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
           [javax.vecmath Matrix4d Point3d]))

(set! *warn-on-reflection* true)

(def mesh-icon "icons/32/Icons_27-AT-Mesh.png")

(vtx/defvertex vtx-pos-nrm-tex
  (vec3 position)
  (vec3 normal)
  (vec2 texcoord0))

(defn render-mesh [^GL2 gl render-args renderables rcount]
  (let [pass (:pass render-args)
        renderable (first renderables)
        user-data (:user-data renderable)
        vbs (:vbs user-data)]
    (cond
      (= pass pass/outline)
      (let [outline-vertex-binding (vtx/use-with ::mesh-outline (render/gen-outline-vb renderables rcount) render/shader-outline)]
        (gl/with-gl-bindings gl render-args [render/shader-outline outline-vertex-binding]
          (gl/gl-draw-arrays gl GL/GL_LINES 0 (* rcount 8))))

      (= pass pass/transparent)
      (doseq [vb vbs]
        (prn "rend-t" (type vb))
        #_(prn "vb" vb)
        (let [vertex-binding (vtx/use-with ::mesh-trans vb render/shader-tex-tint)]
          (gl/with-gl-bindings gl render-args [render/shader-tex-tint vertex-binding]
            (.glBlendFunc gl GL/GL_ONE GL/GL_ONE_MINUS_SRC_ALPHA)
            (shader/set-uniform render/shader-tex-tint gl "texture" 0)
            (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (count vb))
            (.glBlendFunc gl GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA))))

      (= pass pass/selection)
      (doseq [vb vbs]
        (let [vertex-binding (vtx/use-with ::mesh-selection vb render/shader-tex-tint)]
          (gl/with-gl-bindings gl render-args [render/shader-tex-tint vertex-binding]
            (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (count vb))))))))

(defn- build-mesh [self basis resource dep-resources user-data]
  (let [content (:content user-data)]
    {:resource resource :content (protobuf/map->bytes Mesh$MeshDesc content)}))

(g/defnk produce-build-targets [_node-id resource content]
  [{:node-id _node-id
   :resource (workspace/make-build-resource resource)
   :build-fn build-mesh
   :user-data {:content content}}])

(defn- elements [node tag]
  (filter #(= tag (:tag %)) (:content node)))

(defn- element [node tag]
  (first (elements node tag)))

(defn- input [inputs semantic]
  (first (filter #(= semantic (get-in % [:attrs :semantic])) inputs)))

(defn- source [sources input]
  (let [source-key (subs (get-in input [:attrs :source]) 1)]
    (get sources source-key)))

(defn- offset [input]
  (Integer/parseInt (get-in input [:attrs :offset] "0")))

(defn- source->floats [source]
  (let [floats (first (-> source (element :float_array) (:content)))]
    (mapv #(Float/parseFloat %) (str/split (str/triml floats) #"\s+"))))

(defn- extract [input source comp-count tri-count tri-indices stride factor default]
  (let [factor (or factor 1.0)
        src-floats (source->floats source)]
    (loop [tris (range tri-count)
           values (transient [])]
      (if-let [tri (first tris)]
        (let [tri-index (+ (* tri stride 3) (offset input))
              values (reduce (fn [values v-index]
                               (let [v-offset (* comp-count (get tri-indices (+ tri-index (* v-index stride))))]
                                 (reduce (fn [values comp-index]
                                           (conj! values (* factor (get src-floats (+ v-offset comp-index) (get default comp-index)))))
                                         values (range comp-count))))
                             values (range 3))]
          (recur (rest tris) values))
        (persistent! values)))))

(g/defnk produce-content [resource]
  (let [collada (xml/parse (io/input-stream resource))
        meter (Double/parseDouble (-> collada (element :asset) (element :unit) (get-in [:attrs :meter] "1")))
        geometry (-> collada (element :library_geometries) (element :geometry))
        name (get-in geometry [:attrs :name] "Unnamed")
        mesh (-> collada (element :library_geometries) (element :geometry) (element :mesh))
        sources-map (into {} (map #(do [(get-in % [:attrs :id]) %]) (elements mesh :source)))
        vertices (element mesh :vertices)
        triangles (or (element mesh :triangles) (element mesh :polylist))
        vertices-inputs (-> mesh (element :vertices) (elements :input))
        tri-inputs (elements triangles :input)
        pos-input (input vertices-inputs "POSITION")
        vertex-input (input tri-inputs "VERTEX")
        normal-input (or
                       (input tri-inputs "NORMAL")
                       (input vertices-inputs "NORMAL"))
        texcoord-input (input tri-inputs "TEXCOORD")
        stride (inc (reduce max 0 (map offset tri-inputs)))
        pos-source (source sources-map pos-input)
        normal-source (source sources-map normal-input)
        texcoord-source (source sources-map texcoord-input)
        tri-count (Integer/parseInt (get-in triangles [:attrs :count] "0"))
        tri-indices (mapv #(Integer/parseInt %) (str/split (str/triml (first (-> triangles (element :p) (:content)))) #"\s+"))
        positions (extract vertex-input pos-source 3 tri-count tri-indices stride meter [0 0 0])
        normals (extract normal-input normal-source 3 tri-count tri-indices stride nil [0 0 1])
        texcoords (extract texcoord-input texcoord-source 2 tri-count tri-indices stride nil [0 0 1])]
    {:primitive-type :triangles
     :components [{:name name
                   :positions positions
                   :normals normals
                   :texcoord0 texcoords
                   :primitive-count (* tri-count 3)}]}))

(g/defnk produce-scene [_node-id aabb vbs]
  (prn "prod-ty" (type (first vbs)))
  {:node-id _node-id
   :aabb aabb
   :renderable {:render-fn render-mesh
                :batch-key _node-id
                :select-batch-key _node-id
                :user-data {:vbs vbs}
                :passes [pass/transparent pass/selection pass/outline]}})

(defn- component->vb [c]
  (let [vcount (* 3 (:primitive-count c))]
    (when (> vcount 0)
      (loop [vb (->vtx-pos-nrm-tex vcount)
             ps (partition 3 (:positions c))
             ns (partition 3 (:normals c))
             ts (partition 2 (:texcoord0 c))]
        (if-let [[x y z] (first ps)]
          (let [[nx ny nz] (first ns)
                [tu tv] (first ts)]
            (recur (conj! vb [x y z nx ny nz tu tv]) (rest ps) (rest ns) (rest ts)))
          (persistent! vb))))))

(g/defnode MeshNode
  (inherits project/ResourceNode)

  (output content g/Any :cached produce-content)
  (output aabb AABB :cached (g/fnk [content]
                                   (reduce (fn [aabb [x y z]]
                                             (geom/aabb-incorporate aabb x y z))
                                           (geom/null-aabb)
                                           (partition 3 (get-in content [:components 0 :positions])))))
  (output build-targets g/Any :cached produce-build-targets)
  (output vbs g/Any #_:cached (g/fnk [content]
                                     (let [vbs (loop [components (:components content)
                                                      vbs []]
                                                 (if-let [c (first components)]
                                                   (let [vb (component->vb c)]
                                                     (prn "gen-t" (type vb))
                                                     (recur (rest components) (if vb (conj vbs vb) vbs)))
                                                   vbs))]
                                       (prn "gen-t-real" (type (first vbs)))
                                       [(list 1 2)])))
  (output scene g/Any #_:cached produce-scene))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "dae"
                                    :label "Collada Mesh"
                                    :build-ext "meshc"
                                    :node-type MeshNode
                                    :icon mesh-icon
                                    :view-types [:scene :text]
                                    :view-opts {:scene {:grid true}}))
