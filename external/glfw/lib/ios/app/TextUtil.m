// Copyright 2020-2023 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
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

#import "TextUtil.h"

@implementation IndexedPosition
@synthesize index = _index;

+ (IndexedPosition *)positionWithIndex:(NSUInteger)index {
    IndexedPosition *pos = [[IndexedPosition alloc] init];
    pos.index = index;
    return [pos autorelease];
}

@end

@implementation IndexedRange
@synthesize range = _range;

+ (IndexedRange *)rangeWithNSRange:(NSRange)nsrange {
    if (nsrange.location == NSNotFound)
        return nil;
    IndexedRange *range = [[IndexedRange alloc] init];
    range.range = nsrange;
    return [range autorelease];
}

- (UITextPosition *)start {
    return [IndexedPosition positionWithIndex:self.range.location];
}

- (UITextPosition *)end {
        return [IndexedPosition positionWithIndex:(self.range.location + self.range.length)];
}

-(BOOL)isEmpty {
    return (self.range.length == 0);
}
@end
