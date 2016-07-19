var editor = null;

function init() {
  editor = CodeMirror(document.body, {
    value: '',
    lineNumbers: true,
    mode: "lua"
  });
  if (window.java) {
    editor.on("change", function(editor, change) {
      window.java.onchange();
    });
  }
}


window.onload = function() {
  // hack thanks to http://stackoverflow.com/a/28414332/1663009
  window.status = "MY-MAGIC-VALUE";
  window.status = "";

  init();
  
  if (window.java) {
    window.java.onload();
    //window.java.onchange();
  }
  // else {
  //   init();
  // }
};
