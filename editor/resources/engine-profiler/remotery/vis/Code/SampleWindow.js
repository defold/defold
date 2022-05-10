
SampleWindow = (function()
{
    function SampleWindow(wm, name, offset)
    {
        // Sample digest for checking if grid needs to be repopulated
        this.NbSamples = 0;
        this.SampleDigest = null;

        // Source sample reference to reduce repopulation
        this.Samples = null;

        this.XPos = 10 + offset * 410;
        this.Window = wm.AddWindow(name, 100, 100, 100, 100);
        this.Window.ShowNoAnim();
        this.Visible = true;

        // Create a grid that's indexed by the unique sample ID
        this.Grid = this.Window.AddControlNew(new WM.Grid());
        var cell_data =
        {
            Name: "Samples",
            Length: "Time (ms)",
            Self: "Self (ms)",
            Calls: "Calls",
            Recurse: "Recurse",
        };
        var cell_classes =
        {
            Name: "SampleTitleNameCell",
            Length: "SampleTitleTimeCell",
            Self: "SampleTitleTimeCell",
            Calls: "SampleTitleCountCell",
            Recurse: "SampleTitleCountCell",
        };
        this.RootRow = this.Grid.Rows.Add(cell_data, "GridGroup", cell_classes);
        this.RootRow.Rows.AddIndex("_ID");

        // Create a grid that's indexed by the unique sample ID
        this.GridStats = this.Window.AddControlNew(new WM.Grid());
        var stat_cell_data =
        {
            Name: "Statistics",
            Value: "Value",
        };
        var stat_cell_classes =
        {
            Name: "SampleTitleNameCell",
            Value: "SampleTitleNameCell",
        };
        this.RootRowStats = this.GridStats.Rows.Add(stat_cell_data, "GridGroupStats", stat_cell_classes);
        this.RootRowStats.Rows.AddIndex("_ID");
    }


    SampleWindow.prototype.Close = function()
    {
        this.Window.Close();
    }


    SampleWindow.prototype.SetXPos = function(xpos, top_window, bottom_window)
    {
        Anim.Animate(
            Bind(AnimatedMove, this, top_window, bottom_window),
            this.XPos, 10 + xpos * 410, 0.25);
    }


    function AnimatedMove(self, top_window, bottom_window, val)
    {
        self.XPos = val;
        self.WindowResized(top_window, bottom_window);
    }


    SampleWindow.prototype.SetVisible = function(visible)
    {
        if (visible != this.Visible)
        {
            if (visible == true)
                this.Window.ShowNoAnim();
            else
                this.Window.HideNoAnim();

            this.Visible = visible;
        }
    }


    SampleWindow.prototype.WindowResized = function(top_window, bottom_window)
    {
        var top = top_window.Position[1] + top_window.Size[1] + 10;
        this.Window.SetPosition(this.XPos, top_window.Position[1] + top_window.Size[1] + 10);

        var height = bottom_window.Position[1] - 10 - top;
        this.Window.SetSize(400, height);

        // TODO: Make it resize when the window resize handle is moved!
        DOM.Node.SetHeight(this.Grid.Node, height/2);
        DOM.Node.SetHeight(this.GridStats.Node, height/2);
    }

    SampleWindow.prototype.OnSamples = function(nb_samples, sample_digest, samples)
    {
        if (!this.Visible)
            return;
        
        // If the source hasn't changed, don't repopulate
        if (this.Samples == samples)
            return;
        this.Samples = samples;

        // Recreate all the HTML if the number of samples gets bigger
        if (nb_samples > this.NbSamples)
        {
            GrowGridStats(this.RootRowStats, nb_samples);
            GrowGrid(this.RootRow, nb_samples);
            this.NbSamples = nb_samples;
        }

        // If the content of the samples changes from previous update, update them all
        if (this.SampleDigest != sample_digest)
        {
            // *********************************************************************
            // Statistics
            this.RootRowStats.Rows.ClearIndex("_ID");
            var index = UpdateAllStatisticsFields(this.RootRowStats, samples, 0, "", "");

            // Clear out any left-over rows
            for (var i = index; i < this.RootRowStats.Rows.Rows.length; i++)
            {
                var row = this.RootRowStats.Rows.Rows[i];
                DOM.Node.Hide(row.Node);
            }
            // *********************************************************************
            // Samples
            this.RootRow.Rows.ClearIndex("_ID");
            var index = UpdateAllSampleFields(this.RootRow, samples, 0, "");

            // Clear out any left-over rows
            for (var i = index; i < this.RootRow.Rows.Rows.length; i++)
            {
                var row = this.RootRow.Rows.Rows[i];
                DOM.Node.Hide(row.Node);

            }
            this.SampleDigest = sample_digest;
        }

        else if (this.Visible)
        {
            // Otherwise just update the existing sample fields
            UpdateChangedStatisticsFields(this.RootRowStats, samples, "", "");
            UpdateChangedSampleFields(this.RootRow, samples, "");
        }
    }

    function GrowGridStats(parent_row, nb_statistics)
    {
        parent_row.Rows.Clear();

        for (var i = 0; i < nb_statistics; i++)
        {
            var cell_data =
            {
                _ID: i,
                Name: "",
                Value: "",
            };

            var cell_classes =
            {
                Name: "SampleNameCell",
                Value: "SampleNameCell",
            };

            parent_row.Rows.Add(cell_data, null, cell_classes);
        }
    }
    
    function GrowGrid(parent_row, nb_samples)
    {
        parent_row.Rows.Clear();

        for (var i = 0; i < nb_samples; i++)
        {
            var cell_data =
            {
                _ID: i,
                Name: "",
                Length: "",
                Self: "",
                Calls: "",
                Recurse: "",
            };

            var cell_classes =
            {
                Name: "SampleNameCell",
                Length: "SampleTimeCell",
                Self: "SampleTimeCell",
                Calls: "SampleCountCell",
                Recurse: "SampleCountCell",
            };

            parent_row.Rows.Add(cell_data, null, cell_classes);
        }
    }

    function UpdateAllStatisticsFields(parent_row, samples, index, indent, parent_name)
    {
        for (var i in samples)
        {
            var sample = samples[i];
            var name = sample.name.string;
            if (parent_name != "")
                name = parent_name + "." + name;

            if (sample.stat_value != null)
            {
                // Match row allocation in GrowGridStats
                var row = parent_row.Rows.Rows[index++];

                // Sample row may have been hidden previously
                DOM.Node.Show(row.Node);
                
                // Assign unique ID so that the common fast path of updating sample times only
                // can lookup target samples in the grid
                row.CellData._ID = sample.id;
                parent_row.Rows.AddRowToIndex("_ID", sample.id, row);

                // Record sample name for later comparison
                row.CellData.Name = sample.name.string;
                
                // Set sample name and colour
                var name_node = row.CellNodes["Name"];
                name_node.innerHTML = name;
                DOM.Node.SetColour(name_node, sample.colour);

                row.CellNodes["Value"].innerHTML = sample.stat_value + " " + sample.stat_desc.string;
            }

            index = UpdateAllStatisticsFields(parent_row, sample.children, index, indent + "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;", name);
        }

        return index;
    }

    function UpdateAllSampleFields(parent_row, samples, index, indent)
    {
        for (var i in samples)
        {
            var sample = samples[i];
            if (sample.stat_value == null)
            {
                // Match row allocation in GrowGridStats
                var row = parent_row.Rows.Rows[index++];

                // Sample row may have been hidden previously
                DOM.Node.Show(row.Node);
                
                // Assign unique ID so that the common fast path of updating sample times only
                // can lookup target samples in the grid
                row.CellData._ID = sample.id;
                parent_row.Rows.AddRowToIndex("_ID", sample.id, row);

                // Record sample name for later comparison
                row.CellData.Name = sample.name.string;
                
                // Set sample name and colour
                var name_node = row.CellNodes["Name"];
                name_node.innerHTML = indent + sample.name.string;
                DOM.Node.SetColour(name_node, sample.colour);

                row.CellNodes["Length"].innerHTML = sample.ms_length;
                row.CellNodes["Self"].innerHTML = sample.ms_self;
                row.CellNodes["Calls"].innerHTML = sample.call_count;
                row.CellNodes["Recurse"].innerHTML = sample.recurse_depth;
            }

            index = UpdateAllSampleFields(parent_row, sample.children, index, indent + "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;");
        }

        return index;
    }

    function UpdateChangedStatisticsFields(parent_row, samples, indent, parent_name)
    {
        for (var i in samples)
        {
            var sample = samples[i];
            var name = sample.name.string;
            if (parent_name != "")
                name = parent_name + "." + name;

            if (sample.stat_value != null)
            {
                var row = parent_row.Rows.GetBy("_ID", sample.id);
                if (row)
                {
                    row.CellNodes["Value"].innerHTML = sample.stat_value;

                    // Sample name will change when it switches from hash ID to network-retrieved 
                    // name. Quickly check that before re-applying the HTML for the name.
                    if (row.CellData.Name != name)
                    {
                        var name_node = row.CellNodes["Name"];
                        row.CellData.Name = name;
                        name_node.innerHTML = name;
                    }
                }
            }

            UpdateChangedStatisticsFields(parent_row, sample.children, indent + "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;", name);
        }
    }

    function UpdateChangedSampleFields(parent_row, samples, indent)
    {
        for (var i in samples)
        {
            var sample = samples[i];
            if (sample.stat_value != null)
            {
                continue; // Don't show statistics samples in this grid
            }

            var row = parent_row.Rows.GetBy("_ID", sample.id);
            if (row)
            {
                row.CellNodes["Length"].innerHTML = sample.ms_length;
                row.CellNodes["Self"].innerHTML = sample.ms_self;
                row.CellNodes["Calls"].innerHTML = sample.call_count;
                row.CellNodes["Recurse"].innerHTML = sample.recurse_depth;

                // Sample name will change when it switches from hash ID to network-retrieved 
                // name. Quickly check that before re-applying the HTML for the name.
                if (row.CellData.Name != sample.name.string)
                {
                    var name_node = row.CellNodes["Name"];
                    row.CellData.Name = sample.name.string;
                    name_node.innerHTML = indent + sample.name.string;
                }
            }

            UpdateChangedSampleFields(parent_row, sample.children, indent + "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;");
        }
    }


    return SampleWindow;
})();
