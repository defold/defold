(ns editor.code-completion
  (:require [clojure.string :as string]
            [clojure.set :as set]
            [editor.code :as code]
            [editor.defold-project :as project]
            [editor.lua :as lua]
            [editor.lua-parser :as lua-parser]
            [editor.resource :as resource]
            [dynamo.graph :as g]))

(defn replace-alias [s alias]
  (string/replace s #".*\." (str alias ".")))

(defn make-completions [resource-completion include-locals? namespace-alias]
  (let [{:keys [vars local-vars functions local-functions]} resource-completion
        var-info (if include-locals? (set/union vars local-vars) vars)
        vars (map (fn [v]  (code/create-hint v))
                  var-info)
        fn-info (if include-locals? (merge functions local-functions) functions)
        fns  (map (fn [[fname {:keys [params]}]]
                    (let [n (if namespace-alias (replace-alias fname namespace-alias) fname)
                          display-string (str n "(" (string/join "," params)  ")")]
                      (code/create-hint n display-string display-string "" {:select params})))
                  fn-info)]
    (set (concat vars fns))))

(defn find-module-node-in-project [node-id module-name]
  (let [project-node (project/get-project node-id)
        resource-node-pairs (g/node-value project-node :nodes-by-resource-path)
        results (filter #(= (lua/lua-module->path module-name) (first %)) resource-node-pairs)]
    (when (= 1 (count results))
      (let [[_ result-node-id] (first results)]
        (when  (= "editor.script/ScriptNode" (-> result-node-id (g/node-by-id) (g/node-type) deref :name))
          result-node-id)))))

(defn resource-node-path [nid]
  (resource/proj-path (g/node-value nid :resource)))

(defn find-module-node [node-id module-name module-nodes]
  (when module-name
   (let [rpath (lua/lua-module->path module-name)
         module-node-id (some (fn [nid] (if (= rpath (resource-node-path nid)) nid)) module-nodes)]
     (if module-node-id
       module-node-id
       (when-let [nid (find-module-node-in-project node-id module-name)]
         (g/transact (g/connect nid :_node-id node-id :module-nodes))
         nid)))))

(defn gather-requires [node-id ralias rname module-nodes]
  (let [rnode (find-module-node node-id rname module-nodes)
        module-name (if (= ralias rname) (last (string/split rname #"\.")) ralias)
        rcompletion-info (g/node-value rnode :completion-info)]
    {module-name (make-completions rcompletion-info false module-name)
     "" #{(code/create-hint module-name)}}))


(defn combine-completions [_node-id completion-info module-nodes]
 (let [local-completions {"" (make-completions completion-info true nil)}
       require-info (or (:requires completion-info) {})
       require-completions (apply merge
                                  (map (fn [[ralias rname]] (gather-requires _node-id ralias rname module-nodes)) require-info))]
   (merge-with into
               @lua/defold-docs
               @lua/lua-std-libs-docs
               local-completions
               require-completions)))
