(ns integration.reload-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system undo-stack]]
            [editor.math :as math]
            [editor.project :as project]
            [editor.protobuf :as protobuf]
            [editor.atlas :as atlas]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util])
  (:import [com.dynamo.gameobject.proto GameObject GameObject$CollectionDesc GameObject$CollectionInstanceDesc GameObject$InstanceDesc
            GameObject$EmbeddedInstanceDesc GameObject$PrototypeDesc]
           [com.dynamo.textureset.proto TextureSetProto$TextureSet]
           [com.dynamo.render.proto Font$FontMap]
           [com.dynamo.particle.proto Particle$ParticleFX]
           [com.dynamo.sound.proto Sound$SoundDesc]
           [com.dynamo.spine.proto Spine$SpineScene]
           [com.dynamo.mesh.proto Mesh$MeshDesc]
           [com.dynamo.model.proto Model$ModelDesc]
           [editor.types Region]
           [editor.workspace BuildResource]
           [editor.resource FileResource]
           [java.awt.image BufferedImage]
           [java.io File]
           [java.nio.file Files attribute.FileAttribute]
           [javax.imageio ImageIO]
           [javax.vecmath Point3d Matrix4d]
           [org.apache.commons.io FilenameUtils FileUtils]))

(def ^:dynamic *project-path*)

(defn- setup [ws-graph]
  (alter-var-root #'*project-path* (fn [_] (-> (Files/createTempDirectory "foo" (into-array FileAttribute []))
                                             (.toFile)
                                             (.getAbsolutePath))))
  (FileUtils/copyDirectory (File. "resources/reload_project") (File. *project-path*))
  (let [workspace (test-util/setup-workspace! ws-graph *project-path*)
        project (test-util/setup-project! workspace)]
    [workspace project]))

(defn- mkdirs [^File f]
  (let [parent (.getParentFile f)]
    (when (not (.exists parent))
      (.mkdirs parent))))

(defn- template [workspace name]
  (let [resource (workspace/file-resource workspace name)]
    (workspace/template (workspace/resource-type resource))))

(defn- write-file [workspace name content]
  (let [f (File. (File. *project-path*) name)]
    (mkdirs f)
    (if (not (.exists f))
      (.createNewFile f)
      (Thread/sleep 1100))
    (spit f content))
  (workspace/fs-sync workspace))

(defn- add-file [workspace name]
  (write-file workspace name (template workspace name)))

(defn- delete-file [workspace name]
  (let [f (File. (File. *project-path*) name)]
    (.delete f))
  (workspace/fs-sync workspace))

