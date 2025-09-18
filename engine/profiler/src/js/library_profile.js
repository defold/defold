
/**
 *  Profiler implementation that uses Performance API in browser.
 */
var LibraryDmProfile = {
    $DefoldProfiler: {
        threads: new Map(),
        enablePerformanceTimelineTimeline: false,
        getThread: function(thread_id) {
            let thread = this.threads.get(thread_id);
            if(!thread) {
                const rootMark = { idx: 0, nameStr: "root" };
                const rootProperty = { idx: 0, nameStr: "root" };
                thread = { rootMark: rootMark, currentMark: rootMark, marks: new Map(), idxMark: 0,
                           rootProperty: rootProperty, properties: new Map(), idxProperty: 0 };
                this.threads.set(thread_id, thread);
            }
            return thread;
        }
    },
    dmProfileJSInit: function(enablePerformanceTimeline) {
        if(typeof performance === 'undefined')
            return false;
        DefoldProfiler.enablePerformanceTimeline = enablePerformanceTimeline;
        return true;
    },
    dmProfileJSReset: function(thread_id) {
        const thread = DefoldProfiler.getThread(thread_id);
        if(DefoldProfiler.enablePerformanceTimeline && performance.endMeasure) {
            for(let mark = thread.currentMark; mark && mark != thread.rootMark; mark = mark.parent)
                performance.endMeasure(mark.nameStr);
        }
        const resetProperties = (p) => {
            if(p.firstChild) {
                for(let c = p.firstChild; c; c = c.nextSibling)
                    resetProperties(c);
            } else if(p.flags) {
                p.value = undefined;
            }
        };
        for(let c = thread.rootProperty.firstChild; c; c = c.nextSibling)
            resetProperties(c);
        thread.currentMark = thread.rootMark;
        thread.rootMark.firstChild = thread.rootMark.lastChild = undefined;
        thread.marks.clear();
    },
    dmProfileJSEndFrame: function(thread_id) {
        const thread = DefoldProfiler.getThread(thread_id);
        let properties;
        if(thread.rootProperty.firstChild) {
            properties = {};
            const addToProperties = (p, v) => {
                if(p.firstChild) {
                    const g = {};
                    for(let c = p.firstChild; c; c = c.nextSibling)
                        addToProperties(c, g);
                    v[p.nameStr] = g;
                } else {
                    v[p.nameStr] = p.value;
                }
            };
            for(let c = thread.rootProperty.firstChild; c; c = c.nextSibling)
                addToProperties(c, properties);
        }
        if(DefoldProfiler.enablePerformanceTimeline)
            performance.mark("frame", { detail: properties });
    },
    dmProfileJSBeginMark: function(thread_id, name) {
        const thread = DefoldProfiler.getThread(thread_id);
        let mark;
        for(let m = thread.currentMark.firstChild; m; m = m.nextSibling) {
            if(m.name == name) {
                mark = m;
                break;
            }
        }
        if(mark) {
            ++mark.calls;
        } else {
            mark = { idx: ++thread.idxMark, nameStr: UTF8ToString(name), name, begins: [], measures: [], calls: 1, parent: thread.currentMark };
            if(mark.parent.firstChild)
                mark.parent.lastChild = mark.parent.lastChild.nextSibling = mark;
            else
                mark.parent.firstChild = mark.parent.lastChild = mark;
            thread.marks.set(mark.idx, mark);
        }
        if(DefoldProfiler.enablePerformanceTimeline) {
            if(performance.beginMeasure)
                performance.beginMeasure(mark.nameStr);
        } else {
            mark.begins.push(performance.now());
        }
        thread.currentMark = mark;
        return mark.idx;
    },
    dmProfileJSEndMark: function(thread_id) {
        const thread = DefoldProfiler.getThread(thread_id);
        const mark = thread.currentMark;
        if(!mark || mark == thread.rootMark)
            return true;
        if(DefoldProfiler.enablePerformanceTimeline) {
            if(performance.endMeasure) {
                performance.endMeasure(mark.nameStr);
            } else {
                mark.measures.push(performance.measure(mark.nameStr, { start: mark.begins.pop() }));
            }
        } else {
            const startTime = mark.begins.pop(), endTime = performance.now();
            mark.measures.push({ startTime, endTime, duration: endTime - startTime });
        }
        thread.currentMark = mark.parent;
        return thread.currentMark === thread.rootMark;
    },
    dmProfileJSFirstChild: function(thread_id, idx) {
        const thread = DefoldProfiler.getThread(thread_id);
        const mark = idx ? thread.marks.get(idx) : thread.rootMark;
        if(mark.firstChild)
            return mark.firstChild.idx;
        return -1;
    },
    dmProfileJSNextSibling: function(thread_id, idx) {
        const thread = DefoldProfiler.getThread(thread_id);
        const mark = thread.marks.get(idx);
        if(mark.nextSibling)
            return mark.nextSibling.idx;
        return -1;
    },
    dmProfileJSGetName: function(thread_id, idx) {
        const thread = DefoldProfiler.getThread(thread_id);
        const mark = thread.marks.get(idx);
        return mark.name;
    },
    dmProfileJSGetStart: function(thread_id, idx) {
        const thread = DefoldProfiler.getThread(thread_id);
        const mark = thread.marks.get(idx);
        if(mark.measures.length)
            return mark.measures[0].startTime;
        return 0;
    },
    dmProfileJSGetDuration: function(thread_id, idx) {
        const thread = DefoldProfiler.getThread(thread_id);
        const mark = thread.marks.get(idx);
        let duration = 0;
        for(let m of mark.measures)
            duration += m.duration;
        return duration;
    },
    dmProfileJSGetSelfDuration: function(thread_id, idx) {
        const thread = DefoldProfiler.getThread(thread_id);
        const mark = thread.marks.get(idx);
        let duration = 0;
        for(let m of mark.measures)
            duration += m.duration;
        for(let c = mark.firstChild; c; c = c.nextSibling) {
            for(let m of c.measures)
                duration -= m.duration;
        }
        return duration;
    },
    dmProfileJSGetCallCount: function(thread_id, idx) {
        const thread = DefoldProfiler.getThread(thread_id);
        const mark = thread.marks.get(idx);
        return mark.calls;
    },
    dmProfileJSFirstProperty: function(idx) {
        const thread = DefoldProfiler.getThread(0);
        let property = (idx ? thread.properties.get(idx) : thread.rootProperty).firstChild;
        while(property) { //find first with value
            if(1 || property.value !== undefined)
                return property.idx;
            property = property.nextSibling;
        }
        return -1;
    },
    dmProfileJSNextProperty: function(idx) {
        const thread = DefoldProfiler.getThread(0);
        let property = thread.properties.get(idx).nextSibling;
        while(property) { //find next with value
            if(1 || property.value !== undefined)
                return property.idx;
            property = property.nextSibling;
        }
        return -1;
    },
    dmProfileJSGePropertyName: function(idx) {
        const thread = DefoldProfiler.getThread(0);
        const property = thread.properties.get(idx);
        var b = _malloc(property.name.length);
        HEAPU8.set(property.name, b);
        return b;
    },
    dmProfileJSGetPropertyDescription: function(idx) {
        const thread = DefoldProfiler.getThread(0);
        const property = thread.properties.get(idx);
        return property.desc;
    },
    dmProfileJSGetPropertyName: function(idx) {
        const thread = DefoldProfiler.getThread(0);
        const property = thread.properties.get(idx);
        return property.name;
    },
    dmProfileJSGetPropertyType: function(idx) {
        const thread = DefoldProfiler.getThread(0);
        const property = thread.properties.get(idx);
        return property.type;
    },
    dmProfileJSGetPropertyFlags: function(idx) {
        const thread = DefoldProfiler.getThread(0);
        const property = thread.properties.get(idx);
        return property.flags;
    },
    dmProfileJSCreatePropertyGroup: function(type, name, desc, idx, parent_idx) {
        const thread = DefoldProfiler.getThread(0);
        const property = { idx: idx, name, desc, type, nameStr: UTF8ToString(name) };
        thread.properties.set(property.idx, property);
        const parent = parent_idx === -1 ? thread.rootProperty : thread.properties.get(parent_idx);
        if(parent.firstChild)
            parent.lastChild = parent.lastChild.nextSibling = property;
        else
            parent.firstChild = parent.lastChild = property;
        return property.idx;
    },
    dmProfileJSCreatePropertyBool: function(type, name, desc, v, flags, idx, parent_idx) {
        const thread = DefoldProfiler.getThread(0);
        const property = { idx: idx, name, desc, type, defaultValue: v, nameStr: UTF8ToString(name) };
        thread.properties.set(property.idx, property);

        const parent = parent_idx === -1 ? thread.rootProperty : thread.properties.get(parent_idx);
        if(parent.firstChild)
            parent.lastChild = parent.lastChild.nextSibling = property;
        else
            parent.firstChild = parent.lastChild = property;
        return property.idx;
    },
    dmProfileJSCreatePropertyS32: function(type, name, desc, v, flags, idx, parent_idx) {
        const thread = DefoldProfiler.getThread(0);
        const property = { idx: idx, name, desc, type, defaultValue: v, nameStr: UTF8ToString(name) };
        thread.properties.set(property.idx, property);
        const parent = parent_idx === -1 ? thread.rootProperty : thread.properties.get(parent_idx);
        if(parent.firstChild)
            parent.lastChild = parent.lastChild.nextSibling = property;
        else
            parent.firstChild = parent.lastChild = property;
        return property.idx;
    },
    dmProfileJSCreatePropertyU32: function(type, name, desc, v, flags, idx, parent_idx) {
        const thread = DefoldProfiler.getThread(0);
        const property = { idx: idx, name, desc, type, defaultValue: v, nameStr: UTF8ToString(name) };
        thread.properties.set(property.idx, property);
        const parent = parent_idx === -1 ? thread.rootProperty : thread.properties.get(parent_idx);
        if(parent.firstChild)
            parent.lastChild = parent.lastChild.nextSibling = property;
        else
            parent.firstChild = parent.lastChild = property;
        return property.idx;
    },
    dmProfileJSCreatePropertyF32: function(type, name, desc, v, flags, idx, parent_idx) {
        const thread = DefoldProfiler.getThread(0);
        const property = { idx: idx, name, desc, type, defaultValue: v, nameStr: UTF8ToString(name) };
        thread.properties.set(property.idx, property);
        const parent = parent_idx === -1 ? thread.rootProperty : thread.properties.get(parent_idx);
        if(parent.firstChild)
            parent.lastChild = parent.lastChild.nextSibling = property;
        else
            parent.firstChild = parent.lastChild = property;
        return property.idx;
    },
    dmProfileJSCreatePropertyS64: function(type, name, desc, v, flags, idx, parent_idx) {
        const thread = DefoldProfiler.getThread(0);
        const property = { idx: idx, name, desc, type, defaultValue: v, nameStr: UTF8ToString(name) };
        thread.properties.set(property.idx, property);
        const parent = parent_idx === -1 ? thread.rootProperty : thread.properties.get(parent_idx);
        if(parent.firstChild)
            parent.lastChild = parent.lastChild.nextSibling = property;
        else
            parent.firstChild = parent.lastChild = property;
        return property.idx;
    },
    dmProfileJSCreatePropertyU64: function(type, name, desc, v, flags, idx, parent_idx) {
        const thread = DefoldProfiler.getThread(0);
        const property = { idx: idx, name, desc, type, defaultValue: v, nameStr: UTF8ToString(name) };
        thread.properties.set(property.idx, property);
        const parent = parent_idx === -1 ? thread.rootProperty : thread.properties.get(parent_idx);
        if(parent.firstChild)
            parent.lastChild = parent.lastChild.nextSibling = property;
        else
            parent.firstChild = parent.lastChild = property;
        return property.idx;
    },
    dmProfileJSCreatePropertyF64: function(type, name, desc, v, flags, idx, parent_idx) {
        const thread = DefoldProfiler.getThread(0);
        const property = { idx: idx, name, desc, type, defaultValue: v, nameStr: UTF8ToString(name) };
        thread.properties.set(property.idx, property);
        const parent = parent_idx === -1 ? thread.rootProperty : thread.properties.get(parent_idx);
        if(parent.firstChild)
            parent.lastChild = parent.lastChild.nextSibling = property;
        else
            parent.firstChild = parent.lastChild = property;
        return property.idx;
    },
    dmProfileJSSetPropertyBool: function(idx, v) {
        const thread = DefoldProfiler.getThread(0);
        const property = thread.properties.get(idx);
        property.value = v;
    },
    dmProfileJSSetPropertyS32: function(idx, v) {
        const thread = DefoldProfiler.getThread(0);
        const property = thread.properties.get(idx);
        property.value = v;
    },
    dmProfileJSSetPropertyU32: function(idx, v) {
        const thread = DefoldProfiler.getThread(0);
        const property = thread.properties.get(idx);
        property.value = v;
    },
    dmProfileJSSetPropertyF32: function(idx, v) {
        const thread = DefoldProfiler.getThread(0);
        const property = thread.properties.get(idx);
        property.value = v;
    },
    dmProfileJSSetPropertyS64: function(idx, v) {
        const thread = DefoldProfiler.getThread(0);
        const property = thread.properties.get(idx);
        property.value = v;
    },
    dmProfileJSSetPropertyU64: function(idx, v) {
        const thread = DefoldProfiler.getThread(0);
        const property = thread.properties.get(idx);
        property.value = v;
    },
    dmProfileJSSetPropertyF64: function(idx, v) {
        const thread = DefoldProfiler.getThread(0);
        const property = thread.properties.get(idx);
        property.value = v;
    },
    dmProfileJSAddPropertyS32: function(idx, v) {
        const thread = DefoldProfiler.getThread(0);
        const property = thread.properties.get(idx);
        if(property.value === undefined)
            property.value = v;
        else
            property.value += v;
    },
    dmProfileJSAddPropertyU32: function(idx, v) {
        const thread = DefoldProfiler.getThread(0);
        const property = thread.properties.get(idx);
        if(property.value === undefined) {
            if(property.defaultValue)
                property.value = property.defaultValue + v;
            else
                property.value = v;
        } else {
            property.value += v;
        }
    },
    dmProfileJSAddPropertyF32: function(idx, v) {
        const thread = DefoldProfiler.getThread(0);
        const property = thread.properties.get(idx);
        if(property.value === undefined) {
            if(property.defaultValue)
                property.value = property.defaultValue + v;
            else
                property.value = v;
        } else {
            property.value += v;
        }
    },
    dmProfileJSAddPropertyS64: function(idx, v) {
        const thread = DefoldProfiler.getThread(0);
        const property = thread.properties.get(idx);
        if(property.value === undefined) {
            if(property.defaultValue)
                property.value = property.defaultValue + v;
            else
                property.value = v;
        } else {
            property.value += v;
        }
    },
    dmProfileJSAddPropertyU64: function(idx, v) {
        const thread = DefoldProfiler.getThread(0);
        const property = thread.properties.get(idx);
        if(property.value === undefined) {
            if(property.defaultValue)
                property.value = property.defaultValue + v;
            else
                property.value = v;
        } else {
            property.value += v;
        }
    },
    dmProfileJSAddPropertyF64: function(idx, v) {
        const thread = DefoldProfiler.getThread(0);
        const property = thread.properties.get(idx);
        if(property.value === undefined) {
            if(property.defaultValue)
                property.value = property.defaultValue + v;
            else
                property.value = v;
        } else {
            property.value += v;
        }
    },
    dmProfileJSResetProperty: function(idx) {
        const thread = DefoldProfiler.getThread(0);
        const property = thread.properties.get(idx);
        property.value = undefined;
    },
};
autoAddDeps(LibraryDmProfile, '$DefoldProfiler');
addToLibrary(LibraryDmProfile);
