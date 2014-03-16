// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HeapTerminatedArrayBuilder_h
#define HeapTerminatedArrayBuilder_h

#include "heap/Heap.h"
#include "heap/HeapTerminatedArray.h"
#include "wtf/TerminatedArrayBuilder.h"

namespace WebCore {

template<typename T>
class HeapTerminatedArrayBuilder : public TerminatedArrayBuilder<T, HeapTerminatedArray> {
public:
    explicit HeapTerminatedArrayBuilder(HeapTerminatedArray<T>* array) : TerminatedArrayBuilder<T, HeapTerminatedArray>(array) { }
};

} // namespace WebCore

#endif // HeapTerminatedArrayBuilder_h
