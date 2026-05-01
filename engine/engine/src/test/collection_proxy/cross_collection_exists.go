components {
  id: "script"
  component: "/collection_proxy/cross_collection_exists.script"
}
embedded_components {
  id: "target_proxy"
  type: "collectionproxy"
  data: "collection: \"/collection_proxy/cross_collection_target.collection\"\n"
}