(ns integration.reload-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system undo-stack]]
            [editor.math :as math]
            [editor.project :as project]
            [editor.protobuf :as protobuf]
            [editor.atlas :as atlas]
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
     (testing "Add external file, no transaction"
       (add-img workspace img-path 64 64)
       (let [node (project/get-resource-node project img-path)]
         (is (nil? node)))
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
           (is (thrown? java.io.FileNotFoundException (g/node-value atlas-node-id :anim-data)))))))))

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
