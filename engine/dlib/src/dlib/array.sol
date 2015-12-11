module array

-- This functions copies POD data from source array into destination array
-- Returns number of elements stored in destination array.
!symbol("SolArrayCopy")
extern array_copy(dest:any, source:any):int
