#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include "pot/coroutines/task.h"
#include "pot/coroutines/when_all.h"
#include "pot/IO/io_uring.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

static const std::string TEST_DIR = "io_uring_test_dir";
static const std::string TEST_FILE = TEST_DIR + "/test_file.txt";

struct IoTestFixture
{
    IoTestFixture()
    {
        fs::create_directories(TEST_DIR);
    }

    ~IoTestFixture()
    {
        fs::remove_all(TEST_DIR);
    }
};

TEST_CASE("io_uring context creation", "[io_uring]")
{
    SECTION("default config")
    {
        pot::io::io_uring_context ctx;
        REQUIRE(true);
    }

    SECTION("custom queue depth")
    {
        pot::io::io_uring_config config;
        config.queue_depth = 256;
        pot::io::io_uring_context ctx(config);
        REQUIRE(true);
    }
}

TEST_CASE("io_uring open and close fd", "[io_uring]")
{
    IoTestFixture fixture;
    pot::io::io_uring_context ctx;

    SECTION("open new file with create")
    {
        auto task = ctx.open_fd(TEST_FILE, O_CREAT | O_WRONLY, 0644);
        ctx.submit();
        auto result = task.get();
        REQUIRE(result.is_open());
        REQUIRE(result.fd >= 0);

        auto ct = ctx.close_fd(result.fd);
        ctx.submit();
        ct.get();
    }

    SECTION("open non-existent file for read fails")
    {
        auto task = ctx.open_fd(TEST_DIR + "/no_such_file.txt", O_RDONLY);
        ctx.submit();
        auto result = task.get();
        REQUIRE_FALSE(result.is_open());
        REQUIRE(result.fd < 0);
    }

    SECTION("open and close roundtrip")
    {
        auto ot = ctx.open_fd(TEST_FILE, O_CREAT | O_RDWR, 0644);
        ctx.submit();
        auto open_res = ot.get();
        REQUIRE(open_res.is_open());

        auto ct = ctx.close_fd(open_res.fd);
        ctx.submit();
        auto close_res = ct.get();
        REQUIRE(close_res.ok());
    }
}

TEST_CASE("io_uring write and read", "[io_uring]")
{
    IoTestFixture fixture;
    pot::io::io_uring_context ctx;

    SECTION("write string and read back")
    {
        std::string payload = "hello io_uring";
        auto wt = ctx.write(TEST_FILE, payload);
        ctx.submit();
        auto wr = wt.get();
        REQUIRE(wr.ok());
        REQUIRE(wr.bytes_written() == payload.size());

        auto rt = ctx.read(TEST_FILE, payload.size());
        ctx.submit();
        auto rd = rt.get();
        REQUIRE(rd.ok());
        REQUIRE(rd.string() == payload);
    }

    SECTION("write bytes and read back")
    {
        std::vector<std::byte> data(64);
        for (size_t i = 0; i < data.size(); ++i)
            data[i] = static_cast<std::byte>(i & 0xFF);

        auto wt = ctx.write(TEST_FILE, std::span<const std::byte>(data));
        ctx.submit();
        auto wr = wt.get();
        REQUIRE(wr.ok());
        REQUIRE(wr.bytes_written() == 64);

        auto rt = ctx.read(TEST_FILE, 64);
        ctx.submit();
        auto rd = rt.get();
        REQUIRE(rd.ok());
        REQUIRE(rd.data.size() == 64);
        REQUIRE(rd.data == data);
    }

    SECTION("read with size=0 auto-detects file size")
    {
        std::string payload = "auto size detection";
        auto wt = ctx.write(TEST_FILE, payload);
        ctx.submit();
        wt.get();

        auto rt = ctx.read(TEST_FILE, 0);
        ctx.submit();
        auto rd = rt.get();
        REQUIRE(rd.ok());
        REQUIRE(rd.string() == payload);
    }
}

