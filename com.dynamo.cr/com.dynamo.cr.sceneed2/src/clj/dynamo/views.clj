(ns dynamo.views
  (:require [dynamo.node :refer [defnode]]
            [dynamo.types :as t]))

(defmacro assure
  [conds & body]
  (loop [exp `(do ~@body)
         c   (reverse conds)]
    (if (seq c)
      (recur `(if ~(second c) ~exp ~(first c)) (drop 2 c))
      exp)))

(defmacro defview
  [nm & more]
  `(defnode ~nm ~@more))

(defn dispatch-message [type view & {:as body}]
  (t/process-one-event view (assoc body :type type)))

;; (on post-create :transactional
;;   (set! self :label (Text. parent))

;;; get selection injected into injectableview
;;; track upward from selection to project state
;;; dispatch a message on selection change too 

#_(defnode Printer (on :create (prn "CREATE!")) View (project-state [this] (user/current-project)))
#_(swt-safe (internal.ui.views/open-part (make-printer))
