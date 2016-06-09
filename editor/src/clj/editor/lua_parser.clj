(ns editor.lua-parser
  (:require [clj-antlr.core :as antlr]
            [clojure.java.io :as io]
            [clojure.zip :as zip]
            [clojure.edn :as edn]))

(def lua-parser (antlr/parser (slurp (io/resource "Lua.g4")) {:throw? false}))

(defn- parse-namelist [namelist]
  (vec (remove #(= "," %) (rest namelist))))

(defn- require-name [loc]
  (if (zip/end? loc)
    nil
    (let [node (zip/node loc)]
      (if (and (seq? node) (= :string (first node)))
        (edn/read-string (second node))
        (recur (zip/next loc))))))

(defn- has-require? [loc]
  (if (zip/end? loc)
    nil
    (let [node (zip/node loc)]
      (if (and (seq? node) (= :var (first node)) (= "require" (second node)))
        node
        (recur (zip/next loc))))))

(defn collect-require [parsed-names exprlist]
  (if exprlist
    (let [zexpr (zip/seq-zip exprlist)]
      (when (has-require? zexpr)
        {(first parsed-names) (require-name zexpr)}))
    {}))

(defn collect-vars [varlist]
  (remove nil? (mapv second (rest varlist))))

(defmulti xform-node (fn [k node] k))

(defmethod xform-node :default [k node]
  nil)

(defmethod xform-node :local [k node]
  (if (and (> (count node) 2) (string? (nth node 2)) (= "function") (nth node 2))
   (let [[_ _ _ funcname funcbody] node
         [_ _  [_  namelist]] funcbody]
     {:functions {funcname {:params (parse-namelist namelist)}}})
   (let [[_ _ namelist _ exprlist] node
         parsed-names (parse-namelist namelist)
         require-info (collect-require parsed-names exprlist)]
     {:vars parsed-names :requires require-info})))

(defmethod xform-node :varlist [k node]
  (let [[_ varlist _ exprlist] node
        parsed-names (collect-vars varlist)
        require-info (collect-require parsed-names exprlist)]
    {:vars parsed-names :requires require-info}))

(defmethod xform-node :function [k node]
  (let [[_ _ [_ funcname] funcbody] node
        [_ _  [_  namelist]] funcbody]
    {:functions {funcname {:params (parse-namelist namelist)}}}))

(defmethod xform-node :functioncall [k node]
  (let [zexpr (zip/seq-zip node)]
      (when (has-require? zexpr)
        (let [rname (require-name (zip/seq-zip node))]
          {:requires {rname rname}}))))

(defn collect-info [loc]
  (loop [loc loc
        result []]
   (if (zip/end? loc)
     result
     (let [node (zip/node loc)]
       (if (and (seq? node) (= :stat (first node)))
         (let [k (second node)]
           (if (seq? k)
             (recur (zip/next loc) (conj result (xform-node (first k) node)))
             (recur (zip/next loc) (conj result (xform-node (keyword k) node)))
             ))
         (recur (zip/next loc) result))))))


(defn lua-info [code]
  (let [info (-> code lua-parser zip/seq-zip collect-info)
        locals-info (into #{} (apply concat (map :vars info)))
        functions-info (or (apply merge (map :functions info)) {})
        requires-info (or (apply merge (map :requires info)) {})]
    {:vars locals-info
     :functions functions-info
     :requires requires-info}))


(comment

  (lua-info "local d \n local e")

  (lua-info  "foo=1")

  (lua-parser "foo=1")
  (:chunk (:block (:stat (:varlist (:var "foo")) "=" (:explist (:exp (:number "1"))))) "<EOF>")
  (lua-parser "local foo=1")
  (:chunk (:block (:stat "local" (:namelist "foo") "=" (:explist (:exp (:number "1"))))) "<EOF>")

  (lua-parser "foo,bar=1,2")
(:chunk (:block (:stat (:varlist (:var "foo") "," (:var "bar")) "=" (:explist (:exp (:number "1")) "," (:exp (:number "2"))))) "<EOF>")

  (lua-info "function oadd(num1, num2) return num1 end")
  (lua-parser "function oadd(num1, num2) return num1 end")
  (:chunk (:block (:stat "function" (:funcname "oadd") (:funcbody "(" (:parlist (:namelist "num1" "," "num2")) ")" (:block (:retstat "return" (:explist (:exp (:prefixexp (:varOrExp (:var "num1"))))))) "end"))) "<EOF>")

  (lua-info "local function oadd(num1, num2) return num1 end")
  (lua-parser "local function oadd(num1, num2) return num1 end")
  (:chunk (:block (:stat "local" "function" "oadd" (:funcbody "(" (:parlist (:namelist "num1" "," "num2")) ")" (:block (:retstat "return" (:explist (:exp (:prefixexp (:varOrExp (:var "num1"))))))) "end"))) "<EOF>")



  (collect-info  (zip/seq-zip (lua-parser "a,v=1,2")))
  (apply merge-with concat  [{:locals ["d"]} {:locals ["e"]}])


  [(:stat "local" (:namelist "d")) (:stat "local" (:namelist "e"))]
  (collect-info  (zip/seq-zip (lua-parser "local d,e ")))
  (collect-info  (zip/seq-zip (lua-parser "local mymathmodule = require(\"mymath\")")))


  (collect-info  (zip/seq-zip (lua-parser "function oadd(num1, num2) return num1 end")))

  (lua-parser "function oadd(num1, num2) return num1 end")
  (lua-info "function oadd(num1, num2) return num1 end")
  (:chunk (:block (:stat "function" (:funcname "oadd") (:funcbody "(" (:parlist (:namelist "num1" "," "num2")) ")" (:block (:retstat "return" (:explist (:exp (:prefixexp (:varOrExp (:var "num1"))))))) "end"))) "<EOF>")

  (lua-info "require(\"mymath\")")
  (lua-parser "require(\"mymath\")")
(:chunk (:block (:stat (:functioncall (:varOrExp (:var "require")) (:nameAndArgs (:args "(" (:explist (:exp (:string "\"mymath\""))) ")"))))) "<EOF>")

  (lua-parser "local mymathmodule = require(\"mymath\")")
  (lua-info "local mymathmodule = require(\"mymath\")")
  (:chunk (:block (:stat "local" (:namelist "mymathmodule") "=" (:explist (:exp (:prefixexp (:varOrExp (:var "require")) (:nameAndArgs (:args "(" (:explist (:exp (:string "\"mymath\""))) ")"))))))) "<EOF>")
  (:chunk (:block (:stat (:varlist (:var "mymathmodule")) "=" (:explist (:exp (:prefixexp (:varOrExp (:var "require")) (:nameAndArgs (:args "(" (:explist (:exp (:string "\"mymath\""))) ")"))))))) "<EOF>")





  )