TEST_CASE("io_uring overwrite / truncate", "[io_uring]")
{
    IoTestFixture fixture;
    pot::io::io_uring_context ctx;

    SECTION("overwrite shorter content truncates")
    {
        auto wt1 = ctx.write(TEST_FILE, "short");
        ctx.submit();
        wt1.get();

        auto wt2 = ctx.write(TEST_FILE, "a very long string that is much longer than short");
        ctx.submit();
        auto wr = wt2.get();
        REQUIRE(wr.ok());

        auto rt = ctx.read(TEST_FILE);
        ctx.submit();
        auto rd = rt.get();
        REQUIRE(rd.ok());
        REQUIRE(rd.string() == "a very long string that is much longer than short");
    }

    SECTION("overwrite longer content")
    {
        auto wt1 = ctx.write(TEST_FILE, "initial");
        ctx.submit();
        wt1.get();

        auto wt2 = ctx.write(TEST_FILE, "replaced");
        ctx.submit();
        auto wr = wt2.get();
        REQUIRE(wr.ok());

        auto rt = ctx.read(TEST_FILE);
        ctx.submit();
        auto rd = rt.get();
        REQUIRE(rd.string() == "replaced");
    }
}

TEST_CASE("io_uring write_fd and read_fd", "[io_uring]")
{
    IoTestFixture fixture;
    pot::io::io_uring_context ctx;

    SECTION("raw fd write and read")
    {
        int fd = ::open(TEST_FILE.c_str(), O_CREAT | O_RDWR, 0644);
        REQUIRE(fd >= 0);

        std::string payload = "raw fd test";
        auto buf_span = std::as_bytes(std::span{payload});
        std::vector<std::byte> buf(buf_span.begin(), buf_span.end());

        auto wt = ctx.write_fd(fd, std::span<const std::byte>(buf));
        ctx.submit();
        auto wr = wt.get();
        REQUIRE(wr.ok());
        REQUIRE(wr.bytes_written() == buf.size());

        auto rt = ctx.read_fd(fd, buf.size(), 0);
        ctx.submit();
        auto rd = rt.get();
        REQUIRE(rd.ok());
        REQUIRE(rd.string() == payload);

        ::close(fd);
    }
}

TEST_CASE("io_uring append", "[io_uring]")
{
    IoTestFixture fixture;
    pot::io::io_uring_context ctx;

    SECTION("append writes at end")
    {
        auto wt1 = ctx.write(TEST_FILE, "line1\n");
        ctx.submit();
        wt1.get();

        auto wt2 = ctx.write(TEST_FILE, "line2\n", pot::io::open_flags::append);
        ctx.submit();
        wt2.get();

        auto rt = ctx.read(TEST_FILE);
        ctx.submit();
        auto rd = rt.get();
        REQUIRE(rd.ok());
        REQUIRE(rd.string() == "line1\nline2\n");
    }
}

TEST_CASE("io_uring stat", "[io_uring]")
{
    IoTestFixture fixture;
    pot::io::io_uring_context ctx;

    SECTION("stat reports correct size")
    {
        std::string payload = "stat me";
        auto wt = ctx.write(TEST_FILE, payload);
        ctx.submit();
        wt.get();

        auto stt = ctx.stat(TEST_FILE);
        ctx.submit();
        auto st = stt.get();
        REQUIRE(st.ok());
        REQUIRE(st.size() == payload.size());
    }

    SECTION("stat on non-existent file fails")
    {
        auto stt = ctx.stat(TEST_DIR + "/ghost.txt");
        ctx.submit();
        auto st = stt.get();
        REQUIRE_FALSE(st.ok());
    }
}

TEST_CASE("io_uring unlink", "[io_uring]")
{
    IoTestFixture fixture;
    pot::io::io_uring_context ctx;

    SECTION("unlink removes file")
    {
        auto wt = ctx.write(TEST_FILE, "delete me");
        ctx.submit();
        wt.get();

        auto ut = ctx.unlink(TEST_FILE);
        ctx.submit();
        auto un = ut.get();
        REQUIRE(un.ok());

        REQUIRE(::access(TEST_FILE.c_str(), F_OK) != 0);
    }
}

