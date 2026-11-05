#include "msm_metrics.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char **argv)
{
    assert(argc == 2);
    msm_metrics::BusyRatio ratio;
    assert(ratio.sample(1000000000, 2000000000) == -1);
    assert(ratio.sample(1400000000, 2500000000) == 80);
    assert(ratio.sample(1400000000, 3000000000) == 0);
    assert(ratio.sample(2400000000, 3500000000) == 100);
    assert(ratio.sample(0, 4000000000) == -1);
    assert(ratio.sample(250000000, 4500000000) == 50);
    assert(ratio.sample(250000000, 4500000000) == -1);
    ratio.reset();
    assert(ratio.sample(350000000, 5000000000) == -1);

    namespace fs = std::filesystem;
    const fs::path root(argv[1]);
    fs::create_directories(root / "hwmon0");
    fs::create_directories(root / "hwmon1");
    fs::create_directories(root / "hwmon2");
    fs::create_directories(root / "hwmon3");
    std::ofstream(root / "hwmon0/name") << "gpuss_8_thermal\n";
    std::ofstream(root / "hwmon0/temp1_input") << "41000\n";
    std::ofstream(root / "hwmon1/name") << "gpuss_2_thermal\n";
    std::ofstream(root / "hwmon1/temp1_input") << "53000\n";
    std::ofstream(root / "hwmon1/temp2_input") << "57000\n";
    std::ofstream(root / "hwmon2/name") << "cpu_0_5_1_thermal\n";
    std::ofstream(root / "hwmon2/temp1_input") << "99000\n";
    std::ofstream(root / "hwmon3/name") << "gpuss_10_thermal\n";
    std::ofstream(root / "hwmon3/temp1_input") << "invalid\n";
    msm_metrics::Temperatures temperatures(root.string());
    assert(temperatures.size() == 4);
    assert(temperatures.maximum() == 57);
    std::ofstream(root / "hwmon0/temp1_input", std::ios::trunc) << "63000\n";
    assert(temperatures.maximum() == 63);
    std::ofstream(root / "hwmon0/temp1_input", std::ios::trunc) << "invalid\n";
    assert(temperatures.maximum() == 57);
    std::ofstream(root / "hwmon3/temp1_input", std::ios::trunc) << "67000\n";
    assert(temperatures.maximum() == 67);
    msm_metrics::Temperatures absent((root / "missing").string());
    assert(absent.size() == 0 && absent.maximum() == -1);
    std::cout << "Busy interval, idle, reset, invalid time, maximum temperature, sensor isolation and read recovery passed\n";
}
