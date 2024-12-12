#include "coroutine.h"

pot::tasks::task<int> foo()
{
    co_return 0;
}

int main()
{
    auto task = foo();

    return 0;
}