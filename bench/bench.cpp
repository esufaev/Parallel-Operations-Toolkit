#include "../include/pot/experimental/thread_pool/bench.h"

int main()
{
    bench::graph_tn graph1;
    graph1.add_point(2, 1);
    graph1.add_point(8, 92);
    graph1.add_point(67, 23);
    graph1.plot();

    bench::graph_tt graph2;
    graph2.add_point(3, 1);
    graph2.add_point(6, 2);
    graph2.add_point(9, 3);
    graph2.plot();

    std::cin.get();
    return 0;
}
