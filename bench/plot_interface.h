#include <stdio.h>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>

namespace bench
{
    class gnuplot_pipe
    {
    public:
        inline gnuplot_pipe(bool persist = true)
        {
            std::cout << "Opening gnuplot... ";
            pipe = popen(persist ? "gnuplot -persist" : "gnuplot", "w");
            if (!pipe)
                std::cout << "failed!" << std::endl;
            else
                std::cout << "succeeded." << std::endl;
        }
        inline virtual ~gnuplot_pipe()
        {
            if (pipe)
                pclose(pipe);
        }

        void send_line(const std::string &text, bool use_buffer = false)
        {
            if (!pipe)
                return;
            if (use_buffer)
                buffer.push_back(text + "\n");
            else
                fputs((text + "\n").c_str(), pipe);
        }
        void send_end_of_data(unsigned repeat_buffer = 1)
        {
            if (!pipe)
                return;
            for (unsigned i = 0; i < repeat_buffer; i++)
            {
                for (auto &line : buffer)
                    fputs(line.c_str(), pipe);
                fputs("e\n", pipe);
            }
            fflush(pipe);
            buffer.clear();
        }
        void send_new_data_block()
        {
            send_line("\n", !buffer.empty());
        }

        void write_buffer_to_file(const std::string &file_name)
        {
            std::ofstream file_out(file_name);
            for (auto &line : buffer)
                file_out << line;
            file_out.close();
        }

    private:
        gnuplot_pipe(gnuplot_pipe const &) = delete;
        void operator=(gnuplot_pipe const &) = delete;

        FILE *pipe;
        std::vector<std::string> buffer;
    };

    template <bool PlotType>
    class graph
    {
    public:
        graph() : gp(true) {}

        void add_point(double x, double y)
        {
            points.push_back(std::make_pair(x, y));
        }

        void plot()
        {
            if (PlotType)
            {
                gp.send_line("set xlabel 'Time (ms)'");
                gp.send_line("set ylabel 'Number of tasks'");
            }
            else
            {
                gp.send_line("set xlabel 'Threads'");
                gp.send_line("set ylabel 'Time (ms)'");
            }
            gp.send_line("plot '-' with linespoints");

            for (const auto &point : points)
            {
                gp.send_line(std::to_string(point.first) + " " + std::to_string(point.second), true);
            }

            gp.send_end_of_data();
        }

    private:
        std::vector<std::pair<double, double>> points;
        gnuplot_pipe gp;
    };

    using graph_threads = graph<false>;
    using graph_tasks = graph<true>;

} // namespace bench
