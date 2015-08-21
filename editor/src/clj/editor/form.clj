(ns editor.form
  (:require [dynamo.graph :as g]))

;;; sample form data
#_{
 :ctxt [node-id [property & path-within-property]]
 :sections
 [
  {
   :section "project"
   :help "..."
   :title "..."
   :fields
   [
    {
     :field "name"
     :help "..."
     :type String
     :default "<my name>"
     }
    ]
   }
  {
   :section "sound"
   :help "..."
   :title "..."
   :fields
   [
    {
     :field "f"
     :help "freq"
     :type g/Int
     :default 22050
     }
    ]
   }
  ]
 :values
 {
  "project" {"name" "my fps"}
  "sound" {"f" 44100}
  }
}


(defn set-value! [{:keys [user-data set]} path value]
  (set user-data path value))

(defn clear-value! [{:keys [user-data clear]} path]
  (clear user-data path))