TEST_CASE("io_uring rename", "[io_uring]")
{
    IoTestFixture fixture;
    pot::io::io_uring_context ctx;

    SECTION("rename moves file")
    {
        std::string dst = TEST_DIR + "/renamed.txt";

        auto wt = ctx.write(TEST_FILE, "rename me");
        ctx.submit();
        auto wr = wt.get();
        REQUIRE(wr.ok());
        REQUIRE(wr.bytes_written() == 9);

        {
            struct stat st;
            REQUIRE(::stat(TEST_FILE.c_str(), &st) == 0);
            REQUIRE(st.st_size == 9);
        }

        auto rt = ctx.rename(TEST_FILE, dst);
        ctx.submit();
        auto rn = rt.get();
        REQUIRE(rn.ok());

        REQUIRE(::access(dst.c_str(), F_OK) == 0);
        {
            struct stat st;
            REQUIRE(::stat(dst.c_str(), &st) == 0);
            REQUIRE(st.st_size == 9);
        }

        auto rdt = ctx.read(dst);
        ctx.submit();
        auto rd = rdt.get();
        REQUIRE(rd.ok());
        REQUIRE(rd.string() == "rename me");

        REQUIRE(::access(TEST_FILE.c_str(), F_OK) != 0);

        fs::remove(dst);
    }
}

TEST_CASE("io_uring mkdir", "[io_uring]")
{
    IoTestFixture fixture;
    pot::io::io_uring_context ctx;

    SECTION("mkdir creates directory")
    {
        std::string subdir = TEST_DIR + "/subdir";
        auto mt = ctx.mkdir(subdir);
        ctx.submit();
        auto mr = mt.get();
        REQUIRE(mr.ok());
        REQUIRE(fs::is_directory(subdir));
    }
}

TEST_CASE("io_uring free functions", "[io_uring]")
{
    IoTestFixture fixture;
    pot::io::io_uring_context ctx;

    SECTION("write_file and read_file")
    {
        std::string payload = "free func test";
        auto wt = pot::io::write_file(ctx, TEST_FILE, payload);
        ctx.submit();
        auto wr = wt.get();
        REQUIRE(wr.ok());

        auto rt = pot::io::read_file(ctx, TEST_FILE, payload.size());
        ctx.submit();
        auto rd = rt.get();
        REQUIRE(rd.ok());
        REQUIRE(rd.string() == payload);
    }

    SECTION("stat_file")
    {
        auto wt = ctx.write(TEST_FILE, "stat me");
        ctx.submit();
        wt.get();

        auto stt = pot::io::stat_file(ctx, TEST_FILE);
        ctx.submit();
        auto st = stt.get();
        REQUIRE(st.ok());
        REQUIRE(st.size() == 7);
    }

    SECTION("delete_file")
    {
        auto wt = ctx.write(TEST_FILE, "bye");
        ctx.submit();
        wt.get();

        auto dt = pot::io::delete_file(ctx, TEST_FILE);
        ctx.submit();
        auto dr = dt.get();
        REQUIRE(dr.ok());
    }

    SECTION("rename_file")
    {
        std::string dst = TEST_DIR + "/renamed_free.txt";

        auto wt = ctx.write(TEST_FILE, "rename free");
        ctx.submit();
        wt.get();

        auto rt = pot::io::rename_file(ctx, TEST_FILE, dst);
        ctx.submit();
        auto rr = rt.get();
        REQUIRE(rr.ok());

        auto rdt = pot::io::read_file(ctx, dst);
        ctx.submit();
        auto rd = rdt.get();
        REQUIRE(rd.ok());
        REQUIRE(rd.string() == "rename free");

        fs::remove(dst);
    }
}

TEST_CASE("io_uring multiple operations", "[io_uring]")
{
    IoTestFixture fixture;
    pot::io::io_uring_context ctx;

    SECTION("write multiple files and read each")
    {
        for (int i = 0; i < 5; ++i)
        {
            auto path = TEST_DIR + "/file_" + std::to_string(i) + ".txt";
            auto content = "content_" + std::to_string(i);
            auto wt = ctx.write(path, content);
            ctx.submit();
            wt.get();
        }

        for (int i = 0; i < 5; ++i)
        {
            auto path = TEST_DIR + "/file_" + std::to_string(i) + ".txt";
            auto rt = ctx.read(path);
            ctx.submit();
            auto rd = rt.get();
            REQUIRE(rd.ok());
            REQUIRE(rd.string() == "content_" + std::to_string(i));
        }
    }

    SECTION("sequential writes and single read")
    {
        auto wt1 = ctx.write(TEST_FILE, "aaa", pot::io::open_flags::truncate | pot::io::open_flags::create);
        ctx.submit();
        wt1.get();

        auto wt2 = ctx.write(TEST_FILE, "bbbbb", pot::io::open_flags::truncate | pot::io::open_flags::create);
        ctx.submit();
        wt2.get();

        auto wt3 = ctx.write(TEST_FILE, "cc", pot::io::open_flags::truncate | pot::io::open_flags::create);
        ctx.submit();
        wt3.get();

        auto rt = ctx.read(TEST_FILE);
        ctx.submit();
        auto rd = rt.get();
        REQUIRE(rd.ok());
        REQUIRE(rd.string() == "cc");
    }
}

