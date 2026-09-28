// Copyright 2026 The Defold Foundation
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

// Only the separately built `memory` binary installs this allocator. Each block
// retains its original domain, including across resize and cross-phase frees.
#[derive(Clone, Copy, Default, Debug)]
pub struct Counts {
    pub bytes: usize,
    pub blocks: usize,
    pub peak_bytes: usize,
    pub peak_blocks: usize,
    pub allocations: usize,
    pub reallocations: usize,
    pub frees: usize,
    pub allocated_bytes: usize,
}

pub const NONE: usize = 0;
pub const FIXTURE: usize = 1;
pub const BACKEND: usize = 2;

#[cfg(feature = "memory")]
mod tracked {
    use super::*;
    use std::alloc::{GlobalAlloc, Layout, System};
    use std::cell::Cell;
    use std::sync::atomic::{AtomicUsize, Ordering::Relaxed};

    // std::sync::Mutex may allocate on first use on macOS. Atomic counters
    // avoid allocator recursion; allocation domains are local to this thread.
    thread_local! { static DOMAIN: Cell<usize> = const { Cell::new(NONE) }; }

    struct AtomicCounts {
        bytes: AtomicUsize,
        blocks: AtomicUsize,
        peak_bytes: AtomicUsize,
        peak_blocks: AtomicUsize,
        allocations: AtomicUsize,
        reallocations: AtomicUsize,
        frees: AtomicUsize,
        allocated_bytes: AtomicUsize,
    }
    impl AtomicCounts {
        const fn new() -> Self {
            Self {
                bytes: AtomicUsize::new(0),
                blocks: AtomicUsize::new(0),
                peak_bytes: AtomicUsize::new(0),
                peak_blocks: AtomicUsize::new(0),
                allocations: AtomicUsize::new(0),
                reallocations: AtomicUsize::new(0),
                frees: AtomicUsize::new(0),
                allocated_bytes: AtomicUsize::new(0),
            }
        }
        fn read(&self) -> Counts {
            Counts {
                bytes: self.bytes.load(Relaxed),
                blocks: self.blocks.load(Relaxed),
                peak_bytes: self.peak_bytes.load(Relaxed),
                peak_blocks: self.peak_blocks.load(Relaxed),
                allocations: self.allocations.load(Relaxed),
                reallocations: self.reallocations.load(Relaxed),
                frees: self.frees.load(Relaxed),
                allocated_bytes: self.allocated_bytes.load(Relaxed),
            }
        }
    }
    static COUNTS: [AtomicCounts; 3] = [const { AtomicCounts::new() }; 3];

    #[repr(C)]
    struct Header {
        domain: usize,
    }

    fn extended(layout: Layout) -> (Layout, usize) {
        Layout::new::<Header>().extend(layout).unwrap()
    }

    pub struct Allocator;
    // SAFETY: payload alignment is preserved with Layout::extend. Allocations
    // and frees use the same extended layout; metadata access stays in the
    // header. Accounting uses atomics and never allocates itself.
    unsafe impl GlobalAlloc for Allocator {
        unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
            let (full, offset) = extended(layout);
            let base = unsafe { System.alloc(full) };
            if base.is_null() {
                return base;
            }
            let domain = DOMAIN.try_with(Cell::get).unwrap_or(NONE);
            unsafe {
                base.cast::<Header>().write(Header { domain });
            }
            if domain != NONE {
                let c = &COUNTS[domain];
                let bytes = c.bytes.fetch_add(layout.size(), Relaxed) + layout.size();
                let blocks = c.blocks.fetch_add(1, Relaxed) + 1;
                c.allocations.fetch_add(1, Relaxed);
                c.allocated_bytes.fetch_add(layout.size(), Relaxed);
                c.peak_bytes.fetch_max(bytes, Relaxed);
                c.peak_blocks.fetch_max(blocks, Relaxed);
            }
            unsafe { base.add(offset) }
        }

        unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
            let (full, offset) = extended(layout);
            let base = unsafe { ptr.sub(offset) };
            let domain = unsafe { (*base.cast::<Header>()).domain };
            if domain != NONE {
                let c = &COUNTS[domain];
                c.bytes.fetch_sub(layout.size(), Relaxed);
                c.blocks.fetch_sub(1, Relaxed);
                c.frees.fetch_add(1, Relaxed);
            }
            unsafe {
                System.dealloc(base, full);
            }
        }

        unsafe fn realloc(&self, ptr: *mut u8, layout: Layout, new_size: usize) -> *mut u8 {
            let (full, offset) = extended(layout);
            let base = unsafe { ptr.sub(offset) };
            let domain = unsafe { (*base.cast::<Header>()).domain };
            let (new_full, new_offset) =
                extended(Layout::from_size_align(new_size, layout.align()).unwrap());
            debug_assert_eq!(offset, new_offset);
            let resized = unsafe { System.realloc(base, full, new_full.size()) };
            if resized.is_null() {
                return resized;
            }
            if domain != NONE {
                let c = &COUNTS[domain];
                c.bytes.fetch_sub(layout.size(), Relaxed);
                let bytes = c.bytes.fetch_add(new_size, Relaxed) + new_size;
                c.reallocations.fetch_add(1, Relaxed);
                c.allocated_bytes.fetch_add(new_size, Relaxed);
                c.peak_bytes.fetch_max(bytes, Relaxed);
            }
            unsafe { resized.add(offset) }
        }
    }

    #[global_allocator]
    static ALLOCATOR: Allocator = Allocator;

    pub fn domain(domain: usize) {
        DOMAIN.with(|value| value.set(domain));
    }
    pub fn counts(domain: usize) -> Counts {
        COUNTS[domain].read()
    }
    pub fn begin_backend() {
        let c = &COUNTS[BACKEND];
        assert_eq!(c.blocks.load(Relaxed), 0);
        for field in [
            &c.bytes,
            &c.blocks,
            &c.peak_bytes,
            &c.peak_blocks,
            &c.allocations,
            &c.reallocations,
            &c.frees,
            &c.allocated_bytes,
        ] {
            field.store(0, Relaxed);
        }
        domain(BACKEND);
    }
    pub fn begin() -> Counts {
        domain(BACKEND);
        let c = &COUNTS[BACKEND];
        c.peak_bytes.store(c.bytes.load(Relaxed), Relaxed);
        c.peak_blocks.store(c.blocks.load(Relaxed), Relaxed);
        c.read()
    }
}

#[cfg(feature = "memory")]
pub use tracked::{begin, begin_backend, counts, domain};

#[cfg(not(feature = "memory"))]
#[inline]
pub fn domain(_: usize) {}
#[cfg(not(feature = "memory"))]
#[inline]
pub fn counts(_: usize) -> Counts {
    Counts::default()
}
#[cfg(not(feature = "memory"))]
#[inline]
pub fn begin_backend() {}
#[cfg(not(feature = "memory"))]
#[inline]
pub fn begin() -> Counts {
    Counts::default()
}

#[cfg(all(test, feature = "memory"))]
mod tests {
    use super::*;
    use std::alloc::{Layout, alloc, alloc_zeroed, dealloc, realloc};

    #[test]
    fn alignment_resize_and_original_domain() {
        begin_backend();
        let layout = Layout::from_size_align(32, 64).unwrap();
        unsafe {
            let ptr = alloc(layout);
            assert!(!ptr.is_null());
            assert_eq!(ptr as usize % 64, 0);
            ptr.write(73);
            domain(FIXTURE);
            let ptr = realloc(ptr, layout, 128);
            assert!(!ptr.is_null());
            assert_eq!(ptr as usize % 64, 0);
            assert_eq!(ptr.read(), 73);
            let c = counts(BACKEND);
            assert_eq!(
                (c.bytes, c.blocks, c.allocations, c.reallocations),
                (128, 1, 1, 1)
            );
            assert_eq!(c.allocated_bytes, 160);
            domain(NONE);
            dealloc(ptr, Layout::from_size_align(128, 64).unwrap());
            assert_eq!(counts(BACKEND).blocks, 0);
            begin();
            let zeroed = alloc_zeroed(layout);
            assert!((0..32).all(|i| *zeroed.add(i) == 0));
            dealloc(zeroed, layout);
        }
        domain(NONE);
        let c = counts(BACKEND);
        assert_eq!((c.bytes, c.blocks, c.frees), (0, 0, 2));
    }
}
