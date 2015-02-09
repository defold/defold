var MemoryStats = {
    _originalMalloc : null,
    _originalFree : null,

    _totalMemoryAllocated : 0,
    _peakMemoryAllocated : 0,
    _numAllocations : 0,
    _numFrees : 0,
    _ptrToSize : {},

    _memorySummary : null,

    _checkBoxes : [ 'chk_heap', 'chk_dynamic', 'chk_static', 'chk_stack' ],

    SHOW_HEAP : 1 << 0,
    SHOW_DYNAMIC : 1 << 1,
    SHOW_STATIC : 1 << 2,
    SHOW_STACK : 1 << 3,

    _showFlags : 0,

    Initialise : function() {
        MemoryStats.InitialiseUI();
        MemoryStats.OverrideMalloc();
        MemoryStats.OverrideFree();
    },

    InitialiseUI : function() {
        this._memorySummary = document.getElementById('memory_summary');

        var onCheck = function(id, element) {
            return function(event) {
                MemoryStats.UpdateCheckVisibility(id, element.checked);
            }
        };
        for ( var i = 0; i < this._checkBoxes.length; ++i) {
            var id = this._checkBoxes[i];
            var chk = document.getElementById(id);
            chk.addEventListener('click', onCheck(id, chk));
            this.UpdateCheckVisibility(id, chk.checked);
        }
    },

    CheckIdToFlag : function(checkId) {
        var flag = 0;
        switch (checkId) {
        case 'chk_heap':
            flag = this.SHOW_HEAP;
            break;
        case 'chk_dynamic':
            flag = this.SHOW_DYNAMIC;
            break;
        case 'chk_static':
            flag = this.SHOW_STATIC;
            break;
        case 'chk_stack':
            flag = this.SHOW_STACK;
            break;
        }
        return flag;
    },

    UpdateCheckVisibility : function(checkId, checked) {
        var flag = this.CheckIdToFlag(checkId);
        if (checked) {
            this._showFlags |= flag;
        } else {
            this._showFlags &= ~flag;
        }
        this.UpdateUI();
    },

    UpdateUI : function() {
        function NumberOfSetBits(i) {
            i = i - ((i >> 1) & 0x55555555);
            i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
            return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
        }

        function ToHex(i, width) {
            var str = i.toString(16);
            while (str.length < width) {
                str = '0' + str;
            }
            return '0x' + str;
        }

        function TruncDec(f) {
            var str = f.toFixed(2);
            if (str.indexOf('.00', str.length - 3) !== -1) {
                return str.substr(0, str.length - 3);
            } else if (str.indexOf('0', str.length - 1) !== -1) {
                return str.substr(0, str.length - 1);
            } else {
                return str;
            }
        }

        function FormatBytes(bytes) {
            if (bytes >= 1000 * 1024 * 1024) {
                return TruncDec(bytes / (1024 * 1024 * 1024)) + ' GB';
            } else if (bytes >= 1000 * 1024) {
                return TruncDec(bytes / (1024 * 1024)) + ' MB';
            } else if (bytes >= 1000) {
                return TruncDec(bytes / 1024) + ' KB';
            } else {
                return TruncDec(bytes) + ' B';
            }
        }

        var width = (NumberOfSetBits(TOTAL_MEMORY) + 3) >> 2;
        var totalHeap = FormatBytes(TOTAL_MEMORY);

        var content = '';

        if (0 != (this._showFlags & this.SHOW_HEAP)) {
            content += 'HEAP size: ' + totalHeap;
        }

        if (0 != (this._showFlags & this.SHOW_DYNAMIC)) {
            if (0 < content.length) {
                content += "\n"
            }

            var dynUsed = FormatBytes(this._totalMemoryAllocated);
            var percent = (this._totalMemoryAllocated * 100.0 / (TOTAL_MEMORY - DYNAMIC_BASE))
                    .toFixed(2);
            content += 'DYN MEM USED: ' + dynUsed + ' (' + percent + '%)';

            var peakUsed = FormatBytes(this._peakMemoryAllocated);
            percet = (this._peakMemoryAllocated * 100.0 / (TOTAL_MEMORY - DYNAMIC_BASE))
                    .toFixed(2);
            content += '\nDYN PEAK USAGE: ' + peakUsed + ' (' + percent + '%)'
                    + '\nALLOCS: ' + this._numAllocations + '\nFREES: '
                    + this._numFrees;

            content += '\nDYNAMIC area size: '
                    + FormatBytes(DYNAMICTOP - DYNAMIC_BASE)
                    + '\nDYNAMIC_BASE: ' + ToHex(DYNAMIC_BASE, width)
                    + '\nDYNAMICTOP: ' + ToHex(DYNAMICTOP, width);
        }

        if (0 != (this._showFlags & this.SHOW_STATIC)) {
            if (0 < content.length) {
                content += "\n"
            }
            content += 'STATIC size: ' + FormatBytes(STATICTOP - STATIC_BASE)
                    + '\nSTATIC_BASE: ' + ToHex(STATIC_BASE, width)
                    + '\nSTATICTOP: ' + ToHex(STATICTOP, width);
        }

        if (0 != (this._showFlags & this.SHOW_STACK)) {
            if (0 < content.length) {
                content += "\n"
            }
            content += 'STACK size: ' + FormatBytes(STACK_MAX - STACK_BASE)
                    + '\nSTACK_BASE: ' + ToHex(STACK_BASE, width)
                    + '\nSTACKTOP: ' + ToHex(STACKTOP, width) + '\nSTACK_MAX: '
                    + ToHex(STACK_MAX, width) + '\nSTACK USED: '
                    + FormatBytes(STACKTOP - STACK_BASE);
        }

        this._memorySummary.value = content;
    },

    OverrideMalloc : function() {
        if (typeof _malloc !== 'undefined') {
            this._originalMalloc = _malloc;
            _malloc = this.SnoopMalloc;
            Module['_malloc'] = this.SnoopMalloc;
        } else {
            console.error("_malloc is undefined")
        }
    },

    OverrideFree : function() {
        if (typeof _free !== 'undefined') {
            this._originalFree = _free;
            _free = this.SnoopFree;
            Module['_free'] = this.SnoopFree;
        } else {
            console.error('_free is undefined');
        }
    },

    SnoopMalloc : function(size) {
        MemoryStats._totalMemoryAllocated += size;
        MemoryStats._peakMemoryAllocated = Math.max(
                MemoryStats._peakMemoryAllocated,
                MemoryStats._totalMemoryAllocated);
        ++MemoryStats._numAllocations;
        var ptr = MemoryStats._originalMalloc(size);
        MemoryStats._ptrToSize[ptr] = size;
        MemoryStats.UpdateUI();
        return ptr;
    },

    SnoopFree : function(ptr) {
        var size = MemoryStats._ptrToSize[ptr];
        MemoryStats._totalMemoryAllocated -= size;
        delete MemoryStats._ptrToSize[ptr]++
        MemoryStats._numFrees;
        var ret = MemoryStats._originalFree(ptr);
        MemoryStats.UpdateUI();
        return ret;
    }
}