TEST_CASE("io_uring co_await async operations", "[io_uring][coroutine]")
{
    IoTestFixture fixture;
    pot::io::io_uring_context ctx;

    SECTION("single co_await write and read")
    {
        auto write_and_read = [&]() -> pot::coroutines::task<void>
        {
            std::string payload = "co_await test";

            auto wt = ctx.write(TEST_FILE, payload);
            ctx.submit();
            auto wr = co_await wt;
            REQUIRE(wr.ok());
            REQUIRE(wr.bytes_written() == payload.size());

            auto rt = ctx.read(TEST_FILE, payload.size());
            ctx.submit();
            auto rd = co_await rt;
            REQUIRE(rd.ok());
            REQUIRE(rd.string() == payload);
        };

        write_and_read().get();
    }

    SECTION("chained co_await operations")
    {
        auto chain = [&]() -> pot::coroutines::task<void>
        {
            auto w1 = ctx.write(TEST_FILE, "step1");
            ctx.submit();
            co_await w1;

            auto w2 = ctx.write(TEST_FILE, "step2");
            ctx.submit();
            co_await w2;

            auto w3 = ctx.write(TEST_FILE, "step3");
            ctx.submit();
            co_await w3;

            auto rt = ctx.read(TEST_FILE);
            ctx.submit();
            auto rd = co_await rt;
            REQUIRE(rd.ok());
            REQUIRE(rd.string() == "step3");
        };

        chain().get();
    }

    SECTION("co_await with open_fd, write_fd, read_fd, close_fd")
    {
        auto fd_ops = [&]() -> pot::coroutines::task<void>
        {
            auto ot = ctx.open_fd(TEST_FILE, O_CREAT | O_RDWR, 0644);
            ctx.submit();
            auto opened = co_await ot;
            REQUIRE(opened.is_open());

            std::string payload = "fd coroutine test";
            auto data = std::as_bytes(std::span{payload});
            std::vector<std::byte> buf(data.begin(), data.end());

            auto wt = ctx.write_fd(opened.fd, std::span<const std::byte>(buf));
            ctx.submit();
            auto wr = co_await wt;
            REQUIRE(wr.ok());

            auto rt = ctx.read_fd(opened.fd, buf.size(), 0);
            ctx.submit();
            auto rd = co_await rt;
            REQUIRE(rd.ok());
            REQUIRE(rd.string() == payload);

            auto ct = ctx.close_fd(opened.fd);
            ctx.submit();
            auto cl = co_await ct;
            REQUIRE(cl.ok());
        };

        fd_ops().get();
    }

    SECTION("co_await stat and unlink")
    {
        auto stat_unlink = [&]() -> pot::coroutines::task<void>
        {
            auto wt = ctx.write(TEST_FILE, "stat_me");
            ctx.submit();
            co_await wt;

            auto stt = ctx.stat(TEST_FILE);
            ctx.submit();
            auto st = co_await stt;
            REQUIRE(st.ok());
            REQUIRE(st.size() == 7);

            auto ut = ctx.unlink(TEST_FILE);
            ctx.submit();
            auto ul = co_await ut;
            REQUIRE(ul.ok());

            auto st2t = ctx.stat(TEST_FILE);
            ctx.submit();
            auto st2 = co_await st2t;
            REQUIRE_FALSE(st2.ok());
        };

        stat_unlink().get();
    }

    SECTION("co_await rename and read from new path")
    {
        auto move = [&]() -> pot::coroutines::task<void>
        {
            std::string dst = TEST_DIR + "/moved_co.txt";

            auto wt = ctx.write(TEST_FILE, "move_me_co");
            ctx.submit();
            co_await wt;

            auto rt = ctx.rename(TEST_FILE, dst);
            ctx.submit();
            auto rn = co_await rt;
            REQUIRE(rn.ok());

            auto rdt = ctx.read(dst);
            ctx.submit();
            auto rd = co_await rdt;
            REQUIRE(rd.ok());
            REQUIRE(rd.string() == "move_me_co");

            REQUIRE(::access(TEST_FILE.c_str(), F_OK) != 0);
            fs::remove(dst);
        };

        move().get();
    }
}

