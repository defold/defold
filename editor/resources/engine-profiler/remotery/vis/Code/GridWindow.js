class SampleGridConfig
{
    constructor()
    {
        this.titleCellData =
        {
            Name: "Samples",
            Length: "Time (ms)",
            Self: "Self (ms)",
            Calls: "Calls",
            Recurse: "Recurse",
        };
        this.titleCellClasses =
        {
            Name: "SampleTitleNameCell",
            Length: "SampleTitleTimeCell",
            Self: "SampleTitleTimeCell",
            Calls: "SampleTitleCountCell",
            Recurse: "SampleTitleCountCell",
        };

        this.rowCellData =
        {
            Name: "",
            Length: "",
            Self: "",
            Calls: "",
            Recurse: "",
        };
        this.rowCellClasses =
        {
            Name: "SampleNameCell",
            Length: "SampleTimeCell",
            Self: "SampleTimeCell",
            Calls: "SampleCountCell",
            Recurse: "SampleCountCell",
        };
    }

    SetRowData(row, entry)
    {
        row.CellNodes["Length"].innerHTML = entry.ms_length;
        row.CellNodes["Self"].innerHTML = entry.ms_self;
        row.CellNodes["Calls"].innerHTML = entry.call_count;
        row.CellNodes["Recurse"].innerHTML = entry.recurse_depth;
    }
}

class PropertyGridConfig
{
    constructor()
    {
        this.titleCellData =
        {
            Name: "Property",
            Value: "Value",
            PrevValue: "Prev Value",
        };
        this.titleCellClasses =
        {
            Name: "SampleTitleNameCell",
            Value: "PropertyTitleValueCell",
            PrevValue: "PropertyTitleValueCell",
        };

        this.rowCellData =
        {
            Name: "",
            Value: "",
            PrevValue: "",
        };
        this.rowCellClasses =
        {
            Name: "SampleNameCell",
            Value: "PropertyValueCell",
            PrevValue: "PropertyValueCell",
        };
    }

    SetRowData(row, entry)
    {
        row.CellNodes["Value"].innerHTML = entry.value;
        row.CellNodes["PrevValue"].innerHTML = entry.prevValue;
    }
}

class GridWindow
{
    constructor(wm, name, offset, config)
    {
        // Digest for checking if grid needs to be repopulated
        this.nbEntries = 0;
        this.layoutDigest = null;

        // Source entry reference to reduce repopulation
        this.entries = null;

        // Window setup
        this.xPos = 10 + offset * 410;
        this.window = wm.AddWindow(name, 100, 100, 100, 100);
        this.window.ShowNoAnim();
        this.visible = true;

        // Create a grid that's indexed by the unique entry ID
        this.grid = this.window.AddControlNew(new WM.Grid());
        this.rootRow = this.grid.Rows.Add(config.titleCellData, "GridGroup", config.titleCellClasses);
        this.rootRow.Rows.AddIndex("_ID");

        this.config = config;
    }

    Close()
    {
        this.window.Close();
    }

    static AnimatedMove(self, top_window, bottom_window, val)
    {
        self.xPos = val;
        self.WindowResized(top_window, bottom_window);
    }

    SetXPos(xpos, top_window, bottom_window)
    {
        Anim.Animate(
            Bind(GridWindow.AnimatedMove, this, top_window, bottom_window),
            this.xPos, 10 + xpos * 410, 0.25);
    }

    SetVisible(visible)
    {
        if (visible != this.visible)
        {
            if (visible == true)
                this.window.ShowNoAnim();
            else
                this.window.HideNoAnim();

            this.visible = visible;
        }
    }

    WindowResized(top_window, bottom_window)
    {
        const top = top_window.Position[1] + top_window.Size[1] + 10;
        this.window.SetPosition(this.xPos, top_window.Position[1] + top_window.Size[1] + 10);
        this.window.SetSize(400, bottom_window.Position[1] - 10 - top);
    }

    GrowGrid(parent_row, nb_entries)
    {
        parent_row.Rows.Clear();

        for (let i = 0; i < nb_entries; i++)
        {
            const cell_data = Object.assign({}, this.config.rowCellData);
            cell_data._ID = i;
            
            parent_row.Rows.Add(cell_data, null, this.config.rowCellClasses);
        }
    }

    UpdateEntries(nb_entries, layout_digest, entries)
    {
        if (!this.visible)
            return;
        
        // If the source hasn't changed, don't repopulate
        if (this.entries == entries)
            return;
        this.entries = entries;

        // Recreate all the HTML if the number of entries gets bigger
        if (nb_entries > this.nbEntries)
        {
            this.GrowGrid(this.rootRow, nb_entries);
            this.nbEntries = nb_entries;
        }

        // If the content of the entries changes from previous update, update them all
        if (this.layoutDigest != layout_digest)
        {
            this.rootRow.Rows.ClearIndex("_ID");
            const index = this.UpdateAllEntryFields(this.rootRow, entries, 0, "");
            this.layoutDigest = layout_digest;

            // Clear out any left-over rows
            for (let i = index; i < this.rootRow.Rows.Rows.length; i++)
            {
                const row = this.rootRow.Rows.Rows[i];
                DOM.Node.Hide(row.Node);
            }
        }

        else if (this.visible)
        {
            // Otherwise just update the existing entry fields
            this.UpdateChangedEntryFields(this.rootRow, entries, "");
        }
    }

    UpdateAllEntryFields(parent_row, entries, index, indent)
    {
        for (let i in entries)
        {
            const entry = entries[i];

            // Match row allocation in GrowGrid
            const row = parent_row.Rows.Rows[index++];

            // Entry row may have been hidden previously
            DOM.Node.Show(row.Node);
            
            // Assign unique ID so that the common fast path of updating sample times only
            // can lookup target samples in the grid
            row.CellData._ID = entry.id;
            parent_row.Rows.AddRowToIndex("_ID", entry.id, row);

            // Record entry name for later comparison
            row.CellData.Name = entry.name.string;
            
            // Set entry name and colour
            const name_node = row.CellNodes["Name"];
            name_node.innerHTML = indent + entry.name.string;
            DOM.Node.SetColour(name_node, entry.colour);

            this.config.SetRowData(row, entry);

            index = this.UpdateAllEntryFields(parent_row, entry.children, index, indent + "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;");
        }

        return index;
    }

    UpdateChangedEntryFields(parent_row, entries, indent)
    {
        for (let i in entries)
        {
            const entry = entries[i];

            const row = parent_row.Rows.GetBy("_ID", entry.id);
            if (row)
            {
                this.config.SetRowData(row, entry);

                // Entry name will change when it switches from hash ID to network-retrieved 
                // name. Quickly check that before re-applying the HTML for the name.
                if (row.CellData.Name != entry.name.string)
                {
                    const name_node = row.CellNodes["Name"];
                    row.CellData.Name = entry.name.string;
                    name_node.innerHTML = indent + entry.name.string;
                }

                if (entry.colourChanged)
                {
                    const name_node = row.CellNodes["Name"];
                    DOM.Node.SetColour(name_node, entry.colour);
                }
            }

            this.UpdateChangedEntryFields(parent_row, entry.children, indent + "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;");
        }
    }
}