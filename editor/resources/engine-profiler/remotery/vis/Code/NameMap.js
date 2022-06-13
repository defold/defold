class NameMap
{
    constructor(text_buffer)
    {
        this.names = { };
        this.textBuffer = text_buffer;
    }

    Get(name_hash)
    {
        // Return immediately if it's in the hash
        let name = this.names[name_hash];
        if (name != undefined)
        {
            return [ true, name ];
        }

        // Create a temporary name that uses the hash
        name = {
            string: name_hash.toString(),
            hash: name_hash
        };
        this.names[name_hash] = name;

        // Add to the text buffer the first time this name is encountered
        name.textEntry = this.textBuffer.AddText(name.string);

        return [ false, name ];
    }

    Set(name_hash, name_string)
    {
        // Create the name on-demand if its hash doesn't exist
        let name = this.names[name_hash];
        if (name == undefined)
        {
            name = {
                string: name_string,
                hash: name_hash
            };
            this.names[name_hash] = name;
        }
        else
        {
            name.string = name_string;
        }

        // Apply the updated text to the buffer
        name.textEntry = this.textBuffer.AddText(name_string);

        return name;
    }
}