TEST_CASE("io_uring batch submit and co_await", "[io_uring][coroutine]")
{
    IoTestFixture fixture;
    pot::io::io_uring_context ctx;

    SECTION("create all tasks, submit once, await all")
    {
        auto batch = [&]() -> pot::coroutines::task<void>
        {
            auto t1 = ctx.write(TEST_DIR + "/b1.txt", "aaa");
            auto t2 = ctx.write(TEST_DIR + "/b2.txt", "bbb");
            auto t3 = ctx.write(TEST_DIR + "/b3.txt", "ccc");
            auto t4 = ctx.write(TEST_DIR + "/b4.txt", "ddd");
            auto t5 = ctx.write(TEST_DIR + "/b5.txt", "eee");

            ctx.submit();

            auto r1 = co_await t1;
            auto r2 = co_await t2;
            auto r3 = co_await t3;
            auto r4 = co_await t4;
            auto r5 = co_await t5;

            REQUIRE(r1.ok());
            REQUIRE(r2.ok());
            REQUIRE(r3.ok());
            REQUIRE(r4.ok());
            REQUIRE(r5.ok());
        };

        batch().get();

        auto rt1 = ctx.read(TEST_DIR + "/b1.txt");
        ctx.submit();
        REQUIRE(rt1.get().string() == "aaa");
        auto rt2 = ctx.read(TEST_DIR + "/b2.txt");
        ctx.submit();
        REQUIRE(rt2.get().string() == "bbb");
        auto rt3 = ctx.read(TEST_DIR + "/b3.txt");
        ctx.submit();
        REQUIRE(rt3.get().string() == "ccc");
        auto rt4 = ctx.read(TEST_DIR + "/b4.txt");
        ctx.submit();
        REQUIRE(rt4.get().string() == "ddd");
        auto rt5 = ctx.read(TEST_DIR + "/b5.txt");
        ctx.submit();
        REQUIRE(rt5.get().string() == "eee");
    }
}

