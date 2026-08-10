mergeInto(LibraryManager.library, {
  list_system_fonts__async: true,
  list_system_fonts: async function () {
    if (!window.queryLocalFonts) {
      console.error("Local Font Access API not supported in this browser.");
      return;
    }

    try {
      const fonts = await window.queryLocalFonts();
      for (const f of fonts) {
        const namePtr = stringToNewUTF8(f.family || f.fullName || "");
        const stylePtr = stringToNewUTF8(f.style || "");
        const pathPtr = stringToNewUTF8("(unavailable in browser)");
        try {
          _on_font(namePtr, stylePtr, Number(f.weight || 0), pathPtr);
        } finally {
          _free(namePtr);
          _free(stylePtr);
          _free(pathPtr);
        }
      }
      _on_done();
    } catch (err) {
      console.error(err.name + ": " + err.message);
    }
  }
});
