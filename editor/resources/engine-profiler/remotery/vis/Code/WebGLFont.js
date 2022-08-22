
class glFont
{
    constructor(gl)
    {
        // Offscreen canvas for rendering individual characters
        this.charCanvas = document.createElement("canvas");
        this.charContext = this.charCanvas.getContext("2d");

        // Describe the font
        const font_size = 9;
        this.fontWidth = 5;
        this.fontHeight = 12;
        const font_face = "LocalFiraCode";
        const font_desc = font_size + "px " + font_face;

        // Ensure the CSS font is loaded before we do any work with it
        const self = this;
        document.fonts.load(font_desc).then(function (){

            // Create a canvas atlas for all characters in the font
            const atlas_canvas = document.createElement("canvas");
            const atlas_context = atlas_canvas.getContext("2d");
            atlas_canvas.width = 16 * self.fontWidth;
            atlas_canvas.height = 16 * self.fontHeight;

            // Add each character to the atlas
            const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-+=[]{};\'~#,./<>?!\"£$%%^&*()";
            for (let char of chars)
            {
                // Render this character to the canvas on its own
                self.RenderTextToCanvas(char, font_desc, self.fontWidth, self.fontHeight);

                // Calculate a location for it in the atlas using its ASCII code
                const ascii_code = char.charCodeAt(0);
                assert(ascii_code < 256);
                const y_index = Math.floor(ascii_code / 16);
                const x_index = ascii_code - y_index * 16
                assert(x_index < 16);
                assert(y_index < 16);

                // Copy into the atlas
                atlas_context.drawImage(self.charCanvas, x_index * self.fontWidth, y_index * self.fontHeight);
            }

            // Create the atlas texture and store it in the destination object
            self.atlasTexture = glCreateTexture(gl, atlas_canvas.width, atlas_canvas.height, atlas_canvas);
        });
    }

    RenderTextToCanvas(text, font, width, height)
    {
        // Resize canvas to match
        this.charCanvas.width = width;
        this.charCanvas.height = height;
    
        // Clear the background
        this.charContext.fillStyle = "black";
        this.charContext.clearRect(0, 0, width, height);

        // Render the text
        this.charContext.font = font;
        this.charContext.textAlign = "left";
        this.charContext.textBaseline = "top";
        this.charContext.fillText(text, 0, 1.5);
    }
}

class glTextBuffer
{
    constructor(gl, font)
    {
        this.font = font;
        this.textMap = {};
        this.textBuffer = new glDynamicBuffer(gl, gl.BYTE, 1, 8, glDynamicBufferType.Texture);
        this.textBufferPos = 0;
        this.textEncoder = new TextEncoder();
    }

    AddText(text)
    {
        // Return if it already exists
        const existing_entry = this.textMap[text];
        if (existing_entry != undefined)
        {
            return existing_entry;
        }

        // Add to the map
        // Note we're leaving an extra NULL character before every piece of text so that the shader can sample into it on text
        // boundaries and sample a zero colour for clamp.
        let entry = {
            offset: this.textBufferPos + 1,
            length: text.length,
        };
        this.textMap[text] = entry;

        // Ensure there's always enough space in the text buffer before adding
        this.textBuffer.ResizeToFitNextPow2(entry.offset + entry.length + 1);
        this.textBuffer.cpuArray.set(this.textEncoder.encode(text), entry.offset, entry.length);
        this.textBuffer.dirty = true;
        this.textBufferPos = entry.offset + entry.length;

        return entry;
    }

    UploadData()
    {
        this.textBuffer.UploadDirtyData();
    }

    SetAsUniform(gl, program, name, index)
    {
        glSetUniform(gl, program, name, this.textBuffer.texture, index);
		glSetUniform(gl, program, "inTextBufferDesc.fontWidth", this.font.fontWidth);
		glSetUniform(gl, program, "inTextBufferDesc.fontHeight", this.font.fontHeight);
		glSetUniform(gl, program, "inTextBufferDesc.textBufferLength", this.textBuffer.nbEntries);
    }
}