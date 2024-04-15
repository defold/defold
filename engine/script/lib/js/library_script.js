var LibraryScript = {
  $Script: {

  },

  dmScriptHttpRequestAsync: function(method, url, headers, arg, onload, onerror, onprogress, send_data, send_data_length, timeout) {
      var xhr = new XMLHttpRequest();

      function listener() {
          var resp_headers = xhr.getAllResponseHeaders();
          resp_headers = resp_headers.replace(new RegExp("\r", "g"), "");
          resp_headers += "\n";

          if (xhr.status != 0) {
              var ab = new Uint8Array(xhr.response);
              var b = _malloc(ab.length * ab.BYTES_PER_ELEMENT);
              HEAPU8.set(ab, b);
              var resp_headers_buffer = stringToNewUTF8(resp_headers);
              {{{ makeDynCall('viiiii', 'onload') }}}(arg, xhr.status, b, ab.length, resp_headers_buffer);
              _free(resp_headers_buffer);
              _free(b);
          } else {
              {{{ makeDynCall('vii', 'onerror') }}}(arg, xhr.status);
          }
      }
      xhr.onload = listener;
      xhr.onerror = listener;
      xhr.ontimeout = listener;
      xhr.onprogress = function(progress_event) {
        if (onprogress != 0) { // NULL passed as 0 into js
            {{{ makeDynCall('viii', 'onprogress') }}}(arg, progress_event.loaded, progress_event.total);
        }
      };
      xhr.open(UTF8ToString(method), UTF8ToString(url), true);
      // TODO: Doesn't work in node.js. Why? xhr2?
      xhr.responseType = 'arraybuffer';
      if (timeout > 0) {
          xhr.timeout = timeout / 1000.0;
      }

      var headersArray = UTF8ToString(headers).split("\n");
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
addToLibrary(LibraryScript);
