var LibraryScript = {
  $Script: {

  },

  dmScriptHttpRequestAsync: function(method, url, headers, arg, onload, onerror, send_data, send_data_length, timeout) {
      var xhr = new XMLHttpRequest();

      function listener() {
          var resp_headers = xhr.getAllResponseHeaders();
          resp_headers = resp_headers.replace(new RegExp("\r", "g"), "");
          resp_headers += "\n";

          if (xhr.status != 0) {
              // ALLOC_STACK - allocation in scope of current function (see preamble.js)
              // _malloc/_free pair could be implemented to the same effect, but the result is the same.
              var ab = new Uint8Array(xhr.response);
              var b = allocate(ab, 'i8', ALLOC_STACK);
              var resp_headers_buffer = allocate(intArrayFromString(resp_headers), 'i8', ALLOC_STACK);
              Runtime.dynCall('viiiii', onload, [arg, xhr.status, b, ab.length, resp_headers_buffer]);
          } else {
              Runtime.dynCall('vii', onerror, [arg, xhr.status]);
          }
      }
      xhr.onload = listener;
      xhr.onerror = listener;
      xhr.ontimeout = listener;
      xhr.open(Pointer_stringify(method), Pointer_stringify(url), true);
      // TODO: Doesn't work in node.js. Why? xhr2?
      xhr.responseType = 'arraybuffer';
      if (timeout > 0) {
          xhr.timeout = timeout / 1000.0;
      }

      var headersArray = Pointer_stringify(headers).split("\n");
      for (var i = 0; i < headersArray.length; i++) {
          if (headersArray[i].trim() != "") {
              var a = headersArray[i].split(":");
              xhr.setRequestHeader(a[0], a[1]);
          }
      }

      if (send_data_length > 0) {
          xhr.send(HEAPU8.subarray(send_data, send_data + send_data_length));
      } else {
          xhr.send();
      }
  }
}

autoAddDeps(LibraryScript, '$Script');
mergeInto(LibraryManager.library, LibraryScript);