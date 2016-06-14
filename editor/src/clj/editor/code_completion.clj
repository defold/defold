(ns editor.code-completion
  (:require [clojure.string :as string]
            [editor.defold-project :as project]
            [editor.lua :as lua]
            [editor.lua-parser :as lua-parser]
            [editor.resource :as resource]
            [dynamo.graph :as g]))

(defn make-completions [resource-completion]
  (let [vars (map (fn [v] {:name v
                          :display-string v
                          :additional-info ""})
                  (:vars resource-completion))
        fns  (map (fn [[fname {:keys [params]}]]
                    {:name fname
                     :display-string (str fname "("
                                          (apply str (interpose "," (map #(str "[\"" % "\"]") params)))
                                          ")")
                     :additional-info ""})
                  (:functions resource-completion))]
    (vec (concat vars fns))))


(defn find-node-for-module [node-id module-name]
  (let [project-node (project/get-project node-id)
        resource-node-pairs (g/node-value project-node :nodes-by-resource-path)
        results (filter #(= (lua/lua-module->path module-name) (first %)) resource-node-pairs)]
    (when (= 1 (count results))
      (let [[_ result-node-id] (first results)]
        (when  (= "editor.script/ScriptNode" (-> result-node-id (g/node-by-id) (g/node-type) :name))
          result-node-id)))))


(defn gather-requires [node-id ralias rname]
  (let [rnode (find-node-for-module node-id rname)
        rcompletion-info (g/node-value rnode :completion-info)]
    {ralias (make-completions rcompletion-info)}))


(defn combine-completions [_node-id defold-docs completion-info]
 (let [base (merge defold-docs {"" (make-completions completion-info)})
       require-info (or (:requires completion-info) {})
       require-completions (apply merge (map (fn [[ralias rname]] (gather-requires _node-id ralias rname)) require-info))]
   (merge base require-completions)))
