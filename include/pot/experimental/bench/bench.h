#include <stdio.h>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <map>

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

    class graph
    {
    public:
        graph() : gp(true) {}

        void add_point(const std::string &label, double x, double y)
        {
            if (datasets.find(label) == datasets.end())
            {
                datasets[label] = std::vector<std::pair<double, double>>();
            }
            datasets[label].push_back(std::make_pair(x, y));
        }

        void plot()
        {
            gp.send_line("set xlabel 'Time (ms)'");
            gp.send_line("set ylabel 'Value'");
            
            std::string plot_cmd = "plot ";
            for (auto it = datasets.begin(); it != datasets.end(); ++it)
            {
                if (it != datasets.begin())
                    plot_cmd += ", ";
                plot_cmd += "'-' title '" + it->first + "' with linespoints";
            }
            gp.send_line(plot_cmd);

            for (const auto &dataset : datasets)
            {
                for (const auto &point : dataset.second)
                {
                    gp.send_line(std::to_string(point.first) + " " + std::to_string(point.second), true);
                }
                gp.send_end_of_data();
            }
        }

    private:
        std::map<std::string, std::vector<std::pair<double, double>>> datasets;
        gnuplot_pipe gp;
    };
} // namespace bench

