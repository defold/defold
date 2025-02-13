(ns integration.unload-test
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.fs :as fs]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :as test-support]
            [util.coll :as coll]))

(defn- resource-type-has-view-type? [resource-type view-type]
  {:pre [(map? resource-type)
         (keyword? view-type)]}
  (boolean
    (coll/some
      #(= view-type (:id %))
      (:view-types resource-type))))

(deftest unloaded-resource-nodes-test
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")
        editable-directory-proj-path (str "/" (name :editable))
        non-editable-directory-proj-path (str "/" (name :non-editable))

        resource-type->proj-path
        (fn resource-type->proj-path [resource-type]
          (format "%s/unloaded.%s"
                  (if (:editable resource-type)
                    editable-directory-proj-path
                    non-editable-directory-proj-path)
                  (:ext resource-type)))]

    (with-open [_ (test-util/make-directory-deleter project-path)]
      (test-util/set-non-editable-directories! project-path [non-editable-directory-proj-path])

      (test-support/with-clean-system
        (let [workspace (test-util/setup-workspace! world project-path)]

          ;; Add dependencies to all sanctioned extensions to game.project.
          (test-util/set-libraries! workspace test-util/sanctioned-extension-urls)

          ;; With the extensions added, we can populate the workspace.
          (let [excluded-ext? #{resource/placeholder-resource-type-ext "project"}

                loadable-resource-type-colls-by-editability
                (test-util/distinct-resource-types-by-editability
                  workspace
                  (fn resource-type-predicate [resource-type]
                    (and (:load-fn resource-type)
                         (not (excluded-ext? (:ext resource-type))))))

                loadable-resource-types-by-proj-paths
                (coll/transfer loadable-resource-type-colls-by-editability (sorted-map)
                  (mapcat val)
                  (map (juxt resource-type->proj-path identity)))

                loadable-resource-proj-paths (keys loadable-resource-types-by-proj-paths)]

            ;; Create files for all resource types.
            (doseq [[proj-path resource-type] loadable-resource-types-by-proj-paths]
              (let [absolute-path (str project-path proj-path)
                    file (io/file absolute-path)
                    contents (workspace/template workspace resource-type)]
                (fs/create-file! file contents)))

            ;; Add a .defunload file to exclude these files from loading.
            (let [defunload-file (io/file (str project-path "/.defunload"))
                  defunload-contents (string/join "\n" loadable-resource-proj-paths)]
              (fs/create-file! defunload-file defunload-contents))

            ;; With the workspace populated, we can load the project.
            (workspace/resource-sync! workspace)

            (let [project (test-util/setup-project! workspace)]
              (test-util/clear-cached-save-data! project)
              (g/with-auto-evaluation-context evaluation-context
                (let [basis (:basis evaluation-context)]
                  (doseq [proj-path loadable-resource-proj-paths]
                    (let [resource (workspace/find-resource workspace proj-path evaluation-context)
                          resource-node (project/get-resource-node project resource evaluation-context)
                          resource-type (resource/resource-type resource)]
                      (testing proj-path
                        (is (not (resource/loaded? resource)))
                        (is (= resource (resource-node/resource basis resource-node)))
                        (is (not (g/connected? basis resource-node :save-data project :save-data)))
                        (is (not (g/error? (g/node-value resource-node :_properties evaluation-context))))
                        (if (:editor-openable resource-type)
                          (is (not (resource/openable? resource)))
                          (is (resource/openable? resource)))
                        (when (:allow-unloaded-use resource-type)
                          (is (not (g/error? (g/node-value resource-node :node-outline evaluation-context))))
                          (is (not (g/error? (g/node-value resource-node :build-targets evaluation-context))))
                          (when (resource-type-has-view-type? resource-type :scene)
                            (is (not (g/error? (g/node-value resource-node :scene evaluation-context))))))))))))))))))
