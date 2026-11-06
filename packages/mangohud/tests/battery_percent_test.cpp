// Regression test for BatteryStats::getPercent
// Compile with: g++ -std=c++17 battery_percent_test.cpp battery.cpp -I<dir with filesystem.h and battery.h> -lspdlog -lfmt
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <filesystem.h>
#include "battery.h"

namespace fs = ghc::filesystem;

static int g_failures = 0;
static int g_checks = 0;

static void report(bool cond, const char* name, const char* detail) {
    g_checks++;
    if (!cond) {
        g_failures++;
        std::printf("FAIL: %s (%s)\n", name, detail);
    } else {
        std::printf("ok:   %s\n", name);
    }
}

static void mkdir_p(const std::string& p) {
    fs::create_directories(p);
}

static void write_file(const std::string& p, const std::string& content) {
    std::ofstream out(p);
    out << content;
}

static std::string make_battery(const std::string& root, const std::string& name) {
    std::string d = root + "/" + name;
    mkdir_p(d);
    write_file(d + "/type", "Battery\n");
    return d;
}

static bool near(float a, float b, float eps = 0.001f) {
    return std::fabs(a - b) < eps;
}

static void reset_state() {
    Battery_Stats.battPath[0] = "";
    Battery_Stats.battPath[1] = "";
    Battery_Stats.batt_count = 0;
    Battery_Stats.current_percent = 0.0f;
    Battery_Stats.current_watt = 0.0f;
    Battery_Stats.remaining_time = 0.0f;
    Battery_Stats.batt_check = true; // prevent numBattery from re-scanning /sys
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: %s <task_dir>\n", argv[0]);
        return 2;
    }
    std::string base = std::string(argv[1]) + "/battery_percent_fixture";
    // Clean fixture root.
    std::error_code ec;
    fs::remove_all(base, ec);
    mkdir_p(base);

    char tmpl[] = "/XXXXXX";
    std::string baseTemplate = base + tmpl;
    std::vector<char> buf(baseTemplate.begin(), baseTemplate.end());
    buf.push_back('\0');
    char* mk = mkdtemp(buf.data());
    if (!mk) {
        std::printf("FAIL: mkdtemp\n");
        return 2;
    }
    std::string root = std::string(mk);

    // ---- Case 1: single battery, capacity 99 takes precedence over charge/energy ratio (baseline bug: 100.5048)
    {
        reset_state();
        Battery_Stats.current_percent = 42.0f;
        std::string d = make_battery(root, "BAT_case1");
        write_file(d + "/charge_now", "8959000\n");
        write_file(d + "/charge_full", "8914000\n");
        write_file(d + "/energy_now", "9800000\n");
        write_file(d + "/energy_full", "10000000\n");
        write_file(d + "/capacity", "99\n");
        Battery_Stats.battPath[0] = d;
        Battery_Stats.batt_count = 1;
        float got = Battery_Stats.getPercent();
        char det[128];
        std::snprintf(det, sizeof(det), "got %.4f", got);
        report(near(got, 99.0f), "case1_capacity_precedence_99", det);
    }

    // ---- Case 2: capacity 0 boundary
    {
        reset_state();
        Battery_Stats.current_percent = 42.0f;
        std::string d = make_battery(root, "BAT_case2");
        write_file(d + "/charge_now", "5000000\n");
        write_file(d + "/charge_full", "10000000\n");
        write_file(d + "/capacity", "0\n");
        Battery_Stats.battPath[0] = d;
        Battery_Stats.batt_count = 1;
        float got = Battery_Stats.getPercent();
        char det[128];
        std::snprintf(det, sizeof(det), "got %.4f", got);
        report(near(got, 0.0f), "case2_capacity_0", det);
    }

    // ---- Case 3: capacity 100 boundary
    {
        reset_state();
        Battery_Stats.current_percent = 42.0f;
        std::string d = make_battery(root, "BAT_case3");
        write_file(d + "/charge_now", "5000000\n");
        write_file(d + "/charge_full", "10000000\n");
        write_file(d + "/capacity", "100\n");
        Battery_Stats.battPath[0] = d;
        Battery_Stats.batt_count = 1;
        float got = Battery_Stats.getPercent();
        char det[128];
        std::snprintf(det, sizeof(det), "got %.4f", got);
        report(near(got, 100.0f), "case3_capacity_100", det);
    }

    // ---- Case 4: capacity 50 vs energy ratio 70 -> capacity wins
    {
        reset_state();
        Battery_Stats.current_percent = 42.0f;
        std::string d = make_battery(root, "BAT_case4");
        write_file(d + "/energy_now", "7000000\n");
        write_file(d + "/energy_full", "10000000\n");
        write_file(d + "/capacity", "50\n");
        Battery_Stats.battPath[0] = d;
        Battery_Stats.batt_count = 1;
        float got = Battery_Stats.getPercent();
        char det[128];
        std::snprintf(det, sizeof(det), "got %.4f", got);
        report(near(got, 50.0f), "case4_capacity_over_energy", det);
    }

    // ---- Case 5: missing capacity -> fallback to charge ratio
    {
        reset_state();
        Battery_Stats.current_percent = 42.0f;
        std::string d = make_battery(root, "BAT_case5");
        write_file(d + "/charge_now", "2500000\n");
        write_file(d + "/charge_full", "10000000\n");
        Battery_Stats.battPath[0] = d;
        Battery_Stats.batt_count = 1;
        float got = Battery_Stats.getPercent();
        char det[128];
        std::snprintf(det, sizeof(det), "got %.4f", got);
        report(near(got, 25.0f, 0.01f), "case5_missing_capacity_fallback", det);
    }

    // ---- Case 6: malformed capacity -> fallback to energy ratio
    {
        reset_state();
        Battery_Stats.current_percent = 42.0f;
        std::string d = make_battery(root, "BAT_case6");
        write_file(d + "/energy_now", "4000000\n");
        write_file(d + "/energy_full", "10000000\n");
        write_file(d + "/capacity", "garbage\n");
        Battery_Stats.battPath[0] = d;
        Battery_Stats.batt_count = 1;
        float got = Battery_Stats.getPercent();
        char det[128];
        std::snprintf(det, sizeof(det), "got %.4f", got);
        report(near(got, 40.0f, 0.01f), "case6_malformed_capacity_fallback", det);
    }

    // ---- Case 7: ratio > 100 clamps to 100
    {
        reset_state();
        Battery_Stats.current_percent = 42.0f;
        std::string d = make_battery(root, "BAT_case7");
        write_file(d + "/charge_now", "15000000\n");
        write_file(d + "/charge_full", "10000000\n");
        Battery_Stats.battPath[0] = d;
        Battery_Stats.batt_count = 1;
        float got = Battery_Stats.getPercent();
        char det[128];
        std::snprintf(det, sizeof(det), "got %.4f", got);
        report(near(got, 100.0f, 0.01f), "case7_ratio_clamp_100", det);
    }

    // ---- Case 8: zero full denominator -> return last finite current_percent (42)
    {
        reset_state();
        Battery_Stats.current_percent = 42.0f;
        std::string d = make_battery(root, "BAT_case8");
        write_file(d + "/charge_now", "5000000\n");
        write_file(d + "/charge_full", "0\n");
        Battery_Stats.battPath[0] = d;
        Battery_Stats.batt_count = 1;
        float got = Battery_Stats.getPercent();
        char det[128];
        std::snprintf(det, sizeof(det), "got %.4f", got);
        report(near(got, 42.0f, 0.01f), "case8_zero_denominator_last_percent", det);
    }

    // ---- Case 9: no battery safe with initialized current_percent 0
    {
        reset_state();
        Battery_Stats.current_percent = 0.0f;
        Battery_Stats.batt_count = 0;
        float got = Battery_Stats.getPercent();
        char det[128];
        std::snprintf(det, sizeof(det), "got %.4f", got);
        report(near(got, 0.0f), "case9_no_battery_safe", det);
    }

    // ---- Case 10: two-charge-battery weighted aggregation preserved (1/2 + 3/6 = 50%)
    {
        reset_state();
        Battery_Stats.current_percent = 42.0f;
        std::string d1 = make_battery(root, "BAT_case10a");
        write_file(d1 + "/charge_now", "1000000\n");
        write_file(d1 + "/charge_full", "2000000\n");
        write_file(d1 + "/capacity", "80\n");
        std::string d2 = make_battery(root, "BAT_case10b");
        write_file(d2 + "/charge_now", "3000000\n");
        write_file(d2 + "/charge_full", "6000000\n");
        write_file(d2 + "/capacity", "20\n");
        Battery_Stats.battPath[0] = d1;
        Battery_Stats.battPath[1] = d2;
        Battery_Stats.batt_count = 2;
        float got = Battery_Stats.getPercent();
        char det[128];
        std::snprintf(det, sizeof(det), "got %.4f", got);
        report(near(got, 50.0f, 0.01f), "case10_two_battery_weighted", det);
    }

    fs::remove_all(root, ec);
    fs::remove_all(base, ec);
    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
