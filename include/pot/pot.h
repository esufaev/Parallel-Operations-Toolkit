#pragma once

#include "pot/coroutines/task.h"
#include "pot/coroutines/async_condition_variable.h"
#include "pot/coroutines/when_all.h"
#include "pot/coroutines/resume_on.h"

#include "pot/algorithms/dot.h"
#include "pot/algorithms/reduce.h"
#include "pot/algorithms/parfor.h"
#include "pot/algorithms/parsections.h"

#include "pot/executors/executor.h"
#include "pot/executors/inline_executor.h"
#include "pot/executors/thread_executor.h"
#include "pot/executors/thread_pool_executor.h"

#include "pot/simd/simd_auto.h"
#include "pot/simd/simd_forced.h"

#include "pot/utils/cache_line.h"
#include "pot/utils/platform.h"
#include "pot/utils/time_it.h"
#include "pot/utils/unique_function.h"

// #include "pot/sync/async_lock.h"

using pot::simd::SIMDType::SSE;
using pot::simd::SIMDType::AVX;
using pot::simd::SIMDType::AVX512;