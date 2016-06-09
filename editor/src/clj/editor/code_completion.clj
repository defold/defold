(ns editor.code-completion
  (:require [clojure.string :as string]
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


(defn combine-completions [defold-docs resource-completions]
 (let [base (-> {:script [] :shader []}
                (assoc :script defold-docs))
       rcompletions (zipmap (map :namespace resource-completions)
                         (map make-completions resource-completions))]
       (update-in base [:script] merge rcompletions)))

(g/defnode CodeCompletionNode
  (input resource-completions g/Any :array)
  (output defold-docs g/Any :cached (g/fnk [] (lua/defold-documentation)))
  (output completions g/Any :cached (g/fnk [defold-docs resource-completions]
                                           (combine-completions defold-docs resource-completions))))
