
//
// Simple wrapper around DataView that auto-advances the read offset and provides
// a few common data type conversions specific to this app
//
DataViewReader = (function ()
{
    function DataViewReader(data_view, offset)
    {
        this.DataView = data_view;
        this.Offset = offset;
    }

    DataViewReader.prototype.AtEnd = function()
    {
        return this.Offset >= this.DataView.byteLength;
    }

    DataViewReader.prototype.GetBool = function ()
    {
        let v = this.DataView.getUint8(this.Offset);
        this.Offset++;
        return v;
    }

    DataViewReader.prototype.GetUInt8 = function ()
    {
        let v = this.DataView.getUint8(this.Offset);
        this.Offset++;
        return v;
    }

    DataViewReader.prototype.GetInt32 = function ()
    {
        let v = this.DataView.getInt32(this.Offset, true);
        this.Offset += 4;
        return v;
    }

    DataViewReader.prototype.GetUInt32 = function ()
    {
        var v = this.DataView.getUint32(this.Offset, true);
        this.Offset += 4;
        return v;
    }

    DataViewReader.prototype.GetFloat32 = function ()
    {
        const v = this.DataView.getFloat32(this.Offset, true);
        this.Offset += 4;
        return v;
    }

    DataViewReader.prototype.GetInt64 = function ()
    {
        var v = this.DataView.getFloat64(this.Offset, true);
        this.Offset += 8;
        return v;
    }

    DataViewReader.prototype.GetUInt64 = function ()
    {
        var v = this.DataView.getFloat64(this.Offset, true);
        this.Offset += 8;
        return v;
    }

    DataViewReader.prototype.GetFloat64 = function ()
    {
        var v = this.DataView.getFloat64(this.Offset, true);
        this.Offset += 8;
        return v;
    }

    DataViewReader.prototype.GetStringOfLength = function (string_length)
    {
        var string = "";
        for (var i = 0; i < string_length; i++)
        {
            string += String.fromCharCode(this.DataView.getInt8(this.Offset));
            this.Offset++;
        }

        return string;
    }

    DataViewReader.prototype.GetString = function ()
    {
        var string_length = this.GetUInt32();
        return this.GetStringOfLength(string_length);
    }

    return DataViewReader;
})();
