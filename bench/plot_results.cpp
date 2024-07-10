#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "plot_interface.h"

struct BenchmarkResult
{
    int num_threads;
    double avg_duration;
    std::string type;
};

void plot_results(const std::vector<BenchmarkResult>& results)
{
    bench::gnuplot_pipe gp(true);
    
    gp.send_line("set xlabel 'Time (ms)'");
    gp.send_line("set ylabel 'Threads'");
    gp.send_line("plot '-' title 'LQ' with linespoints, '-' title 'GQ' with linespoints");

    for (const auto& result : results)
    {
        if (result.type == "LQ")
            gp.send_line(std::to_string(result.avg_duration) + " " + std::to_string(result.num_threads), true);
    }
    gp.send_end_of_data();

    for (const auto& result : results)
    {
        if (result.type == "GQ")
            gp.send_line(std::to_string(result.avg_duration) + " " + std::to_string(result.num_threads), true);
    }
    gp.send_end_of_data();
}

int main()
{
    std::ifstream input_file("benchmark_results.dat", std::ios::binary);
    std::vector<BenchmarkResult> results;

    while (input_file.peek() != EOF)
    {
        BenchmarkResult result;
        input_file.read(reinterpret_cast<char*>(&result.num_threads), sizeof(result.num_threads));
        input_file.read(reinterpret_cast<char*>(&result.avg_duration), sizeof(result.avg_duration));

        std::getline(input_file, result.type, '\0');
        results.push_back(result);
    }

    input_file.close();

    plot_results(results);
    return 0;
}