(defn- copy-file [workspace name new-name]
  (let [[f new-f] (mapv #(File. (File. *project-path*) %) [name new-name])]
    (FileUtils/copyFile f new-f))
  (workspace/fs-sync workspace))

(defn- move-file [workspace name new-name]
  (let [[f new-f] (mapv #(File. (File. *project-path*) %) [name new-name])]
    (FileUtils/moveFile f new-f)
    (workspace/fs-sync workspace true [[f new-f]])))

(defn- add-img [workspace name width height]
  (let [img (BufferedImage. width height BufferedImage/TYPE_INT_ARGB)
        type (FilenameUtils/getExtension name)
        f (File. (File. *project-path*) name)]
    (when (.exists f)
      (Thread/sleep 1100))
    (ImageIO/write img type f)
    (workspace/fs-sync workspace)))

(defn- has-undo? [project]
  (g/has-undo? (g/node-id->graph-id project)))

(defn- no-undo? [project]
  (not (has-undo? project)))

(deftest internal-file
  (with-clean-system
    (let [[workspace project] (setup world)]
      (testing "Add internal file"
               (add-file workspace "/test.collection")
               (let [node (project/get-resource-node project "/test.collection")]
                 (is (not (nil? node)))
                 (is (= "default" (g/node-value node :name))))
               (is (no-undo? project)))
      (testing "Change internal file"
               (write-file workspace "/test.collection" "name: \"test_name\"")
               (let [node (project/get-resource-node project "/test.collection")]
                 (is (not (nil? node)))
                 (is (= "test_name" (g/node-value node :name))))
               (is (no-undo? project)))
      (testing "Delete internal file"
               (delete-file workspace "/test.collection")
               (let [node (project/get-resource-node project "/test.collection")]
                 (is (nil? node)))
               (is (no-undo? project))))))

(deftest external-file
 (with-clean-system
   (let [[workspace project] (setup world)
         atlas-node-id (project/get-resource-node project "/atlas/empty.atlas")
         img-path "/test_img.png"
         anim-id (FilenameUtils/getBaseName img-path)]
     (is (not (nil? atlas-node-id)))
     (testing "Add external file, cleared history"
       (add-img workspace img-path 64 64)
       (let [node (project/get-resource-node project img-path)]
         (is (some? node)))
       (is (no-undo? project)))
     (testing "Reference it, node added and linked"
       (g/transact
        (atlas/add-images atlas-node-id [(workspace/resolve-resource (g/node-value atlas-node-id :resource) img-path)]))
       (is (has-undo? project))
       (let [undo-count (count (undo-stack (g/node-id->graph-id project)))
             anim-data (g/node-value atlas-node-id :anim-data)
             anim (get anim-data anim-id)]
         (is (and (= 64 (:width anim)) (= 64 (:height anim))))
         (testing "Modify image, anim data updated"
           (add-img workspace img-path 128 128)
           (is (= undo-count (count (undo-stack (g/node-id->graph-id project)))))
           (let [anim-data (g/node-value atlas-node-id :anim-data)
                 anim (get anim-data anim-id)]
             (is (and (= 128 (:width anim)) (= 128 (:height anim))))))
         (testing "Delete it, errors produced"
           (delete-file workspace img-path)
           (is (= undo-count (count (undo-stack (g/node-id->graph-id project)))))
                                        ; TODO - fix node pollution
           (is (g/error? (g/node-value atlas-node-id :anim-data)))))))))

(deftest save-no-reload
  (with-clean-system
    (let [[workspace project] (setup world)]
      (testing "Add internal file"
               (add-file workspace "/test.collection")
               (let [node (project/get-resource-node project "/test.collection")]
                 (g/transact
                   (g/set-property node :name "new_name"))
                 (is (has-undo? project))
                 (project/save-all project)
                 (workspace/fs-sync workspace)
                 (is (has-undo? project)))))))

(defn- error? [type v]
  (and (g/error? v) (= type (get-in v [:user-data :type]))))

(defn- no-error? [v]
  (not (g/error? v)))

(deftest external-file-errors
  (with-clean-system
    (let [[workspace project] (setup world)
          atlas-node-id (project/get-resource-node project "/atlas/single.atlas")
          img-path "/test_img.png"]
      (is (error? :file-not-found (g/node-value atlas-node-id :anim-data)))
      (add-img workspace img-path 64 64)
      (is (contains? (g/node-value atlas-node-id :anim-data) "test_img"))
      (delete-file workspace img-path)
      (is (error? :file-not-found (g/node-value atlas-node-id :anim-data)))
      (add-img workspace img-path 64 64)
      (is (contains? (g/node-value atlas-node-id :anim-data) "test_img"))
      (write-file workspace img-path "this is not png format")
      (is (error? :invalid-content (g/node-value atlas-node-id :anim-data))))))

(defn- dump-all-nodes
  [project]
  (let [nodes (g/node-value project :nodes-by-resource)]
    (prn (mapv str (keys nodes)))))

(deftest internal-file-errors
  (with-clean-system
    (let [[workspace project] (setup world)
          node-id (project/get-resource-node project "/main/main.go")
          img-path "/test_img.png"
          atlas-path "/atlas/single.atlas"]
      (is (error? :file-not-found (g/node-value node-id :scene)))
      (add-img workspace img-path 64 64)
      (is (no-error? (g/node-value node-id :scene)))
      (copy-file workspace atlas-path "/tmp.atlas")
      (delete-file workspace atlas-path)
      (is (error? :file-not-found (g/node-value node-id :scene)))
      (copy-file workspace "/tmp.atlas" atlas-path)
      (is (no-error? (g/node-value node-id :scene)))
      (write-file workspace atlas-path "test")
      (is (error? :invalid-content (g/node-value node-id :scene)))
      (copy-file workspace "/tmp.atlas" atlas-path)
      (is (no-error? (g/node-value node-id :scene))))))

(defn- image-label [atlas]
  (let [outline (g/node-value atlas :outline)]
    (:label (first (:children outline)))))

(deftest refactoring
  (with-clean-system
    (let [[workspace project] (setup world)
          node-id (project/get-resource-node project "/atlas/single.atlas")
          img-path "/test_img.png"
          new-img-path "/test_img2.png"]
      (add-img workspace img-path 64 64)
      (is (.endsWith (image-label node-id) img-path))
      (move-file workspace img-path new-img-path)
      (is (.endsWith (image-label node-id) new-img-path))
      (move-file workspace new-img-path img-path)
      (is (.endsWith (image-label node-id) img-path)))))