TEST_CASE("io_uring when_all parallel writes", "[io_uring][coroutine]")
{
    IoTestFixture fixture;
    pot::io::io_uring_context ctx;

    SECTION("when_all with variadic io_tasks")
    {
        auto parallel = [&]() -> pot::coroutines::task<void>
        {
            auto t1 = ctx.write(TEST_DIR + "/p1.txt", "aaa");
            auto t2 = ctx.write(TEST_DIR + "/p2.txt", "bbb");
            auto t3 = ctx.write(TEST_DIR + "/p3.txt", "ccc");
            auto t4 = ctx.write(TEST_DIR + "/p4.txt", "ddd");
            auto t5 = ctx.write(TEST_DIR + "/p5.txt", "eee");

            ctx.submit();

            co_await pot::coroutines::when_all(t1, t2, t3, t4, t5);
        };

        parallel().get();

        {
            auto rt = ctx.read(TEST_DIR + "/p1.txt");
            ctx.submit();
            REQUIRE(rt.get().string() == "aaa");
        }
        {
            auto rt = ctx.read(TEST_DIR + "/p2.txt");
            ctx.submit();
            REQUIRE(rt.get().string() == "bbb");
        }
        {
            auto rt = ctx.read(TEST_DIR + "/p3.txt");
            ctx.submit();
            REQUIRE(rt.get().string() == "ccc");
        }
        {
            auto rt = ctx.read(TEST_DIR + "/p4.txt");
            ctx.submit();
            REQUIRE(rt.get().string() == "ddd");
        }
        {
            auto rt = ctx.read(TEST_DIR + "/p5.txt");
            ctx.submit();
            REQUIRE(rt.get().string() == "eee");
        }
    }

    SECTION("when_all with vector of io_tasks")
    {
        constexpr int N = 20;
        auto parallel_vec = [&]() -> pot::coroutines::task<void>
        {
            std::vector<pot::io::io_task<pot::io::write_result>> tasks;
            tasks.reserve(N);

            for (int i = 0; i < N; ++i)
            {
                auto path = TEST_DIR + "/bulk_" + std::to_string(i) + ".txt";
                auto content = "payload_" + std::to_string(i);
                tasks.push_back(ctx.write(path, content));
            }

            ctx.submit();

            co_await pot::coroutines::when_all(tasks);
        };

        parallel_vec().get();

        for (int i = 0; i < N; ++i)
        {
            auto path = TEST_DIR + "/bulk_" + std::to_string(i) + ".txt";
            auto rt = ctx.read(path);
            ctx.submit();
            auto rd = rt.get();
            REQUIRE(rd.ok());
            REQUIRE(rd.string() == "payload_" + std::to_string(i));
        }
    }

    SECTION("when_all mixed read and write")
    {
        for (int i = 0; i < 10; ++i)
        {
            auto wt = ctx.write(TEST_DIR + "/mix_" + std::to_string(i) + ".txt",
                                "data_" + std::to_string(i));
            ctx.submit();
            wt.get();
        }

        auto mixed = [&]() -> pot::coroutines::task<void>
        {
            std::vector<pot::io::io_task<pot::io::read_result>> reads;
            for (int i = 0; i < 10; ++i)
            {
                auto path = TEST_DIR + "/mix_" + std::to_string(i) + ".txt";
                reads.push_back(ctx.read(path));
            }

            ctx.submit();

            co_await pot::coroutines::when_all(reads);
        };

        mixed().get();

        for (int i = 0; i < 10; ++i)
        {
            auto path = TEST_DIR + "/mix_" + std::to_string(i) + ".txt";
            auto rt = ctx.read(path);
            ctx.submit();
            auto rd = rt.get();
            REQUIRE(rd.ok());
            REQUIRE(rd.string() == "data_" + std::to_string(i));
        }
    }

    SECTION("when_all write then read sequentially")
    {
        auto all_ops = [&]() -> pot::coroutines::task<void>
        {
            std::vector<pot::io::io_task<pot::io::write_result>> writes;
            for (int i = 0; i < 15; ++i)
            {
                auto path = TEST_DIR + "/co_" + std::to_string(i) + ".txt";
                writes.push_back(ctx.write(path, "val_" + std::to_string(i)));
            }

            ctx.submit();

            co_await pot::coroutines::when_all(writes);

            std::vector<pot::io::io_task<pot::io::read_result>> reads;
            for (int i = 0; i < 15; ++i)
            {
                auto path = TEST_DIR + "/co_" + std::to_string(i) + ".txt";
                reads.push_back(ctx.read(path));
            }

            ctx.submit();

            co_await pot::coroutines::when_all(reads);
        };

        all_ops().get();

        for (int i = 0; i < 15; ++i)
        {
            auto path = TEST_DIR + "/co_" + std::to_string(i) + ".txt";
            auto rt = ctx.read(path);
            ctx.submit();
            auto rd = rt.get();
            REQUIRE(rd.ok());
            REQUIRE(rd.string() == "val_" + std::to_string(i));
        }
    }

    SECTION("when_all stat on multiple files")
    {
        for (int i = 0; i < 8; ++i)
        {
            auto wt = ctx.write(TEST_DIR + "/stat_" + std::to_string(i) + ".txt",
                                std::string(i + 1, 'x'));
            ctx.submit();
            wt.get();
        }

        auto stat_all = [&]() -> pot::coroutines::task<void>
        {
            std::vector<pot::io::io_task<pot::io::statx_result>> stats;
            for (int i = 0; i < 8; ++i)
            {
                auto path = TEST_DIR + "/stat_" + std::to_string(i) + ".txt";
                stats.push_back(ctx.stat(path));
            }

            ctx.submit();

            co_await pot::coroutines::when_all(stats);
        };

        stat_all().get();
    }
}
